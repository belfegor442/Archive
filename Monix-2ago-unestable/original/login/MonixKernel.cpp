#include "MonixKernel.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <setupapi.h>
#include <intrin.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "setupapi.lib")

#include "../../sensors/sensors.h"
#include "../../src/native/telemetry/Collectors.hpp"

namespace Monix {
namespace Kernel {

static constexpr ULONGLONG READY_DELAY_MS = 1000;
static constexpr ULONGLONG AUTH_SUCCESS_DELAY_MS = 800;
static constexpr ULONGLONG LOADING_STEP_MS = 100;
static constexpr int LOADING_TOTAL_STEPS = 6;

static constexpr wchar_t ASCII_PRINTABLE_MIN = 0x20;
static constexpr wchar_t ASCII_DELETE = 0x7f;

MonixKernel::MonixKernel() {
  diag_.Groups();
}

void MonixKernel::Initialize(Security::AuthManager* auth) {
  auth_ = auth;
  phase_ = KernelPhase::Diagnostics;
  phaseStartMs_ = GetTickCount64();
  lastTestMs_ = 0;
  events_.Clear();
  results_.clear();
  currentGroup_ = 0;
  currentTest_ = 0;
  diagnosticsComplete_ = false;
  passCount_ = 0;
  failCount_ = 0;
  warnCount_ = 0;
  skipCount_ = 0;
  loadingStep_ = 0;
  loadingComplete_ = false;
  username_.clear();
  password_.clear();
  passwordVisible_ = false;
  authAttempts_ = 0;
  authError_.clear();
  statusMessage_ = L"Iniciando...";
}

void MonixKernel::TransitionToReady() {
  diagnosticsComplete_ = true;
  statusMessage_ = L"MONIX KERNEL READY";
  phase_ = KernelPhase::Ready;
  phaseStartMs_ = GetTickCount64();
}

void MonixKernel::RunNextTest() {
  const auto& groups = diag_.Groups();
  if (currentGroup_ >= groups.size()) {
    TransitionToReady();
    return;
  }

  const auto& group = groups[currentGroup_];
  if (currentTest_ >= group.tests.size()) {
    currentGroup_++;
    currentTest_ = 0;
    if (currentGroup_ >= groups.size()) {
      TransitionToReady();
    }
    return;
  }

  const auto& test = group.tests[currentTest_];
  const auto result = diag_.RunTest(currentGroup_, currentTest_, events_);

  std::uint32_t eventId = events_.NextId() - 1;

  KernelTestResult kr;
  kr.groupIndex = currentGroup_;
  kr.testIndex = currentTest_;
  kr.severity = result.severity;
  kr.detail = result.detail;
  kr.eventId = eventId;

  diag_.SetTestResult(currentGroup_, currentTest_, result.severity, result.detail);
  results_.push_back(kr);

  switch (result.severity) {
    case monix::kernel::TestSeverity::Pass: passCount_++; break;
    case monix::kernel::TestSeverity::Warning: warnCount_++; break;
    case monix::kernel::TestSeverity::Fail: failCount_++; break;
    case monix::kernel::TestSeverity::Skipped: skipCount_++; break;
    case monix::kernel::TestSeverity::Fatal:
      failCount_++;
      if (test.critical) {
        TriggerCrash(
          monix::kernel::KernelEventEngine::SubsystemName(test.subsystem),
          test.name, result.detail ? result.detail : "Fatal test failure");
        return;
      }
      break;
  }

  currentTest_++;
  lastTestMs_ = GetTickCount64();
}

void MonixKernel::ExecuteDiagnosticsPhase() {
  const ULONGLONG now = GetTickCount64();
  if (now - lastTestMs_ < testIntervalMs_) return;
  RunNextTest();
}

void MonixKernel::ExecuteReadyPhase() {
  if (GetTickCount64() - phaseStartMs_ >= READY_DELAY_MS) {
    phase_ = KernelPhase::Authenticating;
    phaseStartMs_ = GetTickCount64();
    if (auth_) auth_->ShowLogin();
    statusMessage_ = L"AUTHENTICATION";
  }
}

void MonixKernel::ExecuteAuthPhase() {
  if (!auth_) {
    phase_ = KernelPhase::AuthSuccess;
    phaseStartMs_ = GetTickCount64();
    statusMessage_ = L"Authentication successful";
  }
}

void MonixKernel::ExecuteAuthSuccessPhase() {
  if (GetTickCount64() - phaseStartMs_ >= AUTH_SUCCESS_DELAY_MS) {
    phase_ = KernelPhase::LoadingEnvironment;
    phaseStartMs_ = GetTickCount64();
    loadingStep_ = 0;
    statusMessage_ = L"Loading user environment...";
  }
}

void MonixKernel::ExecuteLoadingPhase() {
  const ULONGLONG now = GetTickCount64();
  if (now - phaseStartMs_ >= LOADING_STEP_MS) {
    loadingStep_++;
    phaseStartMs_ = now;
    if (loadingStep_ >= LOADING_TOTAL_STEPS) {
      loadingComplete_ = true;
      statusMessage_ = L"Starting MONIX GUI...";
    }
  }
}

void MonixKernel::ExecuteShutdownPhase() {
  statusMessage_ = L"SHUTDOWN";
}

void MonixKernel::TriggerCrash(const char* subsystem, const char* error, const char* description) {
  phase_ = KernelPhase::Crash;
  events_.Emit(monix::kernel::EventSubsystem::Kernel, monix::kernel::EventSeverity::Fatal, error, description);
  statusMessage_ = L"FATAL SYSTEM ERROR";
}

bool MonixKernel::Update() {
  switch (phase_) {
    case KernelPhase::Startup:
    case KernelPhase::Diagnostics:
      ExecuteDiagnosticsPhase();
      break;
    case KernelPhase::Ready:
      ExecuteReadyPhase();
      break;
    case KernelPhase::Authenticating:
      ExecuteAuthPhase();
      break;
    case KernelPhase::AuthSuccess:
      ExecuteAuthSuccessPhase();
      break;
    case KernelPhase::LoadingEnvironment:
    case KernelPhase::LoadingGui:
      ExecuteLoadingPhase();
      if (loadingComplete_) {
        phase_ = KernelPhase::Running;
      }
      break;
    default:
      break;
  }
  return phase_ != KernelPhase::Running && phase_ != KernelPhase::Halted;
}

bool MonixKernel::HandleChar(WPARAM wParam) {
  if (phase_ != KernelPhase::Authenticating) return false;
  if (wParam >= ASCII_PRINTABLE_MIN && wParam != ASCII_DELETE) {
    password_.push_back(static_cast<wchar_t>(wParam));
    authError_.clear();
  }
  return true;
}

bool MonixKernel::HandleKeyDown(WPARAM wParam, bool& outLoggedIn, bool& outShutdown) {
  outLoggedIn = false;
  outShutdown = false;

  if (phase_ == KernelPhase::Authenticating) {
    if (wParam == VK_TAB) {
      passwordVisible_ = !passwordVisible_;
      return true;
    }
    if (wParam == VK_BACK) {
      if (!password_.empty()) password_.pop_back();
      return true;
    }
    if (wParam == VK_RETURN) {
      if (!auth_) return true;
      std::wstring msg;
      bool ok = auth_->HandlePasswordEntry(password_, msg);
      authAttempts_++;
      if (ok) {
        phase_ = KernelPhase::AuthSuccess;
        phaseStartMs_ = GetTickCount64();
        statusMessage_ = L"Authentication successful";
        authError_.clear();
        outLoggedIn = true;
      } else {
        authError_ = msg;
        password_.clear();
      }
      return true;
    }
    if (wParam == VK_ESCAPE) {
      outShutdown = true;
      return true;
    }
    return true;
  }

  if (phase_ == KernelPhase::Running) {
    if (wParam == VK_F10) {
      outShutdown = true;
      return true;
    }
  }

  return false;
}

bool MonixKernel::IsAuthLocked() const {
  return auth_ && auth_->IsLockedOut();
}

} // namespace Kernel
} // namespace Monix
