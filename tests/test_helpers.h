#pragma once

#include <iostream>
#include <string>

namespace test {

inline int& tests_run() {
    static int count = 0;
    return count;
}

inline int& tests_passed() {
    static int count = 0;
    return count;
}

#define TEST_BEGIN(name) \
    do { \
        ::test::tests_run()++; \
        std::cout << "  " << name << "... "; \
    } while(0)

#define TEST_PASS() \
    do { \
        ::test::tests_passed()++; \
        std::cout << "OK" << std::endl; \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cout << "FAIL" << std::endl; \
            std::cerr << "    Assertion failed: " << #a << " == " << #b << std::endl; \
            return; \
        } \
    } while(0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cout << "FAIL" << std::endl; \
            std::cerr << "    Assertion failed: " << #expr << std::endl; \
            return; \
        } \
    } while(0)

#define ASSERT_THROW(expr) \
    do { \
        bool threw = false; \
        try { expr; } catch (...) { threw = true; } \
        if (!threw) { \
            std::cout << "FAIL" << std::endl; \
            std::cerr << "    Expected exception from: " << #expr << std::endl; \
            return; \
        } \
    } while(0)

inline int test_summary() {
    std::cout << std::endl;
    std::cout << tests_passed() << "/" << tests_run() << " tests passed" << std::endl;
    return (tests_passed() == tests_run()) ? 0 : 1;
}

} // namespace test
