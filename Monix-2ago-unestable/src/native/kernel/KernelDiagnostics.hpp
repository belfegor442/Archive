#pragma once

#include <vector>
#include "KernelState.hpp"
#include "KernelEvent.hpp"
#include "KernelSelfTest.hpp"

namespace monix {
namespace kernel {

class KernelDiagnostics {
public:
  KernelDiagnostics();
  const std::vector<DiagnosticGroup>& Groups() const { return groups_; }
  std::size_t TotalTests() const;
  bool HasFatal() const { return hasFatal_; }
  bool HasCriticalFail() const { return hasCriticalFail_; }

  TestResult RunTest(std::size_t groupIndex, std::size_t testIndex,
                     KernelEventEngine& events);

  void SetTestResult(std::size_t groupIndex, std::size_t testIndex,
                     TestSeverity severity, const char* detail = nullptr) {
    if (groupIndex < results_.size() && testIndex < results_[groupIndex].size()) {
      results_[groupIndex][testIndex] = { severity, detail };
      if (severity == TestSeverity::Fatal) hasFatal_ = true;
      if (severity == TestSeverity::Fail && groups_[groupIndex].tests[testIndex].critical)
        hasCriticalFail_ = true;
    }
  }

  TestSeverity GetTestSeverity(std::size_t g, std::size_t t) const {
    if (g < results_.size() && t < results_[g].size())
      return results_[g][t].severity;
    return TestSeverity::Skipped;
  }

  const char* GetTestDetail(std::size_t g, std::size_t t) const {
    if (g < results_.size() && t < results_[g].size())
      return results_[g][t].detail;
    return nullptr;
  }

private:
  void BuildTestGroups();
  std::vector<DiagnosticGroup> groups_;
  struct ResultEntry { TestSeverity severity = TestSeverity::Skipped; const char* detail = nullptr; };
  std::vector<std::vector<ResultEntry>> results_;
  bool hasFatal_ = false;
  bool hasCriticalFail_ = false;
};

}
}
