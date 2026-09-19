#include <cstdio>
#include <string>

#include "storage/internal_page.hpp"
#include "storage/pager.hpp"
#include "temp_db_file.hpp"
#include "test_framework.hpp"

using namespace db::storage;

namespace {

Page empty_internal(PageId first_child) {
    Page page;
    InternalPage::format(page, first_child);
    return page;
}

std::string numbered_key(int i) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "key%05d", i);
    return buffer;
}

} // namespace

TEST_CASE(formatted_internal_node_has_only_its_first_child) {
    Page page = empty_internal(100);
    InternalPage node(page);
    REQUIRE(page.type() == PageType::Internal);
    REQUIRE(node.key_count() == 0);
    REQUIRE(node.child_at(0) == 100);
    REQUIRE(node.find_child("anything") == 100);
}

TEST_CASE(rejects_a_page_that_is_not_an_internal_node) {
    Page page;
    REQUIRE_THROWS(InternalPage{page});
}

TEST_CASE(separators_are_kept_sorted_with_their_children) {
    Page page = empty_internal(10);
    InternalPage node(page);
    REQUIRE(node.insert_separator("m", 30));
    REQUIRE(node.insert_separator("d", 20));
    REQUIRE(node.insert_separator("t", 40));

    REQUIRE(node.key_count() == 3);
    REQUIRE(node.key_at(0) == "d");
    REQUIRE(node.key_at(1) == "m");
    REQUIRE(node.key_at(2) == "t");
    REQUIRE(node.child_at(0) == 10);
    REQUIRE(node.child_at(1) == 20);
    REQUIRE(node.child_at(2) == 30);
    REQUIRE(node.child_at(3) == 40);
}

TEST_CASE(find_child_routes_keys_to_the_right_subtree) {
    Page page = empty_internal(10);
    InternalPage node(page);
    node.insert_separator("d", 20);
    node.insert_separator("m", 30);

    REQUIRE(node.find_child("a") == 10);  // before the first separator
    REQUIRE(node.find_child("d") == 20);  // equal to a separator goes right
    REQUIRE(node.find_child("f") == 20);
    REQUIRE(node.find_child("m") == 30);
    REQUIRE(node.find_child("zz") == 30); // after the last separator
}

TEST_CASE(duplicate_separator_is_rejected) {
    Page page = empty_internal(10);
    InternalPage node(page);
    node.insert_separator("d", 20);
    REQUIRE_THROWS(node.insert_separator("d", 30));
}

TEST_CASE(a_full_internal_node_reports_overflow) {
    Page page = empty_internal(1);
    InternalPage node(page);
    const std::string padding(100, 'k');

    int inserted = 0;
    while (node.insert_separator(numbered_key(inserted) + padding, static_cast<PageId>(inserted + 2))) {
        ++inserted;
    }
    REQUIRE(inserted > 0);
    REQUIRE(node.key_count() == inserted);
    REQUIRE(node.child_at(0) == 1);
    for (int i = 0; i < inserted; ++i) {
        REQUIRE(node.key_at(static_cast<std::uint16_t>(i)) == numbered_key(i) + padding);
        REQUIRE(node.child_at(static_cast<std::uint16_t>(i + 1)) == static_cast<PageId>(i + 2));
    }
}

TEST_CASE(split_promotes_the_middle_key_and_keeps_every_child) {
    Page page = empty_internal(1);
    InternalPage node(page);
    const std::string padding(100, 'k');

    int inserted = 0;
    while (node.insert_separator(numbered_key(inserted) + padding, static_cast<PageId>(inserted + 2))) {
        ++inserted;
    }

    Page right_page = empty_internal(kInvalidPageId);
    InternalPage right(right_page);
    const std::string promoted = node.split_into(right);

    // The promoted key lives in neither node.
    REQUIRE(node.key_count() + right.key_count() == inserted - 1);
    REQUIRE(node.key_count() > 0);
    REQUIRE(right.key_count() > 0);
    REQUIRE(node.key_at(static_cast<std::uint16_t>(node.key_count() - 1)) < promoted);
    REQUIRE(right.key_at(0) > promoted);

    // Children are preserved in order across both nodes: 1, 2, 3, ...
    PageId expected = 1;
    for (std::uint16_t i = 0; i <= node.key_count(); ++i) {
        REQUIRE(node.child_at(i) == expected);
        ++expected;
    }
    for (std::uint16_t i = 0; i <= right.key_count(); ++i) {
        REQUIRE(right.child_at(i) == expected);
        ++expected;
    }
    REQUIRE(expected == static_cast<PageId>(inserted + 2));
}

TEST_CASE(split_rejects_unusable_targets) {
    Page page = empty_internal(1);
    InternalPage node(page);
    node.insert_separator("a", 2);
    node.insert_separator("b", 3);

    Page used_page = empty_internal(9);
    InternalPage used(used_page);
    used.insert_separator("z", 10);
    REQUIRE_THROWS(node.split_into(used));

    Page single_page = empty_internal(1);
    InternalPage single(single_page);
    single.insert_separator("only", 2);
    Page target_page = empty_internal(kInvalidPageId);
    InternalPage target(target_page);
    REQUIRE_THROWS(single.split_into(target));
}

TEST_CASE(internal_node_survives_a_pager_round_trip) {
    TempDbFile file("internal_round_trip");
    PageId id = 0;
    {
        Pager pager(file.path());
        id = pager.allocate_page();

        Page page = empty_internal(1);
        InternalPage node(page);
        for (int i = 0; i < 20; ++i) {
            node.insert_separator(numbered_key(i), static_cast<PageId>(i + 2));
        }
        pager.write_page(id, page);
        pager.sync();
    }

    Pager reopened(file.path());
    Page page = reopened.read_page(id);
    InternalPage node(page);
    REQUIRE(node.key_count() == 20);
    REQUIRE(node.child_at(0) == 1);
    for (int i = 0; i < 20; ++i) {
        REQUIRE(node.key_at(static_cast<std::uint16_t>(i)) == numbered_key(i));
        REQUIRE(node.find_child(numbered_key(i)) == static_cast<PageId>(i + 2));
    }
}
