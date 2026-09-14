#include "KernelSelfTest.hpp"
#include "KernelDiagnostics.hpp"
#include <cstring>
#include <functional>
#include <cstddef>

// Common struct types matching test file definitions (global scope)
struct KTestEntry { const char* name; void (*func)(); };
struct KBoolTestEntry { const char* name; bool (*func)(); };

#ifdef MONIX_KERNEL_TEST_BUILD
// Pattern A: void test_* with gPassed/gFailed (C++ linkage)
extern const KTestEntry* GetKTests_EventCore();
extern std::size_t GetKTestCount_EventCore();

extern const KTestEntry* GetKTests_EventBus();
extern std::size_t GetKTestCount_EventBus();

extern const KTestEntry* GetKTests_Validation();
extern std::size_t GetKTestCount_Validation();

extern const KTestEntry* GetKTests_Collector();
extern std::size_t GetKTestCount_Collector();

extern const KTestEntry* GetKTests_CollectorConfig();
extern std::size_t GetKTestCount_CollectorConfig();

extern const KTestEntry* GetKTests_Filesystem();
extern std::size_t GetKTestCount_Filesystem();

extern const KTestEntry* GetKTests_FilesystemScope();
extern std::size_t GetKTestCount_FilesystemScope();

extern const KTestEntry* GetKTests_ProcessCollector();
extern std::size_t GetKTestCount_ProcessCollector();

extern const KTestEntry* GetKTests_UserSessionCollector();
extern std::size_t GetKTestCount_UserSessionCollector();

extern const KTestEntry* GetKTests_ScriptCollector();
extern std::size_t GetKTestCount_ScriptCollector();

extern const KTestEntry* GetKTests_DeviceCollector();
extern std::size_t GetKTestCount_DeviceCollector();

extern const KTestEntry* GetKTests_StorageCollector();
extern std::size_t GetKTestCount_StorageCollector();

extern const KTestEntry* GetKTests_NetworkCollector();
extern std::size_t GetKTestCount_NetworkCollector();

// Pattern B: bool test_* (C++ linkage)
extern const KBoolTestEntry* GetKBoolTests_SensorCollector();
extern std::size_t GetKBoolTestCount_SensorCollector();

extern const KBoolTestEntry* GetKBoolTests_SnapshotEngine();
extern std::size_t GetKBoolTestCount_SnapshotEngine();

extern const KBoolTestEntry* GetKBoolTests_Health();
extern std::size_t GetKBoolTestCount_Health();

extern const KBoolTestEntry* GetKBoolTests_Phases23_26();
extern std::size_t GetKBoolTestCount_Phases23_26();

extern const KBoolTestEntry* GetKBoolTests_Phases27_29();
extern std::size_t GetKBoolTestCount_Phases27_29();

extern const KBoolTestEntry* GetKBoolTests_Phases30_33();
extern std::size_t GetKBoolTestCount_Phases30_33();

extern const KBoolTestEntry* GetKBoolTests_Phases34_39();
extern std::size_t GetKBoolTestCount_Phases34_39();

extern const KBoolTestEntry* GetKBoolTests_Phases40_44();
extern std::size_t GetKBoolTestCount_Phases40_44();

extern const KBoolTestEntry* GetKBoolTests_Phases45_49();
extern std::size_t GetKBoolTestCount_Phases45_49();

extern const KBoolTestEntry* GetKBoolTests_ServiceCollector();
extern std::size_t GetKBoolTestCount_ServiceCollector();

extern const KBoolTestEntry* GetKBoolTests_DriverCollector();
extern std::size_t GetKBoolTestCount_DriverCollector();

extern const KBoolTestEntry* GetKBoolTests_ConfigCollector();
extern std::size_t GetKBoolTestCount_ConfigCollector();
#endif // MONIX_KERNEL_TEST_BUILD

namespace monix {
namespace kernel {

static bool RunVoidTest(void (*fn)()) {
  fn();
  return true;
}

static bool RunBoolTest(bool (*fn)()) {
  return fn();
}

static bool IsSlowTest(const char* name) {
  if (!name) return false;
  if (strstr(name, "Perf:")) return true;
  if (strstr(name, "Concurrency:")) return true;
  if (strstr(name, "Stress")) return true;
  if (strstr(name, "Storm")) return true;
  if (strstr(name, "Accept")) return true;
  if (strstr(name, "1M")) return true;
  for (const char* p = name; *p; ++p) {
    if (*p >= '1' && *p <= '9') {
      const char* q = p + 1;
      while (*q >= '0' && *q <= '9') ++q;
      if ((*q == 'K' || *q == 'k') && (q[1] == ' ' || q[1] == '\0' || q[1] == ')' || q[1] == ',')) return true;
    }
  }
  return false;
}

KernelSelfTest& KernelSelfTest::Instance() {
  static KernelSelfTest i;
  return i;
}

std::size_t KernelSelfTest::TotalTests() const {
  std::size_t t = 0;
  for (auto& g : groups_) t += g.tests.size();
  return t;
}

void KernelSelfTest::RegisterGroups() {
  groups_.clear();

#ifdef MONIX_KERNEL_TEST_BUILD

  // --- EVENT CORE ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_EventCore();
    std::size_t count = GetKTestCount_EventCore();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"EVENT CORE", std::move(tests)});
  }
  // --- EVENT BUS ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_EventBus();
    std::size_t count = GetKTestCount_EventBus();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"EVENT BUS", std::move(tests)});
  }
  // --- VALIDATION ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_Validation();
    std::size_t count = GetKTestCount_Validation();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"VALIDATION", std::move(tests)});
  }
  // --- COLLECTOR BASE ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_Collector();
    std::size_t count = GetKTestCount_Collector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"COLLECTOR BASE", std::move(tests)});
  }
  // --- COLLECTOR CONFIG ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_CollectorConfig();
    std::size_t count = GetKTestCount_CollectorConfig();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"COLLECTOR CONFIG", std::move(tests)});
  }
  // --- FILESYSTEM ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_Filesystem();
    std::size_t count = GetKTestCount_Filesystem();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Storage, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"FILESYSTEM", std::move(tests)});
  }
  // --- FILESCOPE ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_FilesystemScope();
    std::size_t count = GetKTestCount_FilesystemScope();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Storage, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"FILESCOPE", std::move(tests)});
  }
  // --- PROCESS ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_ProcessCollector();
    std::size_t count = GetKTestCount_ProcessCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Process, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"PROCESS", std::move(tests)});
  }
  // --- USER SESSION ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_UserSessionCollector();
    std::size_t count = GetKTestCount_UserSessionCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Security, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"USER SESSION", std::move(tests)});
  }
  // --- SCRIPT ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_ScriptCollector();
    std::size_t count = GetKTestCount_ScriptCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Process, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"SCRIPT", std::move(tests)});
  }
  // --- DEVICE ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_DeviceCollector();
    std::size_t count = GetKTestCount_DeviceCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Devices, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"DEVICE", std::move(tests)});
  }
  // --- STORAGE ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_StorageCollector();
    std::size_t count = GetKTestCount_StorageCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Storage, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"STORAGE", std::move(tests)});
  }
  // --- NETWORK ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKTests_NetworkCollector();
    std::size_t count = GetKTestCount_NetworkCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      void (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Network, false, [fn, nm]() -> TestResult { bool ok = RunVoidTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"NETWORK", std::move(tests)});
  }
  // --- SENSOR ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_SensorCollector();
    std::size_t count = GetKBoolTestCount_SensorCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Sensors, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"SENSOR", std::move(tests)});
  }
  // --- SNAPSHOT ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_SnapshotEngine();
    std::size_t count = GetKBoolTestCount_SnapshotEngine();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"SNAPSHOT", std::move(tests)});
  }
  // --- HEALTH ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Health();
    std::size_t count = GetKBoolTestCount_Health();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Error, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"HEALTH", std::move(tests)});
  }
  // --- CIRCUIT/CORR ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Phases23_26();
    std::size_t count = GetKBoolTestCount_Phases23_26();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"CIRCUIT/CORR", std::move(tests)});
  }
  // --- STORM/QUAR ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Phases27_29();
    std::size_t count = GetKBoolTestCount_Phases27_29();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"STORM/QUAR", std::move(tests)});
  }
  // --- CAMERA/TRUST ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Phases30_33();
    std::size_t count = GetKBoolTestCount_Phases30_33();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Devices, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"CAMERA/TRUST", std::move(tests)});
  }
  // --- STORAGE/RET ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Phases34_39();
    std::size_t count = GetKBoolTestCount_Phases34_39();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Storage, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"STORAGE/RET", std::move(tests)});
  }
  // --- RULE/CONTEXT ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Phases40_44();
    std::size_t count = GetKBoolTestCount_Phases40_44();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"RULE/CONTEXT", std::move(tests)});
  }
  // --- FAIL/PLAT/SEC ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_Phases45_49();
    std::size_t count = GetKBoolTestCount_Phases45_49();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"FAIL/PLAT/SEC", std::move(tests)});
  }
  // --- SERVICE ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_ServiceCollector();
    std::size_t count = GetKBoolTestCount_ServiceCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Kernel, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"SERVICE", std::move(tests)});
  }
  // --- DRIVER ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_DriverCollector();
    std::size_t count = GetKBoolTestCount_DriverCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Devices, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"DRIVER", std::move(tests)});
  }
  // --- CONFIG COLLECTOR ---
  {
    std::vector<DiagnosticTest> tests;
    auto* entries = GetKBoolTests_ConfigCollector();
    std::size_t count = GetKBoolTestCount_ConfigCollector();
    for (std::size_t i = 0; i < count; ++i) {
      const char* nm = entries[i].name;
      if (IsSlowTest(nm)) continue;
      bool (*fn)() = entries[i].func;
      tests.push_back({nm, EventSubsystem::Configuration, false, [fn, nm]() -> TestResult { bool ok = RunBoolTest(fn); return {nm, ok ? TestSeverity::Pass : TestSeverity::Fail, ok ? "ok" : "FAIL"}; }});
    }
    groups_.push_back({"CONFIG COLLECTOR", std::move(tests)});
  }
#endif // MONIX_KERNEL_TEST_BUILD
}

} // namespace kernel
} // namespace monix
