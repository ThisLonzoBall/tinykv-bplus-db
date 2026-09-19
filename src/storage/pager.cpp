#include "storage/pager.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>

namespace db::storage {

namespace {

constexpr PageId kMetaPageId = 0;
constexpr std::uint32_t kMagic = 0x31564B54; // "TKV1" as little-endian bytes
constexpr std::uint32_t kFormatVersion = 1;

constexpr std::size_t kMetaMagicOffset = kPageHeaderSize;
constexpr std::size_t kMetaVersionOffset = kPageHeaderSize + 4;
constexpr std::size_t kMetaPageCountOffset = kPageHeaderSize + 8;
constexpr std::size_t kMetaFreeListHeadOffset = kPageHeaderSize + 12;
constexpr std::size_t kMetaRootPageOffset = kPageHeaderSize + 16;

constexpr std::size_t kFreeNextOffset = kPageHeaderSize;

[[noreturn]] void throw_errno(const std::string& what) {
    throw StorageError(what + ": " + std::strerror(errno));
}

off_t file_offset(PageId id, std::size_t within_page) {
    return static_cast<off_t>(id) * static_cast<off_t>(kPageSize) + static_cast<off_t>(within_page);
}

} // namespace

Pager::Pager(const std::string& path) {
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) {
        throw_errno("open " + path);
    }
    try {
        init_or_load(path);
    } catch (...) {
        ::close(fd_);
        throw;
    }
}

Pager::~Pager() {
    ::close(fd_);
}

void Pager::init_or_load(const std::string& path) {
    struct stat st {};
    if (::fstat(fd_, &st) < 0) {
        throw_errno("fstat " + path);
    }
    const auto file_size = static_cast<std::uint64_t>(st.st_size);

    if (file_size == 0) {
        page_count_ = 1;
        free_list_head_ = kInvalidPageId;
        root_page_id_ = kInvalidPageId;
        write_meta();
        return;
    }

    if (file_size % kPageSize != 0) {
        throw StorageError(path + ": size " + std::to_string(file_size) + " is not a multiple of the page size");
    }

    const Page meta = read_raw(kMetaPageId);
    if (meta.type() != PageType::Meta || meta.read_u32(kMetaMagicOffset) != kMagic) {
        throw StorageError(path + ": not a TinyKV database file");
    }
    if (meta.read_u32(kMetaVersionOffset) != kFormatVersion) {
        throw StorageError(path + ": unsupported format version " + std::to_string(meta.read_u32(kMetaVersionOffset)));
    }

    page_count_ = meta.read_u32(kMetaPageCountOffset);
    free_list_head_ = meta.read_u32(kMetaFreeListHeadOffset);
    root_page_id_ = meta.read_u32(kMetaRootPageOffset);

    // A file longer than page_count is tolerated: allocate_page extends the file before updating metadata.
    if (page_count_ == 0 || static_cast<std::uint64_t>(page_count_) * kPageSize > file_size) {
        throw StorageError(path + ": metadata page count " + std::to_string(page_count_) + " exceeds file size");
    }
}

Page Pager::read_page(PageId id) const {
    check_user_page_id(id);
    return read_raw(id);
}

void Pager::write_page(PageId id, const Page& page) {
    check_user_page_id(id);
    write_raw(id, page);
}

PageId Pager::allocate_page() {
    if (free_list_head_ == kInvalidPageId) {
        const PageId id = page_count_;
        write_raw(id, Page{});
        ++page_count_;
        write_meta();
        return id;
    }

    const PageId id = free_list_head_;
    const Page head = read_raw(id);
    if (head.type() != PageType::Free) {
        throw StorageError("free list head " + std::to_string(id) + " is not a free page");
    }
    free_list_head_ = head.read_u32(kFreeNextOffset);
    // Unlink before overwriting: a crash in between leaks the page rather than corrupting the free list.
    write_meta();
    write_raw(id, Page{});
    return id;
}

void Pager::free_page(PageId id) {
    check_user_page_id(id);
    if (read_raw(id).type() == PageType::Free) {
        throw StorageError("double free of page " + std::to_string(id));
    }

    Page page;
    page.set_type(PageType::Free);
    page.write_u32(kFreeNextOffset, free_list_head_);
    write_raw(id, page);

    free_list_head_ = id;
    write_meta();
}

void Pager::set_root_page_id(PageId id) {
    check_user_page_id(id);
    root_page_id_ = id;
    write_meta();
}

void Pager::sync() {
    if (::fsync(fd_) < 0) {
        throw_errno("fsync");
    }
}

void Pager::check_user_page_id(PageId id) const {
    if (id == kMetaPageId || id >= page_count_) {
        throw StorageError("invalid page id " + std::to_string(id) + " (page count " + std::to_string(page_count_) +
                           ")");
    }
}

Page Pager::read_raw(PageId id) const {
    Page page;
    std::size_t done = 0;
    while (done < kPageSize) {
        const ssize_t n = ::pread(fd_, page.data() + done, kPageSize - done, file_offset(id, done));
        if (n < 0) {
            throw_errno("read page " + std::to_string(id));
        }
        if (n == 0) {
            throw StorageError("unexpected end of file reading page " + std::to_string(id));
        }
        done += static_cast<std::size_t>(n);
    }

    if (!page.checksum_valid()) {
        throw StorageError("checksum mismatch on page " + std::to_string(id));
    }
    if (page.id() != id) {
        throw StorageError("page " + std::to_string(id) + " has header id " + std::to_string(page.id()));
    }
    return page;
}

void Pager::write_raw(PageId id, Page page) {
    page.set_id(id);
    page.update_checksum();

    std::size_t done = 0;
    while (done < kPageSize) {
        const ssize_t n = ::pwrite(fd_, page.data() + done, kPageSize - done, file_offset(id, done));
        if (n < 0) {
            throw_errno("write page " + std::to_string(id));
        }
        done += static_cast<std::size_t>(n);
    }
}

void Pager::write_meta() {
    Page meta;
    meta.set_type(PageType::Meta);
    meta.write_u32(kMetaMagicOffset, kMagic);
    meta.write_u32(kMetaVersionOffset, kFormatVersion);
    meta.write_u32(kMetaPageCountOffset, page_count_);
    meta.write_u32(kMetaFreeListHeadOffset, free_list_head_);
    meta.write_u32(kMetaRootPageOffset, root_page_id_);
    write_raw(kMetaPageId, meta);
}

} // namespace db::storage
