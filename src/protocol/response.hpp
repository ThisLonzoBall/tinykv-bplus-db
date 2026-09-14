#pragma once

#include <string>

#include "protocol/parser.hpp"
#include "storage/kv_store.hpp"

namespace db::protocol {

// Executes a parsed command against the store and returns the wire response
// line (no trailing newline). One of: "OK", "<value>", "NOT_FOUND",
// "ERROR <reason>".
std::string handle_command(const Command& command, storage::KvStore& store);

} // namespace db::protocol
