#pragma once

#include <stdexcept>

namespace db::storage {

class StorageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace db::storage
