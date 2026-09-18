#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "storage/page.hpp"
#include "storage/storage_error.hpp"

namespace db::storage {

// Leaf header, after the common page header.
constexpr std::size_t kLeafEntryCountOffset = kPageHeaderSize;    // u16
constexpr std::size_t kLeafHeapStartOffset = kPageHeaderSize + 2; // u16, lowest byte used by records
constexpr std::size_t kLeafNextPageOffset = kPageHeaderSize + 4;  // u32, sibling pointer
constexpr std::size_t kLeafHeaderSize = kPageHeaderSize + 8;

constexpr std::size_t kLeafSlotSize = 2;       // u16 record offset
constexpr std::size_t kLeafRecordHeaderSize = 4; // u16 key length + u16 value length

// The largest record that fits in an otherwise empty leaf.
constexpr std::size_t kMaxLeafRecordSize = kPageSize - kLeafHeaderSize - kLeafSlotSize;

// A view over a Page holding sorted key/value records.
//
// Slots grow up from the header and records grow down from the end of the page. Slots are kept in key
// order, so a lookup is a binary search over them.
class LeafPage {
public:
    static LeafPage format(Page& page);

    // Throws StorageError if the page is not a formatted leaf.
    explicit LeafPage(Page& page);

    std::uint16_t entry_count() const;

    PageId next_leaf() const;
    void set_next_leaf(PageId id);

    std::optional<std::string> find(std::string_view key) const;

    // Replaces the value if the key is present. Returns false and leaves the page untouched when it is
    // full, which is the caller's signal to split. Throws if the record cannot fit in any leaf.
    bool insert(std::string_view key, std::string_view value);

    bool remove(std::string_view key);

    // Moves the upper half (by bytes) into empty `right`, wires siblings, returns the separator key.
    std::string split_into(LeafPage& right, PageId right_page_id);

    std::string key_at(std::uint16_t index) const;
    std::string value_at(std::uint16_t index) const;

    // Contiguous free bytes; less than free_after_compaction() when removals left gaps.
    std::size_t free_space() const;
    std::size_t free_after_compaction() const;

private:
    std::size_t slot_offset(std::uint16_t index) const;
    std::uint16_t record_offset(std::uint16_t index) const;
    std::size_t record_size(std::uint16_t index) const;
    std::string_view key_view(std::uint16_t index) const;
    std::string_view value_view(std::uint16_t index) const;

    // Index of the first key >= the argument, and whether it is an exact match.
    std::pair<std::uint16_t, bool> lower_bound(std::string_view key) const;

    std::uint16_t heap_start() const;
    void set_heap_start(std::uint16_t offset);
    void set_entry_count(std::uint16_t count);

    void erase_slot(std::uint16_t index);
    void insert_record(std::uint16_t index, std::string_view key, std::string_view value);
    void compact();

    Page& page_;
};

} // namespace db::storage
