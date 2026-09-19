#include "storage/btree.hpp"

#include "storage/internal_page.hpp"
#include "storage/leaf_page.hpp"

namespace db::storage {

BTree::BTree(Pager& pager) : pager_(pager) {
    root_ = pager_.root_page_id();
    if (root_ != kInvalidPageId) {
        return;
    }

    root_ = pager_.allocate_page();
    Page page;
    LeafPage::format(page);
    pager_.write_page(root_, page);
    pager_.set_root_page_id(root_);
}

std::optional<std::string> BTree::get(std::string_view key) const {
    PageId page_id = root_;
    while (true) {
        Page page = pager_.read_page(page_id);
        if (page.type() == PageType::Leaf) {
            return LeafPage(page).find(key);
        }
        page_id = InternalPage(page).find_child(key);
    }
}

void BTree::put(std::string_view key, std::string_view value) {
    const Split split = insert_into(root_, key, value);
    if (!split.happened) {
        return;
    }

    // The root split, so the tree gains a level.
    const PageId new_root = pager_.allocate_page();
    Page page;
    InternalPage root = InternalPage::format(page, root_);
    if (!root.insert_separator(split.separator, split.right)) {
        throw StorageError("separator does not fit in a new root");
    }
    pager_.write_page(new_root, page);

    root_ = new_root;
    pager_.set_root_page_id(new_root);
}

std::uint32_t BTree::height() const {
    std::uint32_t levels = 1;
    PageId page_id = root_;
    while (true) {
        Page page = pager_.read_page(page_id);
        if (page.type() == PageType::Leaf) {
            return levels;
        }
        page_id = InternalPage(page).child_at(0);
        ++levels;
    }
}

BTree::Split BTree::insert_into(PageId page_id, std::string_view key, std::string_view value) {
    Page page = pager_.read_page(page_id);

    if (page.type() == PageType::Leaf) {
        LeafPage leaf(page);
        if (leaf.insert(key, value)) {
            pager_.write_page(page_id, page);
            return {};
        }

        const PageId right_id = pager_.allocate_page();
        Page right_page;
        LeafPage right = LeafPage::format(right_page);
        const std::string separator = leaf.split_into(right, right_id);

        const bool goes_right = key >= separator;
        if (!(goes_right ? right.insert(key, value) : leaf.insert(key, value))) {
            throw StorageError("record does not fit after splitting a leaf");
        }

        pager_.write_page(page_id, page);
        pager_.write_page(right_id, right_page);
        return {true, separator, right_id};
    }

    InternalPage node(page);
    const Split child = insert_into(node.find_child(key), key, value);
    if (!child.happened) {
        return {};
    }

    if (node.insert_separator(child.separator, child.right)) {
        pager_.write_page(page_id, page);
        return {};
    }

    const PageId right_id = pager_.allocate_page();
    Page right_page;
    InternalPage right = InternalPage::format(right_page, kInvalidPageId);
    const std::string promoted = node.split_into(right);

    const bool goes_right = child.separator > promoted;
    if (!(goes_right ? right.insert_separator(child.separator, child.right)
                     : node.insert_separator(child.separator, child.right))) {
        throw StorageError("separator does not fit after splitting an internal node");
    }

    pager_.write_page(page_id, page);
    pager_.write_page(right_id, right_page);
    return {true, promoted, right_id};
}

} // namespace db::storage
