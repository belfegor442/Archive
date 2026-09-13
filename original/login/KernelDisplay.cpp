#include "KernelDisplay.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Monix {
namespace Kernel {

static constexpr COLORREF COL_BG     = RGB(0, 8, 40);
static constexpr COLORREF COL_WHITE  = RGB(255, 255, 255);
static constexpr int kMaxTestNameLen = 28;
static constexpr int kStatusCol     = 30;
static constexpr int kNameBufSize   = 80;

int KernelDisplay::Sc(int base, int dpiScale) {
  return (std::max<int>)(1, static_cast<int>(base * (dpiScale / 96.0)));
}

void KernelDisplay::DrawTextAt(HDC dc, int x, int y, const wchar_t* text, COLORREF color, HFONT font) {
  SetTextColor(dc, color);
  SetBkColor(dc, COL_BG);
  SetBkMode(dc, OPAQUE);
  SelectObject(dc, font);
  SetTextAlign(dc, TA_LEFT | TA_TOP);
  ExtTextOutW(dc, x, y, ETO_OPAQUE, nullptr, text, static_cast<int>(wcslen(text)), nullptr);
}

void KernelDisplay::DrawTextCentered(HDC dc, int cx, int y, const wchar_t* text, COLORREF color, HFONT font) {
  SetTextColor(dc, color);
  SetBkColor(dc, COL_BG);
  SetBkMode(dc, OPAQUE);
  SelectObject(dc, font);
  SetTextAlign(dc, TA_CENTER | TA_TOP);
  ExtTextOutW(dc, cx, y, ETO_OPAQUE, nullptr, text, static_cast<int>(wcslen(text)), nullptr);
}

HFONT KernelDisplay::LoadKernelFont(HDC dc, int dpiScale, int* outHeight) const {
  static HFONT cachedFont = nullptr;
  static int cachedDpi = 0;
  static int cachedHeight = 0;

  if (cachedFont && cachedDpi == dpiScale) {
    if (outHeight) *outHeight = cachedHeight;
    return cachedFont;
  }
  if (cachedFont) { DeleteObject(cachedFont); cachedFont = nullptr; }

  wchar_t fontPath[MAX_PATH];
  GetModuleFileNameW(nullptr, fontPath, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(fontPath, L'\\');
  if (lastSlash) {
    wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - fontPath + 1), L"fonts\\PhoenixEGA 8x8-2y.ttf");
    if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
      AddFontResourceExW(fontPath, FR_PRIVATE, 0);
      int fontSize = Sc(12, dpiScale);
      HFONT font = CreateFontW(fontSize, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Ac437 PhoenixEGA 8x8-2y");
      if (font) {
        TEXTMETRICW tm{};
        HFONT old = static_cast<HFONT>(SelectObject(dc, font));
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

  int fontSize = Sc(12, dpiScale);
  HFONT font = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
    NONANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Terminal");
  if (!font) {
    font = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
      NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
  }
  if (font && outHeight) {
    TEXTMETRICW tm{};
    HFONT old = static_cast<HFONT>(SelectObject(dc, font));
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

void KernelDisplay::Draw(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                         HFONT bodyFont, HFONT smallFont, int dpiScale) const {
  HBRUSH br = CreateSolidBrush(COL_BG);
  FillRect(dc, &clientRect, br);
  DeleteObject(br);

  int fontHeight = 0;
  HFONT mono = LoadKernelFont(dc, dpiScale, &fontHeight);
  if (!mono) mono = bodyFont;
  const int lh = fontHeight > 0 ? fontHeight : Sc(12, dpiScale);
  const int cw = Sc(7, dpiScale);

  const int W = clientRect.right - clientRect.left;
  const int H = clientRect.bottom - clientRect.top;
  const int cx = W / 2;

  switch (kernel.GetPhase()) {
    case KernelPhase::Startup:
    case KernelPhase::Diagnostics:
      DrawDiagnostics(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    case KernelPhase::Ready:
      DrawReady(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    case KernelPhase::Authenticating:
      DrawAuth(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    case KernelPhase::AuthSuccess:
      DrawAuthSuccess(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    case KernelPhase::LoadingEnvironment:
    case KernelPhase::LoadingGui:
      DrawLoading(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    case KernelPhase::Crash:
      DrawCrash(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    case KernelPhase::Shutdown:
    case KernelPhase::Halted:
      DrawShutdown(dc, clientRect, kernel, mono, lh, cw, cx, W, H, dpiScale);
      break;
    default:
      break;
  }
}
void KernelDisplay::DrawDiagnostics(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                    HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);

  const int headerRows = 2;
  const int footerRows = 3;
  const int maxVisibleRows = (H / lh) - headerRows - footerRows;
  const auto& groups = kernel.GetGroups();
  const auto& results = kernel.GetResults();

  // Calculate total row count
  int totalRows = 0;
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    totalRows += 1 + static_cast<int>(groups[gi].tests.size());
  }

  // Find current test position
  int currentPos = 0;
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    for (std::size_t ti = 0; ti < groups[gi].tests.size(); ++ti) {
      if (gi == kernel.GetCurrentGroup() && ti == kernel.GetCurrentTest()
          && !kernel.IsDiagnosticsComplete()) {
        goto found_current;
      }
      currentPos++;
    }
    currentPos++; // group header
  }
found_current:

  // Auto-scroll: keep current test centered
  if (totalRows > maxVisibleRows) {
    int targetOffset = currentPos - maxVisibleRows / 2;
    if (targetOffset < 0) targetOffset = 0;
    if (targetOffset > totalRows - maxVisibleRows) targetOffset = totalRows - maxVisibleRows;
    // Smooth scroll
    if (scrollOffset_ < targetOffset) scrollOffset_ = (std::min)(scrollOffset_ + 3, targetOffset);
    if (scrollOffset_ > targetOffset) scrollOffset_ = (std::max)(scrollOffset_ - 3, targetOffset);
  }

  // Render visible rows
  int row = headerRows;
  int visualRow = 0;
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    // Group header
    if (visualRow >= scrollOffset_ && visualRow < scrollOffset_ + maxVisibleRows) {
      wchar_t groupBuf[64];
      _snwprintf_s(groupBuf, _countof(groupBuf), _TRUNCATE, L"[%hs]", groups[gi].title);
      text(0, row, groupBuf, COL_WHITE);
      row++;
    }
    visualRow++;

    for (std::size_t ti = 0; ti < groups[gi].tests.size(); ++ti) {
      if (visualRow >= scrollOffset_ && visualRow < scrollOffset_ + maxVisibleRows) {
        bool found = false;
        const wchar_t* statusText = L"----";
        COLORREF statusColor = COL_WHITE;

        for (const auto& r : results) {
          if (r.groupIndex == static_cast<std::size_t>(gi) && r.testIndex == static_cast<std::size_t>(ti)) {
            switch (r.severity) {
              case monix::kernel::TestSeverity::Pass:
                statusText = L" OK  "; statusColor = COL_WHITE; break;
              case monix::kernel::TestSeverity::Warning:
                statusText = L"WARN "; statusColor = COL_WHITE; break;
              case monix::kernel::TestSeverity::Fail:
                statusText = L"FAIL "; statusColor = COL_WHITE; break;
              case monix::kernel::TestSeverity::Skipped:
                statusText = L"SKIP "; statusColor = COL_WHITE; break;
              case monix::kernel::TestSeverity::Fatal:
                statusText = L"FATAL"; statusColor = COL_WHITE; break;
              default: break;
            }
            found = true;
            break;
          }
        }

        if (!found && gi == kernel.GetCurrentGroup() && ti == kernel.GetCurrentTest()
            && !kernel.IsDiagnosticsComplete()) {
          statusText = L" >>  ";
          statusColor = COL_WHITE;
          found = true;
        }

        if (found) {
          wchar_t nameBuf[kNameBufSize];
          const char* rawName = groups[gi].tests[ti].name;
          int nameLen = static_cast<int>(strlen(rawName));
          if (nameLen > kMaxTestNameLen) {
            _snwprintf_s(nameBuf, _countof(nameBuf), _TRUNCATE, L"  %.26hs..", rawName);
          } else {
            _snwprintf_s(nameBuf, _countof(nameBuf), _TRUNCATE, L"  %hs", rawName);
          }
          text(0, row, nameBuf, COL_WHITE);
          text(kStatusCol, row, statusText, statusColor);
          row++;
        }
      }
      visualRow++;
    }
  }

  // Footer: progress bar
  int footerY = H / lh - 2;
  wchar_t progress[128];
  int passed = kernel.GetPassCount();
  int failed = kernel.GetFailCount();
  int total = passed + failed + kernel.GetWarnCount() + kernel.GetSkipCount();
  _snwprintf_s(progress, _countof(progress), _TRUNCATE,
    L"%d/%d | %u OK  %u FAIL  %u WARN",
    total, totalRows, passed, failed, kernel.GetWarnCount());
  text(0, footerY, progress, COL_WHITE);

  if (kernel.IsDiagnosticsComplete()) {
    wchar_t summary[128];
    _snwprintf_s(summary, _countof(summary), _TRUNCATE,
      L"DONE: %u passed  %u warnings  %u failed  %u skipped",
      kernel.GetPassCount(), kernel.GetWarnCount(), kernel.GetFailCount(), kernel.GetSkipCount());
    text(0, footerY + 1, summary, COL_WHITE);
  }
}

void KernelDisplay::DrawReady(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                              HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);
  text(0, 2, L"DIAGNOSTICS COMPLETE", COL_WHITE);
  text(0, 4, L"All startup tests passed successfully.", COL_WHITE);
  text(0, 5, L"Kernel status: READY", COL_WHITE);
  text(0, 7, L"Starting authentication...", COL_WHITE);
}

void KernelDisplay::DrawAuth(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                             HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);
  text(0, 2, L"AUTHENTICATION REQUIRED", COL_WHITE);
  text(0, 4, L"Password:", COL_WHITE);

  const auto& pwd = kernel.GetPassword();
  bool visible = kernel.IsPasswordVisible();
  int inputLen = static_cast<int>(pwd.size());
  int maxShow = 40;
  int showLen = (std::min<int>)(inputLen, maxShow);

  wchar_t pwdLine[80];
  if (visible) {
    wchar_t pwdBuf[64];
    int copyLen = showLen;
    for (int i = 0; i < copyLen && i < 63; i++) pwdBuf[i] = pwd[i];
    pwdBuf[copyLen] = 0;
    _snwprintf_s(pwdLine, _countof(pwdLine), _TRUNCATE, L"  [%s%.*s]",
      pwdBuf, maxShow - showLen, L"                                        ");
  } else {
    wchar_t dotsBuf[64];
    for (int i = 0; i < showLen; i++) dotsBuf[i] = 0x2022;
    dotsBuf[showLen] = 0;
    _snwprintf_s(pwdLine, _countof(pwdLine), _TRUNCATE, L"  [%s%.*s]",
      dotsBuf, maxShow - showLen, L"                                        ");
  }
  text(0, 5, pwdLine, COL_WHITE);

  int row = 7;
  if (!kernel.GetAuthError().empty()) {
    text(0, row, kernel.GetAuthError().c_str(), COL_WHITE);
    row++;
  }
  if (kernel.IsAuthLocked()) {
    text(0, row, L"Account locked. Wait or press ESC.", COL_WHITE);
    row++;
  }
  text(0, 10, L"TAB: Show/Hide  |  Enter: Confirm  |  ESC: Shutdown", COL_WHITE);
}

void KernelDisplay::DrawAuthSuccess(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                    HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);
  text(0, 2, L"ACCESS GRANTED", COL_WHITE);
  text(0, 4, L"Loading user environment...", COL_WHITE);
}

void KernelDisplay::DrawLoading(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);
  text(0, 2, L"LOADING ENVIRONMENT", COL_WHITE);

  const wchar_t* steps[] = {
    L"System interface",
    L"GUI subsystem",
    L"Interface services",
    L"Renderer pipeline",
    L"Workspace state",
    L"MONIX GUI"
  };
  int step = kernel.IsLoadingComplete() ? 6 : (kernel.GetLoadingStep());
  for (int i = 0; i < 6; i++) {
    wchar_t line[80];
    if (i < step) {
      _snwprintf_s(line, _countof(line), _TRUNCATE, L"  %hs ................... OK", steps[i]);
      text(0, 4 + i, line, COL_WHITE);
    } else if (i == step && !kernel.IsLoadingComplete()) {
      _snwprintf_s(line, _countof(line), _TRUNCATE, L"  %hs ................... >>", steps[i]);
      text(0, 4 + i, line, COL_WHITE);
    } else {
      _snwprintf_s(line, _countof(line), _TRUNCATE, L"  %hs ...................", steps[i]);
      text(0, 4 + i, line, COL_WHITE);
    }
  }
}

void KernelDisplay::DrawCrash(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                              HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);
  text(0, 2, L"FATAL SYSTEM ERROR", COL_WHITE);
  text(0, 4, L"The kernel encountered a critical failure.", COL_WHITE);
  text(0, 6, L"SYSTEM HALTED", COL_WHITE);
}

void KernelDisplay::DrawShutdown(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                 HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  text(0, 0, L"MONIX KERNEL v1.0", COL_WHITE);
  text(0, 2, L"SHUTDOWN", COL_WHITE);
  text(0, 4, L"Stopping services................... OK", COL_WHITE);
  text(0, 5, L"Stopping processes.................. OK", COL_WHITE);
  text(0, 6, L"Flushing logs....................... OK", COL_WHITE);
  text(0, 7, L"Saving kernel state................. OK", COL_WHITE);
  text(0, 9, L"SYSTEM HALTED", COL_WHITE);
}

} // namespace Kernel
} // namespace Monix