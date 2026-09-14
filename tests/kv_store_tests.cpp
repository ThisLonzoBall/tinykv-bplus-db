#include "storage/kv_store.hpp"
#include "test_framework.hpp"

using db::storage::KvStore;

TEST_CASE(set_then_get_returns_value) {
    KvStore store;
    store.set("name", "Alice");
    auto value = store.get("name");
    REQUIRE(value.has_value());
    REQUIRE(*value == "Alice");
}

TEST_CASE(get_missing_key_returns_nullopt) {
    KvStore store;
    REQUIRE(!store.get("missing").has_value());
}

TEST_CASE(set_overwrites_existing_value) {
    KvStore store;
    store.set("name", "Alice");
    store.set("name", "Bob");
    REQUIRE(*store.get("name") == "Bob");
}

TEST_CASE(delete_existing_key_returns_true_and_removes_it) {
    KvStore store;
    store.set("name", "Alice");
    REQUIRE(store.remove("name") == true);
    REQUIRE(!store.get("name").has_value());
}

TEST_CASE(delete_missing_key_returns_false) {
    KvStore store;
    REQUIRE(store.remove("missing") == false);
}
