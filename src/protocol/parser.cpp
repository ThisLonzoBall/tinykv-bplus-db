#include "protocol/parser.hpp"

#include <cctype>
#include <sstream>
#include <vector>

namespace db::protocol {

namespace {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string to_upper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

Command parse_command(const std::string& line) {
    std::vector<std::string> tokens = tokenize(line);

    if (tokens.empty()) {
        return InvalidCommand{"empty command"};
    }

    const std::string verb = to_upper(tokens[0]);

    if (verb == "SET") {
        if (tokens.size() < 3) {
            return InvalidCommand{"SET requires a key and a value"};
        }
        // Everything after the key is the value, rejoined with single spaces.
        std::string value = tokens[2];
        for (std::size_t i = 3; i < tokens.size(); ++i) {
            value += " " + tokens[i];
        }
        return SetCommand{tokens[1], value};
    }

    if (verb == "GET") {
        if (tokens.size() != 2) {
            return InvalidCommand{"GET requires exactly one key"};
        }
        return GetCommand{tokens[1]};
    }

    if (verb == "DELETE") {
        if (tokens.size() != 2) {
            return InvalidCommand{"DELETE requires exactly one key"};
        }
        return DeleteCommand{tokens[1]};
    }

    return InvalidCommand{"unknown command: " + tokens[0]};
}

} // namespace db::protocol
