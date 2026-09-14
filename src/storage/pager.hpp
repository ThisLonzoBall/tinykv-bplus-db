#pragma once

#include <stdexcept>
#include <string>

#include "storage/page.hpp"

namespace db::storage {

class StorageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Maps one database file onto fixed-size pages. Page 0 holds metadata and is managed internally.
class Pager {
public:
    // Creates and initializes the file if it doesn't exist; throws StorageError if it isn't a valid database.
    explicit Pager(const std::string& path);
    ~Pager();

    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    Page read_page(PageId id) const;

    // Stamps the page's id and checksum before writing.
    void write_page(PageId id, const Page& page);

    // Reuses a freed page if one exists, otherwise grows the file. The page starts as PageType::Empty.
    PageId allocate_page();

    void free_page(PageId id);

    // Writes are buffered by the OS until this is called.
    void sync();

    // Includes the metadata page.
    PageId page_count() const { return page_count_; }

private:
    void init_or_load(const std::string& path);
    void check_user_page_id(PageId id) const;
    Page read_raw(PageId id) const;
    void write_raw(PageId id, Page page);
    void write_meta();

    int fd_ = -1;
    PageId page_count_ = 0;
    PageId free_list_head_ = kInvalidPageId;
};

} // namespace db::storage
