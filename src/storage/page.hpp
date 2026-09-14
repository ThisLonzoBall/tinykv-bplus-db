#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace db::storage {

constexpr std::size_t kPageSize = 4096;

using PageId = std::uint32_t;
constexpr PageId kInvalidPageId = std::numeric_limits<PageId>::max();

enum class PageType : std::uint8_t {
    Empty = 0,
    Meta = 1,
    Free = 2,
};

// Common header shared by every page. All integers are little-endian.
constexpr std::size_t kChecksumOffset = 0; // u32, CRC-32 of bytes [4, kPageSize)
constexpr std::size_t kPageTypeOffset = 4; // u8
constexpr std::size_t kPageIdOffset = 8;   // u32
constexpr std::size_t kPageHeaderSize = 16;

class Page {
public:
    PageType type() const;
    void set_type(PageType type);

    PageId id() const;
    void set_id(PageId id);

    std::uint32_t read_u32(std::size_t offset) const;
    void write_u32(std::size_t offset, std::uint32_t value);

    void update_checksum();
    bool checksum_valid() const;

    const std::uint8_t* data() const { return bytes_.data(); }
    std::uint8_t* data() { return bytes_.data(); }

private:
    std::uint32_t compute_checksum() const;

    std::array<std::uint8_t, kPageSize> bytes_{};
};

} // namespace db::storage
