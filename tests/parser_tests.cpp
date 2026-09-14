#include "protocol/parser.hpp"
#include "test_framework.hpp"

using namespace db::protocol;

TEST_CASE(parses_set_command) {
    Command cmd = parse_command("SET name Alice");
    auto* set = std::get_if<SetCommand>(&cmd);
    REQUIRE(set != nullptr);
    REQUIRE(set->key == "name");
    REQUIRE(set->value == "Alice");
}

TEST_CASE(parses_set_command_with_multi_word_value) {
    Command cmd = parse_command("SET greeting hello there world");
    auto* set = std::get_if<SetCommand>(&cmd);
    REQUIRE(set != nullptr);
    REQUIRE(set->key == "greeting");
    REQUIRE(set->value == "hello there world");
}

TEST_CASE(parses_get_command) {
    Command cmd = parse_command("GET name");
    auto* get = std::get_if<GetCommand>(&cmd);
    REQUIRE(get != nullptr);
    REQUIRE(get->key == "name");
}

TEST_CASE(parses_delete_command) {
    Command cmd = parse_command("DELETE name");
    auto* del = std::get_if<DeleteCommand>(&cmd);
    REQUIRE(del != nullptr);
    REQUIRE(del->key == "name");
}

TEST_CASE(is_case_insensitive_on_verb) {
    Command cmd = parse_command("get name");
    REQUIRE(std::get_if<GetCommand>(&cmd) != nullptr);
}

TEST_CASE(rejects_set_missing_value) {
    Command cmd = parse_command("SET name");
    REQUIRE(std::get_if<InvalidCommand>(&cmd) != nullptr);
}

TEST_CASE(rejects_get_with_extra_arguments) {
    Command cmd = parse_command("GET name extra");
    REQUIRE(std::get_if<InvalidCommand>(&cmd) != nullptr);
}

TEST_CASE(rejects_empty_line) {
    Command cmd = parse_command("");
    REQUIRE(std::get_if<InvalidCommand>(&cmd) != nullptr);
}

TEST_CASE(rejects_unknown_verb) {
    Command cmd = parse_command("FOO bar");
    REQUIRE(std::get_if<InvalidCommand>(&cmd) != nullptr);
}

TEST_CASE(tolerates_extra_whitespace_between_tokens) {
    Command cmd = parse_command("SET   name    Alice");
    auto* set = std::get_if<SetCommand>(&cmd);
    REQUIRE(set != nullptr);
    REQUIRE(set->key == "name");
    REQUIRE(set->value == "Alice");
}
