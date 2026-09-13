#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdio>

namespace monix {
namespace tests {

struct TestCase {
    std::string name;
    std::function<bool()> fn;
};

struct TestResult {
    std::string name;
    bool passed;
    std::string error;
};

inline std::vector<TestResult>& allResults() {
    static std::vector<TestResult> results;
    return results;
}

inline void reportResult(const std::string& name, bool passed, const std::string& error = "") {
    allResults().push_back({name, passed, error});
    if (passed) {
        fprintf(stdout, "  [PASS] %s\n", name.c_str());
    } else {
        fprintf(stdout, "  [FAIL] %s: %s\n", name.c_str(), error.c_str());
    }
    fflush(stdout);
}

inline void printSummary() {
    const auto& results = allResults();
    int pass = 0, fail = 0;
    for (const auto& r : results) {
        if (r.passed) ++pass; else ++fail;
    }
    fprintf(stdout, "\n=== Test Summary: %d passed, %d failed, %d total ===\n",
            pass, fail, static_cast<int>(results.size()));
    for (const auto& r : results) {
        fprintf(stdout, "  %s %s\n", r.passed ? "PASS" : "FAIL", r.name.c_str());
        if (!r.passed && !r.error.empty()) {
            fprintf(stdout, "    Cause: %s\n", r.error.c_str());
        }
    }
    fflush(stdout);
}

}  // namespace tests
}  // namespace monix

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { monix::tests::reportResult(testName, false, msg); return false; } } while(0)

#define TEST_ASSERT_EQ(a, b, msg) \
    do { if ((a) != (b)) { monix::tests::reportResult(testName, false, msg); return false; } } while(0)

#define RUN_VKTEST(fn) \
    do { \
        std::string testName = #fn; \
        bool passed = false; \
        std::string error; \
        try { passed = fn(); } \
        catch (const std::exception& e) { error = e.what(); } \
        catch (...) { error = "exception"; } \
        monix::tests::reportResult(testName, passed, error); \
    } while(0)
