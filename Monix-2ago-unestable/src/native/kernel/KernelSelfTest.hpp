#pragma once

#include "KernelEvent.hpp"
#include "KernelState.hpp"
#include <vector>
#include <functional>

namespace monix {
namespace kernel {

struct DiagnosticTest {
  const char* name = nullptr;
  EventSubsystem subsystem = EventSubsystem::Kernel;
  bool critical = false;
  std::function<TestResult()> runner;

  DiagnosticTest() = default;
  DiagnosticTest(const char* n, EventSubsystem sub, bool crit)
    : name(n), subsystem(sub), critical(crit) {}
  DiagnosticTest(const char* n, EventSubsystem sub, bool crit, std::function<TestResult()> r)
    : name(n), subsystem(sub), critical(crit), runner(std::move(r)) {}
};

struct DiagnosticGroup {
  const char* title = nullptr;
  std::vector<DiagnosticTest> tests;

  DiagnosticGroup() = default;
  DiagnosticGroup(const char* t, std::vector<DiagnosticTest> ts)
    : title(t), tests(std::move(ts)) {}
};

class KernelSelfTest {
public:
  static KernelSelfTest& Instance();

  void RegisterGroups();
  const std::vector<DiagnosticGroup>& GetGroups() const { return groups_; }
  std::size_t TotalTests() const;

private:
  KernelSelfTest() = default;
  std::vector<DiagnosticGroup> groups_;
};

}
}
