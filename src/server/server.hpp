#pragma once

#include <cstdint>

#include "storage/kv_store.hpp"

namespace db::server {

// Binds to 0.0.0.0:port, accepts connections in a loop, and spawns one
// detached thread per connection running run_session(). Blocks forever
// (until the process is killed) — there is no graceful shutdown yet.
// Returns a non-zero exit code if the socket could not be set up.
int run_server(std::uint16_t port, storage::KvStore& store);

} // namespace db::server
