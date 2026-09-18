#include "storage/leaf_page.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

namespace db::storage {

LeafPage LeafPage::format(Page& page) {
    page.set_type(PageType::Leaf);
    page.write_u16(kLeafEntryCountOffset, 0);
    page.write_u16(kLeafHeapStartOffset, static_cast<std::uint16_t>(kPageSize));
    page.write_u32(kLeafNextPageOffset, kInvalidPageId);
    return LeafPage(page);
}

LeafPage::LeafPage(Page& page) : page_(page) {
    if (page_.type() != PageType::Leaf) {
        throw StorageError("page is not a leaf page");
    }
}

std::uint16_t LeafPage::entry_count() const {
    return page_.read_u16(kLeafEntryCountOffset);
}

PageId LeafPage::next_leaf() const {
    return page_.read_u32(kLeafNextPageOffset);
}

void LeafPage::set_next_leaf(PageId id) {
    page_.write_u32(kLeafNextPageOffset, id);
}

std::optional<std::string> LeafPage::find(std::string_view key) const {
    const auto [index, found] = lower_bound(key);
    if (!found) {
        return std::nullopt;
    }
    return std::string(value_view(index));
}

bool LeafPage::insert(std::string_view key, std::string_view value) {
    const std::size_t new_size = kLeafRecordHeaderSize + key.size() + value.size();
    if (new_size > kMaxLeafRecordSize) {
        throw StorageError("record of " + std::to_string(new_size) + " bytes never fits in a leaf page");
    }

    const auto [index, found] = lower_bound(key);
    const std::size_t reclaimable = found ? record_size(index) : 0;
    const std::size_t needed = found ? new_size : new_size + kLeafSlotSize;
    if (free_after_compaction() + reclaimable < needed) {
        return false;
    }

    if (found) {
        erase_slot(index);
    }
    // insert_record always adds a slot, including after an update erased one, so the gap must hold both.
    if (free_space() < new_size + kLeafSlotSize) {
        compact();
    }
    insert_record(index, key, value);
    return true;
}

bool LeafPage::remove(std::string_view key) {
    const auto [index, found] = lower_bound(key);
    if (!found) {
        return false;
    }
    erase_slot(index);
    return true;
}

std::string LeafPage::split_into(LeafPage& right, PageId right_page_id) {
    const std::uint16_t count = entry_count();
    if (count < 2) {
        throw StorageError("cannot split a leaf holding " + std::to_string(count) + " entries");
    }
    if (right.entry_count() != 0) {
        throw StorageError("split target leaf is not empty");
    }

    std::size_t total = 0;
    for (std::uint16_t i = 0; i < count; ++i) {
        total += record_size(i);
    }

    // Split on bytes rather than entry count so one large record cannot leave a lopsided page.
    std::uint16_t split = 0;
    std::size_t accumulated = 0;
    for (std::uint16_t i = 0; i < count; ++i) {
        accumulated += record_size(i);
        if (accumulated * 2 >= total) {
            split = static_cast<std::uint16_t>(i + 1);
            break;
        }
    }
    split = std::max<std::uint16_t>(1, std::min<std::uint16_t>(split, static_cast<std::uint16_t>(count - 1)));

    for (std::uint16_t i = split; i < count; ++i) {
        if (!right.insert(key_view(i), value_view(i))) {
            throw StorageError("split target leaf ran out of space");
        }
    }

    set_entry_count(split);
    compact();

    right.set_next_leaf(next_leaf());
    set_next_leaf(right_page_id);
    return right.key_at(0);
}

std::string LeafPage::key_at(std::uint16_t index) const {
    return std::string(key_view(index));
}

std::string LeafPage::value_at(std::uint16_t index) const {
    return std::string(value_view(index));
}

std::size_t LeafPage::free_space() const {
    return heap_start() - (kLeafHeaderSize + static_cast<std::size_t>(entry_count()) * kLeafSlotSize);
}

std::size_t LeafPage::free_after_compaction() const {
    std::size_t used = 0;
    for (std::uint16_t i = 0; i < entry_count(); ++i) {
        used += record_size(i);
    }
    return kPageSize - kLeafHeaderSize - static_cast<std::size_t>(entry_count()) * kLeafSlotSize - used;
}

std::size_t LeafPage::slot_offset(std::uint16_t index) const {
    return kLeafHeaderSize + static_cast<std::size_t>(index) * kLeafSlotSize;
}

std::uint16_t LeafPage::record_offset(std::uint16_t index) const {
    return page_.read_u16(slot_offset(index));
}

std::size_t LeafPage::record_size(std::uint16_t index) const {
    const std::uint16_t offset = record_offset(index);
    return kLeafRecordHeaderSize + page_.read_u16(offset) + page_.read_u16(offset + 2);
}

std::string_view LeafPage::key_view(std::uint16_t index) const {
    const std::uint16_t offset = record_offset(index);
    const std::uint16_t key_length = page_.read_u16(offset);
    const auto* start = reinterpret_cast<const char*>(page_.data()) + offset + kLeafRecordHeaderSize;
    return std::string_view(start, key_length);
}

std::string_view LeafPage::value_view(std::uint16_t index) const {
    const std::uint16_t offset = record_offset(index);
    const std::uint16_t key_length = page_.read_u16(offset);
    const std::uint16_t value_length = page_.read_u16(offset + 2);
    const auto* start = reinterpret_cast<const char*>(page_.data()) + offset + kLeafRecordHeaderSize + key_length;
    return std::string_view(start, value_length);
}

std::pair<std::uint16_t, bool> LeafPage::lower_bound(std::string_view key) const {
    std::uint16_t low = 0;
    std::uint16_t high = entry_count();
    while (low < high) {
        const auto mid = static_cast<std::uint16_t>(low + (high - low) / 2);
        if (key_view(mid) < key) {
            low = static_cast<std::uint16_t>(mid + 1);
        } else {
            high = mid;
        }
    }
    const bool found = low < entry_count() && key_view(low) == key;
    return {low, found};
}

std::uint16_t LeafPage::heap_start() const {
    return page_.read_u16(kLeafHeapStartOffset);
}

void LeafPage::set_heap_start(std::uint16_t offset) {
    page_.write_u16(kLeafHeapStartOffset, offset);
}

void LeafPage::set_entry_count(std::uint16_t count) {
    page_.write_u16(kLeafEntryCountOffset, count);
}

void LeafPage::erase_slot(std::uint16_t index) {
    const std::uint16_t count = entry_count();
    std::uint8_t* base = page_.data();
    std::memmove(base + slot_offset(index), base + slot_offset(static_cast<std::uint16_t>(index + 1)),
                 static_cast<std::size_t>(count - index - 1) * kLeafSlotSize);
    set_entry_count(static_cast<std::uint16_t>(count - 1));
}

void LeafPage::insert_record(std::uint16_t index, std::string_view key, std::string_view value) {
    const std::size_t size = kLeafRecordHeaderSize + key.size() + value.size();
    const auto offset = static_cast<std::uint16_t>(heap_start() - size);
    assert(offset >= slot_offset(static_cast<std::uint16_t>(entry_count() + 1)));

    page_.write_u16(offset, static_cast<std::uint16_t>(key.size()));
    page_.write_u16(offset + 2, static_cast<std::uint16_t>(value.size()));
    std::uint8_t* base = page_.data();
    std::memcpy(base + offset + kLeafRecordHeaderSize, key.data(), key.size());
    std::memcpy(base + offset + kLeafRecordHeaderSize + key.size(), value.data(), value.size());

    const std::uint16_t count = entry_count();
    std::memmove(base + slot_offset(static_cast<std::uint16_t>(index + 1)), base + slot_offset(index),
                 static_cast<std::size_t>(count - index) * kLeafSlotSize);
    page_.write_u16(slot_offset(index), offset);

    set_heap_start(offset);
    set_entry_count(static_cast<std::uint16_t>(count + 1));
}

void LeafPage::compact() {
    const std::uint16_t count = entry_count();
    std::vector<std::string> records;
    records.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        const auto* start = reinterpret_cast<const char*>(page_.data()) + record_offset(i);
        records.emplace_back(start, record_size(i));
    }

    auto heap = static_cast<std::uint16_t>(kPageSize);
    for (std::uint16_t i = count; i-- > 0;) {
        heap = static_cast<std::uint16_t>(heap - records[i].size());
        std::memcpy(page_.data() + heap, records[i].data(), records[i].size());
        page_.write_u16(slot_offset(i), heap);
    }
    set_heap_start(heap);
}

} // namespace db::storage
