#pragma once

#include <string>
#include <variant>

namespace db::protocol {

struct SetCommand {
    std::string key;
    std::string value;
};

struct GetCommand {
    std::string key;
};

struct DeleteCommand {
    std::string key;
};

// A line that isn't a well-formed SET/GET/DELETE. Carries a human-readable
// reason so the caller can echo it back to the client.
struct InvalidCommand {
    std::string reason;
};

using Command = std::variant<SetCommand, GetCommand, DeleteCommand, InvalidCommand>;

// Parses one line of input (no trailing newline) into a Command.
// Whitespace-separated: "SET key value", "GET key", "DELETE key".
Command parse_command(const std::string& line);

} // namespace db::protocol
