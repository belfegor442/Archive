#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include "AuthManager.hpp"
#include "../../../src/native/kernel/KernelDiagnostics.hpp"
#include "../../../src/native/kernel/KernelEvent.hpp"

namespace Monix {
namespace Security {

class DiagnosticsDisplay {
public:
  DiagnosticsDisplay() = default;

  static constexpr ULONGLONG DIAG_DURATION_MS = 4500;

  void Draw(HDC dc, const RECT& clientRect, const AuthManager& auth,
            HFONT titleFont, HFONT bodyFont, HFONT smallFont,
            int dpiScale, const std::wstring& userId) const;

  bool IsComplete() const;

private:
  void DetectHardware() const;
  void RunTests() const;

  mutable bool detected_ = false;
  mutable bool testsRun_ = false;
  mutable bool started_ = false;
  mutable ULONGLONG startTick_ = 0;

  mutable std::wstring cpuName_;
  mutable std::wstring cpuCores_;
  mutable std::wstring totalRam_;
  mutable std::wstring hostname_;

  mutable monix::kernel::KernelDiagnostics diag_;
  mutable monix::kernel::KernelEventEngine events_;
  mutable int passCount_ = 0;
  mutable int failCount_ = 0;
  mutable int warnCount_ = 0;
  mutable bool hasFatal_ = false;
};

} // namespace Security
} // namespace Monix
