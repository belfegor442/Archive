#pragma once

namespace monix {
namespace kernel {

enum class KernelState {
  Boot,
  Initializing,
  Diagnostics,
  Ready,
  Authenticating,
  Authenticated,
  LoadingUserEnvironment,
  LoadingGui,
  Running,
  Error,
  Crash,
  Shutdown,
  Halted
};

enum class TestSeverity {
  Pass,
  Warning,
  Fail,
  Skipped,
  Fatal
};

struct TestResult {
  const char* name = nullptr;
  TestSeverity severity = TestSeverity::Pass;
  const char* detail = nullptr;
};

}
}
