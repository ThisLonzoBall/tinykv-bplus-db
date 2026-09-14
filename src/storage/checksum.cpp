#include "storage/checksum.hpp"

#include <array>

namespace db::storage {

namespace {

std::array<std::uint32_t, 256> make_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int bit = 0; bit < 8; ++bit) {
            c = (c & 1U) != 0 ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

} // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t length) {
    static const std::array<std::uint32_t, 256> table = make_table();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFU] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

} // namespace db::storage
