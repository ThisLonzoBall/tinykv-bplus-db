#include <cstring>

#include "storage/checksum.hpp"
#include "storage/page.hpp"
#include "test_framework.hpp"

using namespace db::storage;

TEST_CASE(crc32_matches_standard_check_value) {
    const char* input = "123456789";
    REQUIRE(crc32(reinterpret_cast<const std::uint8_t*>(input), std::strlen(input)) == 0xCBF43926U);
}

TEST_CASE(u32_round_trips_as_little_endian) {
    Page page;
    page.write_u32(100, 0x12345678U);
    REQUIRE(page.read_u32(100) == 0x12345678U);
    REQUIRE(page.data()[100] == 0x78);
    REQUIRE(page.data()[103] == 0x12);
}

TEST_CASE(u32_at_last_valid_offset) {
    Page page;
    page.write_u32(kPageSize - 4, 0xDEADBEEFU);
    REQUIRE(page.read_u32(kPageSize - 4) == 0xDEADBEEFU);
}

TEST_CASE(type_and_id_round_trip) {
    Page page;
    page.set_type(PageType::Free);
    page.set_id(42);
    REQUIRE(page.type() == PageType::Free);
    REQUIRE(page.id() == 42);
}

TEST_CASE(checksum_detects_any_modified_byte) {
    Page page;
    page.write_u32(kPageHeaderSize, 7);
    page.update_checksum();
    REQUIRE(page.checksum_valid());

    page.data()[kPageSize - 1] ^= 0x01;
    REQUIRE(!page.checksum_valid());
}

TEST_CASE(checksum_covers_header_fields) {
    Page page;
    page.update_checksum();
    page.set_id(1);
    REQUIRE(!page.checksum_valid());
}
