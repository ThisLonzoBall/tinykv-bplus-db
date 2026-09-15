#include "storage/page.hpp"

#include <cassert>

#include "storage/checksum.hpp"

namespace db::storage {

PageType Page::type() const {
    return static_cast<PageType>(bytes_[kPageTypeOffset]);
}

void Page::set_type(PageType type) {
    bytes_[kPageTypeOffset] = static_cast<std::uint8_t>(type);
}

PageId Page::id() const {
    return read_u32(kPageIdOffset);
}

void Page::set_id(PageId id) {
    write_u32(kPageIdOffset, id);
}

std::uint16_t Page::read_u16(std::size_t offset) const {
    assert(offset + 2 <= kPageSize);
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes_[offset]) |
                                      static_cast<std::uint16_t>(bytes_[offset + 1] << 8));
}

void Page::write_u16(std::size_t offset, std::uint16_t value) {
    assert(offset + 2 <= kPageSize);
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

std::uint32_t Page::read_u32(std::size_t offset) const {
    assert(offset + 4 <= kPageSize);
    return static_cast<std::uint32_t>(bytes_[offset]) | (static_cast<std::uint32_t>(bytes_[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes_[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes_[offset + 3]) << 24);
}

void Page::write_u32(std::size_t offset, std::uint32_t value) {
    assert(offset + 4 <= kPageSize);
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes_[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes_[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

std::uint32_t Page::compute_checksum() const {
    const std::size_t start = kChecksumOffset + 4;
    return crc32(bytes_.data() + start, kPageSize - start);
}

void Page::update_checksum() {
    write_u32(kChecksumOffset, compute_checksum());
}

bool Page::checksum_valid() const {
    return read_u32(kChecksumOffset) == compute_checksum();
}

} // namespace db::storage
