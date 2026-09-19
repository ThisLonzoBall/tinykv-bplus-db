#include "storage/internal_page.hpp"

#include <cassert>
#include <cstring>
#include <vector>

namespace db::storage {

InternalPage InternalPage::format(Page& page, PageId first_child) {
    page.set_type(PageType::Internal);
    page.write_u16(kInternalKeyCountOffset, 0);
    page.write_u16(kInternalHeapStartOffset, static_cast<std::uint16_t>(kPageSize));
    page.write_u32(kInternalFirstChildOffset, first_child);
    return InternalPage(page);
}

InternalPage::InternalPage(Page& page) : page_(page) {
    if (page_.type() != PageType::Internal) {
        throw StorageError("page is not an internal page");
    }
}

std::uint16_t InternalPage::key_count() const {
    return page_.read_u16(kInternalKeyCountOffset);
}

std::string InternalPage::key_at(std::uint16_t index) const {
    return std::string(key_view(index));
}

PageId InternalPage::child_at(std::uint16_t index) const {
    if (index == 0) {
        return page_.read_u32(kInternalFirstChildOffset);
    }
    return page_.read_u32(slot_offset(static_cast<std::uint16_t>(index - 1)) + 2);
}

void InternalPage::set_first_child(PageId id) {
    page_.write_u32(kInternalFirstChildOffset, id);
}

PageId InternalPage::find_child(std::string_view key) const {
    const auto [index, found] = lower_bound(key);
    // A key equal to a separator belongs to the child on its right.
    return child_at(found ? static_cast<std::uint16_t>(index + 1) : index);
}

bool InternalPage::insert_separator(std::string_view key, PageId right_child) {
    const std::size_t record = kInternalRecordHeaderSize + key.size();
    const std::size_t needed = record + kInternalSlotSize;
    if (record > kPageSize - kInternalHeaderSize - kInternalSlotSize) {
        throw StorageError("separator of " + std::to_string(key.size()) + " bytes never fits in an internal page");
    }

    const auto [index, found] = lower_bound(key);
    if (found) {
        throw StorageError("separator already present in internal page");
    }
    if (free_after_compaction() < needed) {
        return false;
    }
    if (free_space() < needed) {
        compact();
    }
    insert_slot(index, key, right_child);
    return true;
}

std::string InternalPage::split_into(InternalPage& right) {
    const std::uint16_t count = key_count();
    if (count < 2) {
        throw StorageError("cannot split an internal node holding " + std::to_string(count) + " separators");
    }
    if (right.key_count() != 0) {
        throw StorageError("split target internal node is not empty");
    }

    const auto middle = static_cast<std::uint16_t>(count / 2);
    std::string promoted = key_at(middle);

    // The middle key moves up to the parent; its right child becomes the right node's first child.
    right.set_first_child(child_at(static_cast<std::uint16_t>(middle + 1)));
    for (std::uint16_t i = static_cast<std::uint16_t>(middle + 1); i < count; ++i) {
        if (!right.insert_separator(key_view(i), child_at(static_cast<std::uint16_t>(i + 1)))) {
            throw StorageError("split target internal node ran out of space");
        }
    }

    set_key_count(middle);
    compact();
    return promoted;
}

std::size_t InternalPage::free_space() const {
    return heap_start() - (kInternalHeaderSize + static_cast<std::size_t>(key_count()) * kInternalSlotSize);
}

std::size_t InternalPage::free_after_compaction() const {
    std::size_t used = 0;
    for (std::uint16_t i = 0; i < key_count(); ++i) {
        used += record_size(i);
    }
    return kPageSize - kInternalHeaderSize - static_cast<std::size_t>(key_count()) * kInternalSlotSize - used;
}

std::size_t InternalPage::slot_offset(std::uint16_t index) const {
    return kInternalHeaderSize + static_cast<std::size_t>(index) * kInternalSlotSize;
}

std::uint16_t InternalPage::record_offset(std::uint16_t index) const {
    return page_.read_u16(slot_offset(index));
}

std::size_t InternalPage::record_size(std::uint16_t index) const {
    return kInternalRecordHeaderSize + page_.read_u16(record_offset(index));
}

std::string_view InternalPage::key_view(std::uint16_t index) const {
    const std::uint16_t offset = record_offset(index);
    const std::uint16_t length = page_.read_u16(offset);
    const auto* start = reinterpret_cast<const char*>(page_.data()) + offset + kInternalRecordHeaderSize;
    return std::string_view(start, length);
}

std::pair<std::uint16_t, bool> InternalPage::lower_bound(std::string_view key) const {
    std::uint16_t low = 0;
    std::uint16_t high = key_count();
    while (low < high) {
        const auto mid = static_cast<std::uint16_t>(low + (high - low) / 2);
        if (key_view(mid) < key) {
            low = static_cast<std::uint16_t>(mid + 1);
        } else {
            high = mid;
        }
    }
    const bool found = low < key_count() && key_view(low) == key;
    return {low, found};
}

std::uint16_t InternalPage::heap_start() const {
    return page_.read_u16(kInternalHeapStartOffset);
}

void InternalPage::set_heap_start(std::uint16_t offset) {
    page_.write_u16(kInternalHeapStartOffset, offset);
}

void InternalPage::set_key_count(std::uint16_t count) {
    page_.write_u16(kInternalKeyCountOffset, count);
}

void InternalPage::insert_slot(std::uint16_t index, std::string_view key, PageId right_child) {
    const std::size_t record = kInternalRecordHeaderSize + key.size();
    const auto offset = static_cast<std::uint16_t>(heap_start() - record);
    assert(offset >= slot_offset(static_cast<std::uint16_t>(key_count() + 1)));

    page_.write_u16(offset, static_cast<std::uint16_t>(key.size()));
    std::uint8_t* base = page_.data();
    std::memcpy(base + offset + kInternalRecordHeaderSize, key.data(), key.size());

    const std::uint16_t count = key_count();
    std::memmove(base + slot_offset(static_cast<std::uint16_t>(index + 1)), base + slot_offset(index),
                 static_cast<std::size_t>(count - index) * kInternalSlotSize);
    page_.write_u16(slot_offset(index), offset);
    page_.write_u32(slot_offset(index) + 2, right_child);

    set_heap_start(offset);
    set_key_count(static_cast<std::uint16_t>(count + 1));
}

void InternalPage::compact() {
    const std::uint16_t count = key_count();
    std::vector<std::string> keys;
    keys.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        keys.emplace_back(key_view(i));
    }

    auto heap = static_cast<std::uint16_t>(kPageSize);
    for (std::uint16_t i = count; i-- > 0;) {
        const std::size_t record = kInternalRecordHeaderSize + keys[i].size();
        heap = static_cast<std::uint16_t>(heap - record);
        page_.write_u16(heap, static_cast<std::uint16_t>(keys[i].size()));
        std::memcpy(page_.data() + heap + kInternalRecordHeaderSize, keys[i].data(), keys[i].size());
        page_.write_u16(slot_offset(i), heap);
    }
    set_heap_start(heap);
}

} // namespace db::storage
