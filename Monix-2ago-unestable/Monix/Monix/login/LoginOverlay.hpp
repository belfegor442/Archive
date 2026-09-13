#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include "AuthManager.hpp"

namespace Monix {
namespace Security {

struct LoginOverlayResult {
  bool loginAttempted = false;
  bool loginCancelled = false;
  bool passwordReset = false;
  bool shutdownRequested = false;
};

class LoginOverlay {
public:
  LoginOverlay() = default;

  static constexpr ULONGLONG T_LOGIN = 6000;

  void Draw(HDC dc, const RECT& clientRect, const AuthManager& auth,
            HFONT titleFont, HFONT bodyFont, HFONT smallFont,
            int dpiScale, const std::wstring& userId) const;

  LoginOverlayResult HandleClick(const RECT& clientRect, POINT point, int dpiScale) const;
  LoginOverlayResult HandleChar(WPARAM wParam, AuthManager& auth);
  LoginOverlayResult HandleKeyDown(WPARAM wParam, AuthManager& auth);

  void Reset() {
    inputBuffer_.clear();
    lastError_.clear();
    showPassword_ = false;
    confirmReset_ = false;
    confirmingPassword_ = false;
    pendingPassword_.clear();
    createdTick_ = GetTickCount64();
    attemptCount_ = 0;
    beepPlayed_ = false;
  }
  void ClearError() { lastError_.clear(); confirmReset_ = false; }
  const std::wstring& GetInputBuffer() const { return inputBuffer_; }
  const std::wstring& GetLastError() const { return lastError_; }

  enum class ButtonId { None, Login, Reset };

  ButtonId HitTest(const RECT& clientRect, POINT point, int dpiScale) const;

private:
  LoginOverlayResult HandleReturn(AuthManager& auth);
  std::wstring inputBuffer_;
  std::wstring lastError_;
  std::wstring pendingPassword_;
  bool showPassword_ = false;
  bool confirmReset_ = false;
  bool confirmingPassword_ = false;
  ULONGLONG createdTick_ = 0;
  mutable int attemptCount_ = 0;
  mutable bool beepPlayed_ = false;
};

} // namespace Security
} // namespace Monix
