#include <cstdio>
#include <map>
#include <random>
#include <string>

#include "storage/leaf_page.hpp"
#include "storage/pager.hpp"
#include "temp_db_file.hpp"
#include "test_framework.hpp"

using namespace db::storage;

namespace {

Page empty_leaf() {
    Page page;
    LeafPage::format(page);
    return page;
}

std::string numbered_key(int i) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "key%05d", i);
    return buffer;
}

} // namespace

TEST_CASE(formatted_leaf_starts_empty) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    REQUIRE(page.type() == PageType::Leaf);
    REQUIRE(leaf.entry_count() == 0);
    REQUIRE(leaf.next_leaf() == kInvalidPageId);
    REQUIRE(!leaf.find("anything").has_value());
}

TEST_CASE(rejects_a_page_that_is_not_a_leaf) {
    Page page;
    REQUIRE_THROWS(LeafPage{page});
}

TEST_CASE(insert_then_find_returns_value) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    REQUIRE(leaf.insert("name", "Alice"));
    REQUIRE(leaf.entry_count() == 1);
    REQUIRE(leaf.find("name") == std::optional<std::string>("Alice"));
    REQUIRE(!leaf.find("nope").has_value());
}

TEST_CASE(entries_are_kept_sorted_regardless_of_insert_order) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    leaf.insert("charlie", "3");
    leaf.insert("alice", "1");
    leaf.insert("bob", "2");

    REQUIRE(leaf.entry_count() == 3);
    REQUIRE(leaf.key_at(0) == "alice");
    REQUIRE(leaf.key_at(1) == "bob");
    REQUIRE(leaf.key_at(2) == "charlie");
    REQUIRE(leaf.value_at(0) == "1");
    REQUIRE(leaf.value_at(2) == "3");
}

TEST_CASE(insert_replaces_the_value_of_an_existing_key) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    leaf.insert("k", "short");
    REQUIRE(leaf.insert("k", "a much longer value than before"));
    REQUIRE(leaf.entry_count() == 1);
    REQUIRE(leaf.find("k") == std::optional<std::string>("a much longer value than before"));

    REQUIRE(leaf.insert("k", "tiny"));
    REQUIRE(leaf.entry_count() == 1);
    REQUIRE(leaf.find("k") == std::optional<std::string>("tiny"));
}

TEST_CASE(remove_deletes_only_the_named_key) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    leaf.insert("a", "1");
    leaf.insert("b", "2");
    leaf.insert("c", "3");

    REQUIRE(leaf.remove("b"));
    REQUIRE(leaf.entry_count() == 2);
    REQUIRE(!leaf.find("b").has_value());
    REQUIRE(leaf.find("a") == std::optional<std::string>("1"));
    REQUIRE(leaf.find("c") == std::optional<std::string>("3"));
    REQUIRE(leaf.key_at(0) == "a");
    REQUIRE(leaf.key_at(1) == "c");

    REQUIRE(!leaf.remove("b"));
}

TEST_CASE(empty_values_round_trip) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    REQUIRE(leaf.insert("k", ""));
    REQUIRE(leaf.find("k") == std::optional<std::string>(""));
}

TEST_CASE(a_full_page_reports_overflow_and_keeps_its_data) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    const std::string value(100, 'v');

    int inserted = 0;
    while (leaf.insert(numbered_key(inserted), value)) {
        ++inserted;
    }
    REQUIRE(inserted > 0);
    REQUIRE(leaf.entry_count() == inserted);

    // The rejected insert must not have disturbed anything.
    REQUIRE(!leaf.insert(numbered_key(inserted), value));
    REQUIRE(leaf.entry_count() == inserted);
    for (int i = 0; i < inserted; ++i) {
        REQUIRE(leaf.find(numbered_key(i)) == std::optional<std::string>(value));
    }
}

TEST_CASE(space_from_removed_entries_is_reused) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    const std::string value(100, 'v');

    int inserted = 0;
    while (leaf.insert(numbered_key(inserted), value)) {
        ++inserted;
    }
    for (int i = 0; i < 5; ++i) {
        REQUIRE(leaf.remove(numbered_key(i)));
    }
    for (int i = 0; i < 5; ++i) {
        REQUIRE(leaf.insert(numbered_key(i), value));
    }
    REQUIRE(leaf.entry_count() == inserted);
}

TEST_CASE(repeated_updates_do_not_exhaust_the_page) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    for (int i = 0; i < 2000; ++i) {
        const std::string value(200, static_cast<char>('a' + i % 26));
        REQUIRE(leaf.insert("hot_key", value));
        REQUIRE(leaf.find("hot_key") == std::optional<std::string>(value));
    }
    REQUIRE(leaf.entry_count() == 1);
}

TEST_CASE(record_too_large_for_any_leaf_is_rejected) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    REQUIRE_THROWS(leaf.insert("k", std::string(kPageSize, 'x')));
}

TEST_CASE(largest_possible_record_fits) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    const std::string value(kMaxLeafRecordSize - kLeafRecordHeaderSize - 1, 'x');
    REQUIRE(leaf.insert("k", value));
    REQUIRE(leaf.find("k") == std::optional<std::string>(value));
}

TEST_CASE(record_that_exactly_fills_the_remaining_space_fits) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    REQUIRE(leaf.insert("a", "first"));

    const std::size_t remaining = leaf.free_space();
    const std::string value(remaining - kLeafRecordHeaderSize - 1 - kLeafSlotSize, 'x');
    REQUIRE(leaf.insert("z", value));

    REQUIRE(leaf.free_space() == 0);
    REQUIRE(leaf.entry_count() == 2);
    REQUIRE(leaf.find("a") == std::optional<std::string>("first"));
    REQUIRE(leaf.find("z") == std::optional<std::string>(value));
}

TEST_CASE(sibling_pointer_round_trips) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    leaf.set_next_leaf(42);
    REQUIRE(leaf.next_leaf() == 42);
}

TEST_CASE(leaf_survives_a_pager_round_trip) {
    TempDbFile file("leaf_round_trip");
    PageId id = 0;
    {
        Pager pager(file.path());
        id = pager.allocate_page();

        Page page = empty_leaf();
        LeafPage leaf(page);
        for (int i = 0; i < 20; ++i) {
            leaf.insert(numbered_key(i), "value" + std::to_string(i));
        }
        leaf.set_next_leaf(7);
        pager.write_page(id, page);
        pager.sync();
    }

    Pager reopened(file.path());
    Page page = reopened.read_page(id);
    LeafPage leaf(page);
    REQUIRE(leaf.entry_count() == 20);
    REQUIRE(leaf.next_leaf() == 7);
    for (int i = 0; i < 20; ++i) {
        REQUIRE(leaf.find(numbered_key(i)) == std::optional<std::string>("value" + std::to_string(i)));
    }
}

TEST_CASE(matches_a_reference_map_under_random_operations) {
    Page page = empty_leaf();
    LeafPage leaf(page);
    std::map<std::string, std::string> reference;

    std::mt19937 rng(12345);
    for (int step = 0; step < 4000; ++step) {
        const std::string key = numbered_key(static_cast<int>(rng() % 200));
        if (rng() % 100 < 70) {
            const std::string value(1 + rng() % 40, static_cast<char>('a' + rng() % 26));
            if (leaf.insert(key, value)) {
                reference[key] = value;
            }
        } else if (leaf.remove(key)) {
            reference.erase(key);
        }
    }

    REQUIRE(leaf.entry_count() == reference.size());
    std::uint16_t index = 0;
    for (const auto& [key, value] : reference) {
        REQUIRE(leaf.key_at(index) == key);
        REQUIRE(leaf.value_at(index) == value);
        REQUIRE(leaf.find(key) == std::optional<std::string>(value));
        ++index;
    }
}
