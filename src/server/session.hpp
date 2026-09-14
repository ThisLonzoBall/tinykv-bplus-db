#pragma once

#include "storage/kv_store.hpp"

namespace db::server {

// Owns one client connection: reads newline-terminated commands, dispatches
// them to the store, writes back responses, until the client disconnects.
// Runs to completion on whatever thread calls it and always closes the
// socket before returning.
void run_session(int client_fd, storage::KvStore& store);

} // namespace db::server
