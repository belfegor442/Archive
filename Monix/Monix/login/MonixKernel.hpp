#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../../../src/native/kernel/KernelState.hpp"
#include "../../../src/native/kernel/KernelEvent.hpp"
#include "../../../src/native/kernel/KernelDiagnostics.hpp"
#include "AuthManager.hpp"

namespace Monix {
namespace Kernel {

struct KernelTestResult {
  std::size_t groupIndex = 0;
  std::size_t testIndex = 0;
  monix::kernel::TestSeverity severity = monix::kernel::TestSeverity::Skipped;
  const char* detail = nullptr;
  std::uint32_t eventId = 0;
};

enum class KernelPhase {
  Startup,
  Diagnostics,
  Ready,
  Authenticating,
  AuthSuccess,
  LoadingEnvironment,
  LoadingGui,
  Running,
  Shutdown,
  Halted,
  Crash
};

class MonixKernel {
public:
  MonixKernel();

  void Initialize(Security::AuthManager* auth);
  bool Update();
  bool HandleChar(WPARAM wParam);
  bool HandleKeyDown(WPARAM wParam, bool& outLoggedIn, bool& outShutdown);

  KernelPhase GetPhase() const { return phase_; }
  bool IsRunning() const { return phase_ == KernelPhase::Running; }
  bool IsHalted() const { return phase_ == KernelPhase::Halted || phase_ == KernelPhase::Crash; }

  const std::vector<KernelTestResult>& GetResults() const { return results_; }
  const std::vector<monix::kernel::DiagnosticGroup>& GetGroups() const { return diag_.Groups(); }
  std::size_t GetCurrentGroup() const { return currentGroup_; }
  std::size_t GetCurrentTest() const { return currentTest_; }
  bool IsDiagnosticsComplete() const { return diagnosticsComplete_; }
  std::uint32_t GetNextEventId() const { return events_.NextId(); }
  std::uint32_t GetEventCount() const { return events_.Count(); }
  std::uint32_t GetPassCount() const { return passCount_; }
  std::uint32_t GetFailCount() const { return failCount_; }
  std::uint32_t GetWarnCount() const { return warnCount_; }
  std::uint32_t GetSkipCount() const { return skipCount_; }

  const std::wstring& GetUsername() const { return username_; }
  const std::wstring& GetPassword() const { return password_; }
  bool IsPasswordVisible() const { return passwordVisible_; }
  bool IsAuthLocked() const;
  int GetAuthAttempts() const { return authAttempts_; }
  const std::wstring& GetAuthError() const { return authError_; }
  const std::wstring& GetStatusMessage() const { return statusMessage_; }
  int GetLoadingStep() const { return loadingStep_; }
  bool IsLoadingComplete() const { return loadingComplete_; }

  const char* GetCrashSubsystem() const { return crashSubsystem_; }
  const char* GetCrashTest() const { return crashTest_; }
  const char* GetCrashDetail() const { return crashDetail_; }
  ULONGLONG GetBootTimeMs() const { return bootTimeMs_; }

  void SkipTests();
  bool AreTestsSkipped() const { return testsSkipped_; }

  const monix::kernel::KernelEventEngine& GetEventEngine() const { return events_; }

private:
  void RunNextTest();
  void TransitionToReady();
  void ExecuteDiagnosticsPhase();
  void ExecuteReadyPhase();
  void ExecuteAuthPhase();
  void ExecuteAuthSuccessPhase();
  void ExecuteLoadingPhase();
  void ExecuteShutdownPhase();
  void TriggerCrash(const char* subsystem, const char* error, const char* description);

  Security::AuthManager* auth_ = nullptr;
  monix::kernel::KernelDiagnostics diag_;
  monix::kernel::KernelEventEngine events_;

  KernelPhase phase_ = KernelPhase::Startup;

  std::vector<KernelTestResult> results_;
  std::size_t currentGroup_ = 0;
  std::size_t currentTest_ = 0;
  bool diagnosticsComplete_ = false;

  std::uint32_t passCount_ = 0;
  std::uint32_t failCount_ = 0;
  std::uint32_t warnCount_ = 0;
  std::uint32_t skipCount_ = 0;

  std::wstring username_;
  std::wstring password_;
  bool passwordVisible_ = false;
  int authAttempts_ = 0;
  std::wstring authError_;
  std::wstring statusMessage_;

  ULONGLONG phaseStartMs_ = 0;
  ULONGLONG lastTestMs_ = 0;
  ULONGLONG testIntervalMs_ = DEFAULT_TEST_INTERVAL_MS;

  static constexpr ULONGLONG DEFAULT_TEST_INTERVAL_MS = 20;

  bool testsSkipped_ = false;

  int loadingStep_ = 0;
  bool loadingComplete_ = false;

  const char* crashSubsystem_ = nullptr;
  const char* crashTest_ = nullptr;
  const char* crashDetail_ = nullptr;
  ULONGLONG bootTimeMs_ = 0;
};

} // namespace Kernel
} // namespace Monix
