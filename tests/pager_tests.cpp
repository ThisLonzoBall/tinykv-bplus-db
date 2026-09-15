#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "storage/pager.hpp"
#include "temp_db_file.hpp"
#include "test_framework.hpp"

using namespace db::storage;

namespace {

Page page_with_pattern(std::uint32_t seed) {
    Page page;
    for (std::size_t offset = kPageHeaderSize; offset + 4 <= kPageSize; offset += 4) {
        page.write_u32(offset, seed * 2654435761U + static_cast<std::uint32_t>(offset));
    }
    return page;
}

bool payload_equal(const Page& a, const Page& b) {
    return std::equal(a.data() + kPageHeaderSize, a.data() + kPageSize, b.data() + kPageHeaderSize);
}

} // namespace

TEST_CASE(new_file_contains_only_metadata_page) {
    TempDbFile file("new_file");
    Pager pager(file.path());
    REQUIRE(pager.page_count() == 1);
    REQUIRE(std::filesystem::file_size(file.path()) == kPageSize);
}

TEST_CASE(allocate_grows_file_sequentially) {
    TempDbFile file("allocate_grows");
    Pager pager(file.path());
    REQUIRE(pager.allocate_page() == 1);
    REQUIRE(pager.allocate_page() == 2);
    REQUIRE(pager.allocate_page() == 3);
    REQUIRE(pager.page_count() == 4);
    REQUIRE(std::filesystem::file_size(file.path()) == 4 * kPageSize);
}

TEST_CASE(allocated_page_starts_empty) {
    TempDbFile file("allocated_empty");
    Pager pager(file.path());
    const PageId id = pager.allocate_page();
    const Page page = pager.read_page(id);
    REQUIRE(page.type() == PageType::Empty);
    REQUIRE(page.id() == id);
    REQUIRE(payload_equal(page, Page{}));
}

TEST_CASE(write_then_read_round_trips_bytes) {
    TempDbFile file("round_trip");
    Pager pager(file.path());
    const PageId id = pager.allocate_page();
    pager.write_page(id, page_with_pattern(1));
    REQUIRE(payload_equal(pager.read_page(id), page_with_pattern(1)));
}

TEST_CASE(pages_survive_reopen) {
    TempDbFile file("reopen");
    {
        Pager pager(file.path());
        for (std::uint32_t i = 0; i < 50; ++i) {
            const PageId id = pager.allocate_page();
            pager.write_page(id, page_with_pattern(id));
        }
        pager.sync();
    }

    Pager reopened(file.path());
    REQUIRE(reopened.page_count() == 51);
    for (PageId id = 1; id <= 50; ++id) {
        REQUIRE(payload_equal(reopened.read_page(id), page_with_pattern(id)));
    }
}

TEST_CASE(freed_pages_are_reused_before_growing) {
    TempDbFile file("reuse");
    Pager pager(file.path());
    pager.allocate_page();
    const PageId second = pager.allocate_page();
    const PageId third = pager.allocate_page();

    pager.free_page(second);
    pager.free_page(third);

    REQUIRE(pager.allocate_page() == third);
    REQUIRE(pager.allocate_page() == second);
    REQUIRE(pager.allocate_page() == 4);
    REQUIRE(pager.page_count() == 5);
}

TEST_CASE(reused_page_does_not_keep_old_contents) {
    TempDbFile file("reuse_clears");
    Pager pager(file.path());
    const PageId id = pager.allocate_page();
    pager.write_page(id, page_with_pattern(9));
    pager.free_page(id);

    REQUIRE(pager.allocate_page() == id);
    const Page page = pager.read_page(id);
    REQUIRE(page.type() == PageType::Empty);
    REQUIRE(payload_equal(page, Page{}));
}

TEST_CASE(free_list_survives_reopen) {
    TempDbFile file("free_list_reopen");
    {
        Pager pager(file.path());
        pager.allocate_page();
        const PageId id = pager.allocate_page();
        pager.allocate_page();
        pager.free_page(id);
    }

    Pager reopened(file.path());
    REQUIRE(reopened.allocate_page() == 2);
    REQUIRE(reopened.allocate_page() == 4);
}

TEST_CASE(rejects_invalid_page_ids) {
    TempDbFile file("invalid_ids");
    Pager pager(file.path());
    pager.allocate_page();

    REQUIRE_THROWS(pager.read_page(0));
    REQUIRE_THROWS(pager.read_page(2));
    REQUIRE_THROWS(pager.read_page(kInvalidPageId));
    REQUIRE_THROWS(pager.write_page(0, Page{}));
    REQUIRE_THROWS(pager.write_page(2, Page{}));
    REQUIRE_THROWS(pager.free_page(0));
    REQUIRE_THROWS(pager.free_page(2));
}

TEST_CASE(rejects_double_free) {
    TempDbFile file("double_free");
    Pager pager(file.path());
    const PageId id = pager.allocate_page();
    pager.free_page(id);
    REQUIRE_THROWS(pager.free_page(id));
}

TEST_CASE(detects_corrupted_page_on_disk) {
    TempDbFile file("corruption");
    {
        Pager pager(file.path());
        const PageId id = pager.allocate_page();
        pager.write_page(id, page_with_pattern(3));
    }
    {
        const auto offset = static_cast<std::streamoff>(kPageSize + 1000);
        std::fstream raw(file.path(), std::ios::in | std::ios::out | std::ios::binary);
        raw.seekg(offset);
        const char original = static_cast<char>(raw.get());
        raw.seekp(offset);
        raw.put(static_cast<char>(original ^ 0xFF));
    }

    Pager reopened(file.path());
    REQUIRE_THROWS(reopened.read_page(1));
}

TEST_CASE(rejects_file_that_is_not_a_database) {
    TempDbFile file("not_a_db");
    {
        std::ofstream raw(file.path(), std::ios::binary);
        raw << std::string(kPageSize, 'x');
    }
    REQUIRE_THROWS(Pager(file.path()));
}

TEST_CASE(rejects_file_with_partial_page) {
    TempDbFile file("partial_page");
    {
        Pager pager(file.path());
    }
    {
        std::ofstream raw(file.path(), std::ios::binary | std::ios::app);
        raw << "torn";
    }
    REQUIRE_THROWS(Pager(file.path()));
}
