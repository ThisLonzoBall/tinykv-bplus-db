CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -Isrc -MMD -MP
LDFLAGS := -pthread

BUILD_DIR := build

CORE_SRCS := \
    src/storage/kv_store.cpp \
    src/storage/checksum.cpp \
    src/storage/page.cpp \
    src/storage/pager.cpp \
    src/storage/leaf_page.cpp \
    src/protocol/parser.cpp \
    src/protocol/response.cpp \
    src/server/session.cpp \
    src/server/server.cpp

CORE_OBJS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))

TEST_SRCS := \
    tests/test_main.cpp \
    tests/kv_store_tests.cpp \
    tests/parser_tests.cpp \
    tests/page_tests.cpp \
    tests/pager_tests.cpp \
    tests/leaf_page_tests.cpp
TEST_OBJS := $(patsubst tests/%.cpp,$(BUILD_DIR)/tests/%.o,$(TEST_SRCS))

DEPS := $(CORE_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(BUILD_DIR)/main.d $(BUILD_DIR)/client_main.d

.PHONY: all clean test

all: $(BUILD_DIR)/db_server $(BUILD_DIR)/db_client

$(BUILD_DIR)/db_server: $(BUILD_DIR)/main.o $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/db_client: $(BUILD_DIR)/client_main.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/run_tests: $(TEST_OBJS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/main.o: src/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/client_main.o: client/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Itests -c $< -o $@

test: $(BUILD_DIR)/run_tests
	./$(BUILD_DIR)/run_tests

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
