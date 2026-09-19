#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "storage/btree.hpp"
#include "storage/internal_page.hpp"
#include "storage/leaf_page.hpp"
#include "temp_db_file.hpp"
#include "test_framework.hpp"

using namespace db::storage;

namespace {

std::string numbered_key(int i) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "key%06d", i);
    return buffer;
}

// Walks the leaf chain from the leftmost leaf, returning every key in order.
std::vector<std::string> keys_along_leaf_chain(Pager& pager, const BTree& tree) {
    PageId page_id = tree.root_page_id();
    while (true) {
        Page page = pager.read_page(page_id);
        if (page.type() == PageType::Leaf) {
            break;
        }
        page_id = InternalPage(page).child_at(0);
    }

    std::vector<std::string> keys;
    while (page_id != kInvalidPageId) {
        Page page = pager.read_page(page_id);
        LeafPage leaf(page);
        for (std::uint16_t slot = 0; slot < leaf.entry_count(); ++slot) {
            keys.push_back(leaf.key_at(slot));
        }
        page_id = leaf.next_leaf();
    }
    return keys;
}

} // namespace

TEST_CASE(an_empty_tree_is_a_single_leaf) {
    TempDbFile file("btree_empty");
    Pager pager(file.path());
    BTree tree(pager);
    REQUIRE(tree.height() == 1);
    REQUIRE(!tree.get("missing").has_value());
}

TEST_CASE(put_then_get_returns_the_value) {
    TempDbFile file("btree_put_get");
    Pager pager(file.path());
    BTree tree(pager);
    tree.put("name", "Alice");
    REQUIRE(tree.get("name") == std::optional<std::string>("Alice"));
    REQUIRE(!tree.get("other").has_value());
}

TEST_CASE(put_replaces_an_existing_value) {
    TempDbFile file("btree_replace");
    Pager pager(file.path());
    BTree tree(pager);
    tree.put("k", "first");
    tree.put("k", "second");
    REQUIRE(tree.get("k") == std::optional<std::string>("second"));
}

TEST_CASE(tree_grows_past_one_page_and_keeps_every_key) {
    TempDbFile file("btree_grows");
    Pager pager(file.path());
    BTree tree(pager);

    const int count = 5000;
    for (int i = 0; i < count; ++i) {
        tree.put(numbered_key(i), "value" + std::to_string(i));
    }

    // Small records fit ~240 leaves under one root, so this stays two levels deep.
    REQUIRE(tree.height() == 2);
    for (int i = 0; i < count; ++i) {
        REQUIRE(tree.get(numbered_key(i)) == std::optional<std::string>("value" + std::to_string(i)));
    }
    REQUIRE(!tree.get(numbered_key(count)).has_value());
    REQUIRE(!tree.get("zzzz").has_value());
}

// Larger values mean more leaves than one internal node can hold, which forces an internal split
// and a new root above internal nodes.
TEST_CASE(larger_records_push_the_tree_to_three_levels) {
    TempDbFile file("btree_deep");
    Pager pager(file.path());
    BTree tree(pager);

    const std::string value(400, 'v');
    const int count = 3000;
    for (int i = 0; i < count; ++i) {
        tree.put(numbered_key(i), value);
    }

    REQUIRE(tree.height() >= 3);
    for (int i = 0; i < count; ++i) {
        REQUIRE(tree.get(numbered_key(i)) == std::optional<std::string>(value));
    }

    const std::vector<std::string> keys = keys_along_leaf_chain(pager, tree);
    REQUIRE(keys.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        REQUIRE(keys[static_cast<std::size_t>(i)] == numbered_key(i));
    }
}

TEST_CASE(descending_insert_order_also_builds_a_valid_tree) {
    TempDbFile file("btree_descending");
    Pager pager(file.path());
    BTree tree(pager);

    const int count = 2000;
    for (int i = count; i-- > 0;) {
        tree.put(numbered_key(i), "v");
    }

    REQUIRE(tree.height() >= 2);
    for (int i = 0; i < count; ++i) {
        REQUIRE(tree.get(numbered_key(i)) == std::optional<std::string>("v"));
    }
}

TEST_CASE(the_leaf_chain_holds_every_key_in_order) {
    TempDbFile file("btree_chain");
    Pager pager(file.path());
    BTree tree(pager);

    const int count = 3000;
    for (int i = 0; i < count; ++i) {
        tree.put(numbered_key(i), "v");
    }

    const std::vector<std::string> keys = keys_along_leaf_chain(pager, tree);
    REQUIRE(keys.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        REQUIRE(keys[static_cast<std::size_t>(i)] == numbered_key(i));
    }
}

TEST_CASE(tree_reopens_from_the_metadata_page) {
    TempDbFile file("btree_reopen");
    const int count = 2000;
    {
        Pager pager(file.path());
        BTree tree(pager);
        for (int i = 0; i < count; ++i) {
            tree.put(numbered_key(i), "value" + std::to_string(i));
        }
        REQUIRE(tree.height() >= 2);
        pager.sync();
    }

    Pager reopened(file.path());
    BTree tree(reopened);
    REQUIRE(tree.height() >= 2);
    for (int i = 0; i < count; ++i) {
        REQUIRE(tree.get(numbered_key(i)) == std::optional<std::string>("value" + std::to_string(i)));
    }
}

TEST_CASE(matches_a_reference_map_under_random_inserts) {
    TempDbFile file("btree_random");
    Pager pager(file.path());
    BTree tree(pager);
    std::map<std::string, std::string> reference;

    std::mt19937 rng(20260919);
    for (int step = 0; step < 4000; ++step) {
        const std::string key = numbered_key(static_cast<int>(rng() % 1500));
        const std::string value(1 + rng() % 60, static_cast<char>('a' + rng() % 26));
        tree.put(key, value);
        reference[key] = value;
    }

    for (const auto& [key, value] : reference) {
        REQUIRE(tree.get(key) == std::optional<std::string>(value));
    }

    const std::vector<std::string> keys = keys_along_leaf_chain(pager, tree);
    REQUIRE(keys.size() == reference.size());
    std::size_t index = 0;
    for (const auto& [key, value] : reference) {
        (void)value;
        REQUIRE(keys[index] == key);
        ++index;
    }
}
