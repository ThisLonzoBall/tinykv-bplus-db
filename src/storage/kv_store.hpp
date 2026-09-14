#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace db::storage {

// Thread-safe in-memory key-value store. Coarse-locked: correct under
// concurrent clients, not optimized for it. Fine-grained concurrency is a
// later phase once disk storage exists.
class KvStore {
public:
    void set(const std::string& key, const std::string& value);

    // Returns std::nullopt if the key does not exist.
    std::optional<std::string> get(const std::string& key) const;

    // Returns true if the key existed and was removed.
    bool remove(const std::string& key);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};

} // namespace db::storage
