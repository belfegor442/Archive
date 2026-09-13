#include "LoginOverlay.hpp"

#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Monix {
namespace Security {

static int Sc(int base, int dpiScale) {
  return (std::max<int>)(1, static_cast<int>(base * (dpiScale / 96.0)));
}

static HFONT LoadBootFont(HDC dc, int dpiScale, int* outHeight) {
  static HFONT cachedFont = nullptr;
  static int cachedDpi = 0;
  static int cachedHeight = 0;

  if (cachedFont && cachedDpi == dpiScale) {
    if (outHeight) *outHeight = cachedHeight;
    return cachedFont;
  }

  if (cachedFont) {
    DeleteObject(cachedFont);
    cachedFont = nullptr;
  }

  wchar_t fontPath[MAX_PATH];
  GetModuleFileNameW(nullptr, fontPath, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(fontPath, L'\\');
  if (lastSlash) {
    wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - fontPath + 1), L"PhoenixEGA 8x8-2y.ttf");
    if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
      AddFontResourceExW(fontPath, FR_PRIVATE, 0);
      int fontSize = Sc(22, dpiScale);
      HFONT font = CreateFontW(
        fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Perfect DOS VGA 437 Win");
      if (font) {
        TEXTMETRICW tm{};
        HFONT old = (HFONT)SelectObject(dc, font);
        GetTextMetricsW(dc, &tm);
        SelectObject(dc, old);
        cachedFont = font;
        cachedDpi = dpiScale;
        cachedHeight = tm.tmHeight + tm.tmExternalLeading;
        if (outHeight) *outHeight = cachedHeight;
        return font;
      }
    }
  }

  int fontSize = Sc(22, dpiScale);
  HFONT font = CreateFontW(
    fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Terminal");
  if (!font) {
    font = CreateFontW(
      fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Fixedsys");
  }
  if (!font) {
    font = CreateFontW(
      fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
  }

  if (font && outHeight) {
    TEXTMETRICW tm{};
    HFONT old = (HFONT)SelectObject(dc, font);
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    cachedHeight = tm.tmHeight + tm.tmExternalLeading;
    *outHeight = cachedHeight;
  }
  cachedFont = font;
  cachedDpi = dpiScale;
  cachedHeight = cachedHeight ? cachedHeight : (fontSize + 4);
  return font;
}

static void DrawTextAt(HDC dc, int x, int y, const wchar_t* text, COLORREF color, HFONT font) {
  SetTextColor(dc, color);
  SetBkMode(dc, TRANSPARENT);
  SelectObject(dc, font);
  SetTextAlign(dc, TA_LEFT | TA_TOP);
  ExtTextOutW(dc, x, y, 0, nullptr, text, static_cast<int>(wcslen(text)), nullptr);
}

void LoginOverlay::Draw(HDC dc, const RECT& clientRect, const AuthManager& auth,
                        HFONT titleFont, HFONT bodyFont, HFONT smallFont,
                        int dpiScale, const std::wstring& userId) const {
  const int W = clientRect.right - clientRect.left;
  const int H = clientRect.bottom - clientRect.top;

  const ULONGLONG now = GetTickCount64();
  const ULONGLONG elapsed = now - createdTick_;

  int fontHeight = 0;
  HFONT mono = LoadBootFont(dc, dpiScale, &fontHeight);
  if (!mono) mono = bodyFont;
  const int lh = fontHeight > 0 ? fontHeight : Sc(26, dpiScale);
  int cw = Sc(13, dpiScale);

  const int maxContentChars = 80;
  const int minContentChars = 60;
  const int totalChars = std::clamp<int>((W - Sc(8, dpiScale)) / (std::max<int>)(1, cw), minContentChars, maxContentChars);
  const int bx = (W - totalChars * cw) / 2;
  const int startY = (std::max<int>)(Sc(8, dpiScale), (H - 12 * lh) / 2);

  const bool locked = auth.IsLockedOut();
  const bool noHash = auth.GetConfig().storedHash.empty();

  constexpr COLORREF C_WHITE   = RGB(255, 255, 255);
  constexpr COLORREF C_YELLOW  = RGB(255, 255, 0);
  constexpr COLORREF C_CYAN    = RGB(0, 255, 255);
  constexpr COLORREF C_GREEN   = RGB(0, 255, 0);
  constexpr COLORREF C_RED     = RGB(255, 0, 0);
  constexpr COLORREF C_DKGRAY  = RGB(85, 85, 85);
  constexpr COLORREF C_BLUESCR = RGB(0, 0, 170);

  {
    HBRUSH br = CreateSolidBrush(C_BLUESCR);
    FillRect(dc, &clientRect, br);
    DeleteObject(br);
  }

  auto drawLine = [&](int row, const wchar_t* text, COLORREF color) {
    DrawTextAt(dc, bx, startY + row * lh, text, color, mono);
  };

  int row = 0;

  drawLine(row, L"MONIX Security Terminal v2.6.2", C_CYAN);
  row += 2;

  if (locked) {
    drawLine(row, L"  *** ACCOUNT LOCKED - Too many failed attempts ***", C_RED);
  } else if (elapsed < T_LOGIN) {
    int dots = static_cast<int>(elapsed / 200) % 4;
    wchar_t loading[32];
    _snwprintf_s(loading, _countof(loading), _TRUNCATE, L"  Initializing system%.*s", dots, L"...");
    drawLine(row, loading, C_DKGRAY);
  } else {
    drawLine(row, L"  Password:", C_WHITE);
    row++;

    wchar_t pwdLine[80];
    int inputLen = static_cast<int>(inputBuffer_.size());
    int maxShow = 40;
    int showLen = (std::min<int>)(inputLen, maxShow);

    if (showPassword_) {
      wchar_t pwdBuf[64];
      int copyLen = (std::min<int>)(inputLen, maxShow);
      wcsncpy_s(pwdBuf, inputBuffer_.c_str(), copyLen);
      pwdBuf[copyLen] = 0;
      _snwprintf_s(pwdLine, _countof(pwdLine), _TRUNCATE, L"  [%s%.*s]", pwdBuf, maxShow - showLen, L"                                        ");
    } else {
      wchar_t dotsBuf[64];
      for (int i = 0; i < showLen; i++) dotsBuf[i] = 0x2022;
      dotsBuf[showLen] = 0;
      _snwprintf_s(pwdLine, _countof(pwdLine), _TRUNCATE, L"  [%s%.*s]", dotsBuf, maxShow - showLen, L"                                        ");
    }
    drawLine(row, pwdLine, C_WHITE);

    if (showLen < maxShow && ((now / 500) % 2 == 0)) {
      int cursorX = bx + (2 + 1 + showLen) * cw;
      int cursorY = startY + row * lh;
      SetTextColor(dc, C_WHITE);
      SetBkMode(dc, OPAQUE);
      SelectObject(dc, mono);
      wchar_t cursor = 0x2588;
      ExtTextOutW(dc, cursorX, cursorY, 0, nullptr, &cursor, 1, nullptr);
    }
  }

  row += 2;

  if (!lastError_.empty()) {
    drawLine(row, lastError_.c_str(), C_RED);
  } else if (noHash) {
    drawLine(row, L"  First boot - enter new password and press Enter", C_YELLOW);
  } else if (!locked && elapsed >= T_LOGIN) {
    drawLine(row, L"  TAB: Show/Hide  |  Ctrl+R: Reset  |  Enter: Confirm", C_DKGRAY);
  }
}

LoginOverlay::ButtonId LoginOverlay::HitTest(const RECT&, POINT, int) const {
  return ButtonId::None;
}

LoginOverlayResult LoginOverlay::HandleClick(const RECT&, POINT, int) const {
  return {};
}

LoginOverlayResult LoginOverlay::HandleChar(WPARAM wParam, AuthManager& auth) {
  LoginOverlayResult result;
  if (auth.GetState() != AuthState::LoginOverlay && auth.GetState() != AuthState::LockedOut)
    return result;
  if (GetTickCount64() - createdTick_ < T_LOGIN) return result;
  if (confirmReset_) return result;
  if (auth.IsLockedOut()) return result;
  if (wParam >= 0x20 && wParam != 0x7f) {
    if (inputBuffer_.size() < 128) {
      inputBuffer_.push_back(static_cast<wchar_t>(wParam));
    }
    lastError_.clear();
  }
  return result;
}

LoginOverlayResult LoginOverlay::HandleReturn(AuthManager& auth) {
  LoginOverlayResult result;
  if (confirmReset_) {
    auth.ResetPassword();
    inputBuffer_.clear();
    lastError_.clear();
    confirmReset_ = false;
    attemptCount_ = 0;
    result.passwordReset = true;
    return result;
  }
  if (confirmingPassword_) {
    if (inputBuffer_ == pendingPassword_) {
      confirmingPassword_ = false;
      pendingPassword_.clear();
      std::wstring msg;
      bool ok = auth.HandlePasswordEntry(inputBuffer_, msg);
      attemptCount_++;
      if (ok) {
        inputBuffer_.clear();
        lastError_.clear();
        result.loginAttempted = true;
      } else {
        inputBuffer_.clear();
        lastError_ = msg;
      }
    } else {
      lastError_ = L"Passwords do not match. Try again.";
      inputBuffer_.clear();
      pendingPassword_.clear();
      confirmingPassword_ = false;
    }
    return result;
  }
  if (auth.GetConfig().storedHash.empty() && !inputBuffer_.empty()) {
    confirmingPassword_ = true;
    pendingPassword_ = inputBuffer_;
    inputBuffer_.clear();
    lastError_.clear();
    return result;
  }
  std::wstring msg;
  bool ok = auth.HandlePasswordEntry(inputBuffer_, msg);
  attemptCount_++;
  if (ok) {
    inputBuffer_.clear();
    lastError_.clear();
    result.loginAttempted = true;
  } else {
    inputBuffer_.clear();
    lastError_ = msg;
  }
  return result;
}

LoginOverlayResult LoginOverlay::HandleKeyDown(WPARAM wParam, AuthManager& auth) {
  LoginOverlayResult result;
  if (auth.GetState() != AuthState::LoginOverlay && auth.GetState() != AuthState::LockedOut)
    return result;

  const ULONGLONG elapsed = GetTickCount64() - createdTick_;
  if (elapsed < T_LOGIN) {
    if (wParam == VK_ESCAPE) result.loginCancelled = true;
    return result;
  }

  if (wParam == VK_TAB) {
    if (!confirmReset_) showPassword_ = !showPassword_;
    return result;
  }

  if (wParam == VK_RETURN) {
    return HandleReturn(auth);
  }

  if (wParam == VK_ESCAPE) {
    if (confirmReset_) {
      confirmReset_ = false;
      lastError_.clear();
      return result;
    }
    if (confirmingPassword_) {
      confirmingPassword_ = false;
      pendingPassword_.clear();
      lastError_.clear();
      inputBuffer_.clear();
      return result;
    }
    inputBuffer_.clear();
    lastError_.clear();
    showPassword_ = false;
    result.loginCancelled = true;
    return result;
  }

  if (wParam == VK_BACK) {
    if (confirmReset_ || confirmingPassword_) return result;
    if (!inputBuffer_.empty()) inputBuffer_.pop_back();
    return result;
  }

  if (wParam == 'Y' && confirmReset_) {
    auth.ResetPassword();
    inputBuffer_.clear();
    lastError_.clear();
    confirmReset_ = false;
    attemptCount_ = 0;
    result.passwordReset = true;
    return result;
  }

  if (wParam == 'N' && confirmReset_) {
    confirmReset_ = false;
    lastError_.clear();
    return result;
  }

  if (wParam == 'R' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
    if (!auth.GetConfig().storedHash.empty()) {
      confirmReset_ = true;
      inputBuffer_.clear();
      lastError_.clear();
    }
    return result;
  }

  return result;
}

} // namespace Security
} // namespace Monix
