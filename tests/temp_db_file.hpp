#pragma once

#include <unistd.h>

#include <filesystem>
#include <string>

// A database file path in the temp directory, removed before and after the test.
class TempDbFile {
public:
    explicit TempDbFile(const std::string& name)
        : path_((std::filesystem::temp_directory_path() /
                 ("tinykv_" + name + "_" + std::to_string(::getpid()) + ".db"))
                    .string()) {
        std::filesystem::remove(path_);
    }
    ~TempDbFile() { std::filesystem::remove(path_); }

    TempDbFile(const TempDbFile&) = delete;
    TempDbFile& operator=(const TempDbFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};
