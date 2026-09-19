#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "storage/page.hpp"
#include "storage/storage_error.hpp"

namespace db::storage {

// Internal header, after the common page header.
constexpr std::size_t kInternalKeyCountOffset = kPageHeaderSize;     // u16
constexpr std::size_t kInternalHeapStartOffset = kPageHeaderSize + 2; // u16
constexpr std::size_t kInternalFirstChildOffset = kPageHeaderSize + 4; // u32, leftmost child
constexpr std::size_t kInternalHeaderSize = kPageHeaderSize + 8;

constexpr std::size_t kInternalSlotSize = 6;         // u16 key offset + u32 right child
constexpr std::size_t kInternalRecordHeaderSize = 2; // u16 key length

// An internal node: n separator keys and n + 1 child pointers.
//
// Child 0 lives in the header; child i + 1 lives in slot i alongside separator i. Keys in child i are
// less than separator i, and keys in child i + 1 are greater than or equal to it.
class InternalPage {
public:
    static InternalPage format(Page& page, PageId first_child);

    // Throws StorageError if the page is not a formatted internal node.
    explicit InternalPage(Page& page);

    std::uint16_t key_count() const;

    std::string key_at(std::uint16_t index) const;
    PageId child_at(std::uint16_t index) const; // index in [0, key_count]
    void set_first_child(PageId id);

    // The child whose subtree must contain key.
    PageId find_child(std::string_view key) const;

    // Returns false when the node is full, which is the caller's signal to split.
    bool insert_separator(std::string_view key, PageId right_child);

    // Moves the upper half into empty `right` and returns the middle key, which moves up to the
    // parent and is stored in neither node.
    std::string split_into(InternalPage& right);

    std::size_t free_space() const;
    std::size_t free_after_compaction() const;

private:
    std::size_t slot_offset(std::uint16_t index) const;
    std::uint16_t record_offset(std::uint16_t index) const;
    std::size_t record_size(std::uint16_t index) const;
    std::string_view key_view(std::uint16_t index) const;

    // Index of the first separator >= the argument, and whether it is an exact match.
    std::pair<std::uint16_t, bool> lower_bound(std::string_view key) const;

    std::uint16_t heap_start() const;
    void set_heap_start(std::uint16_t offset);
    void set_key_count(std::uint16_t count);

    void insert_slot(std::uint16_t index, std::string_view key, PageId right_child);
    void compact();

    Page& page_;
};

} // namespace db::storage
