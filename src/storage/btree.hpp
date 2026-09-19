#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "storage/pager.hpp"

namespace db::storage {

// A disk-backed B+ tree. Records live in leaves; internal nodes hold separators and child pointers.
// The root page id is kept in the pager's metadata page, so a tree reopens with its file.
class BTree {
public:
    // Creates an empty root leaf if the file does not have a tree yet.
    explicit BTree(Pager& pager);

    std::optional<std::string> get(std::string_view key) const;

    // Inserts, or replaces the value if the key is already present.
    void put(std::string_view key, std::string_view value);

    PageId root_page_id() const { return root_; }

    // 1 for a tree that is just a leaf.
    std::uint32_t height() const;

private:
    struct Split {
        bool happened = false;
        std::string separator;
        PageId right = kInvalidPageId;
    };

    Split insert_into(PageId page_id, std::string_view key, std::string_view value);

    Pager& pager_;
    PageId root_ = kInvalidPageId;
};

} // namespace db::storage
