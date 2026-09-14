#pragma once

#include <cstddef>
#include <cstdint>

namespace db::storage {

// CRC-32 (IEEE, reflected polynomial 0xEDB88320) — same output as zlib's crc32.
std::uint32_t crc32(const std::uint8_t* data, std::size_t length);

} // namespace db::storage
