#pragma once

// Minimal single-header test framework: enough for assertion-style unit
// tests without pulling in a third-party dependency at this stage.

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    std::string name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, void (*fn)()) {
        registry().push_back({name, fn});
    }
};

inline int g_failures = 0;
inline std::string g_current_test;

inline int run_all() {
    int passed = 0;
    for (const auto& test : registry()) {
        g_current_test = test.name;
        int failures_before = g_failures;
        try {
            test.fn();
        } catch (const std::exception& e) {
            std::cerr << "FAIL [" << test.name << "] uncaught exception: " << e.what() << "\n";
            ++g_failures;
        }
        if (g_failures == failures_before) {
            ++passed;
        }
    }
    std::cout << passed << "/" << registry().size() << " tests passed\n";
    return g_failures == 0 ? 0 : 1;
}

} // namespace testfw

#define TEST_CASE(name)                                                                                              \
    static void name();                                                                                             \
    static ::testfw::Registrar registrar_##name(#name, &name);                                                      \
    static void name()

#define REQUIRE(condition)                                                                                           \
    do {                                                                                                             \
        if (!(condition)) {                                                                                          \
            std::cerr << "FAIL [" << ::testfw::g_current_test << "] " << __FILE__ << ":" << __LINE__ << ": "         \
                       << #condition << "\n";                                                                        \
            ++::testfw::g_failures;                                                                                  \
        }                                                                                                            \
    } while (false)

#define REQUIRE_THROWS(expression)                                                                                   \
    do {                                                                                                             \
        bool threw = false;                                                                                          \
        try {                                                                                                        \
            (void)(expression);                                                                                      \
        } catch (...) {                                                                                              \
            threw = true;                                                                                            \
        }                                                                                                            \
        if (!threw) {                                                                                                \
            std::cerr << "FAIL [" << ::testfw::g_current_test << "] " << __FILE__ << ":" << __LINE__                \
                      << ": expected exception from " << #expression << "\n";                                        \
            ++::testfw::g_failures;                                                                                  \
        }                                                                                                            \
    } while (false)
