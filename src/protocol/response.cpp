#include "protocol/response.hpp"

namespace db::protocol {

std::string handle_command(const Command& command, storage::KvStore& store) {
    if (const auto* cmd = std::get_if<SetCommand>(&command)) {
        store.set(cmd->key, cmd->value);
        return "OK";
    }

    if (const auto* cmd = std::get_if<GetCommand>(&command)) {
        auto value = store.get(cmd->key);
        return value.has_value() ? *value : "NOT_FOUND";
    }

    if (const auto* cmd = std::get_if<DeleteCommand>(&command)) {
        return store.remove(cmd->key) ? "OK" : "NOT_FOUND";
    }

    const auto& invalid = std::get<InvalidCommand>(command);
    return "ERROR " + invalid.reason;
}

} // namespace db::protocol
