#include "KernelDisplay.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "../../src/native/updater/Version.hpp"

namespace Monix {
namespace Kernel {

static constexpr COLORREF COL_BG       = RGB(0, 8, 40);
static constexpr COLORREF COL_WHITE    = RGB(200, 210, 220);
static constexpr COLORREF COL_DIM      = RGB(80, 95, 110);
static constexpr COLORREF COL_GREEN    = RGB(0, 220, 80);
static constexpr COLORREF COL_YELLOW   = RGB(230, 200, 40);
static constexpr COLORREF COL_RED      = RGB(230, 50, 50);
static constexpr COLORREF COL_RED_BRIGHT = RGB(255, 80, 80);
static constexpr COLORREF COL_CYAN     = RGB(60, 200, 230);
static constexpr COLORREF COL_ACCENT   = RGB(40, 160, 255);
static constexpr COLORREF COL_HEADER   = RGB(100, 180, 255);
static constexpr int kMaxTestNameLen   = 28;
static constexpr int kStatusCol       = 30;
static constexpr int kNameBufSize     = 80;

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

  static wchar_t s_prevFontPath[MAX_PATH] = {};
  if (s_prevFontPath[0] != L'\0') {
    RemoveFontResourceExW(s_prevFontPath, FR_PRIVATE, 0);
    s_prevFontPath[0] = L'\0';
  }

  wchar_t fontPath[MAX_PATH];
  GetModuleFileNameW(nullptr, fontPath, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(fontPath, L'\\');
  if (lastSlash) {
    wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - fontPath + 1), L"fonts\\PhoenixEGA 8x8-2y.ttf");
    if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
      AddFontResourceExW(fontPath, FR_PRIVATE, 0);
      wcscpy_s(s_prevFontPath, fontPath);
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

static void DrawProgressBar(HDC dc, int x, int y, int total, int current, int maxWidth, HFONT font, int lh) {
  if (total <= 0) return;
  float ratio = static_cast<float>(current) / total;
  int barWidth = 30;
  int filled = static_cast<int>(ratio * barWidth);
  filled = (std::min)(filled, barWidth);

  wchar_t bar[64];
  int pos = 0;
  bar[pos++] = L'[';
  for (int i = 0; i < barWidth; ++i) {
    bar[pos++] = i < filled ? 0x2588 : 0x2591;
  }
  bar[pos++] = L']';
  bar[pos++] = L'\0';

  SetTextColor(dc, COL_CYAN);
  SetBkColor(dc, COL_BG);
  SetBkMode(dc, OPAQUE);
  SelectObject(dc, font);
  SetTextAlign(dc, TA_LEFT | TA_TOP);
  ExtTextOutW(dc, x, y, ETO_OPAQUE, nullptr, bar, pos - 1, nullptr);
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

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_HEADER);

  // Separator line
  int sepLen = (W - margin * 2) / cw;
  wchar_t sep[128];
  sepLen = (std::min)(sepLen, 126);
  for (int i = 0; i < sepLen; ++i) sep[i] = 0x2500;
  sep[sepLen] = L'\0';
  text(0, 1, sep, COL_DIM);

  const int headerRows = 3;
  const int footerRows = 4;
  const int maxVisibleRows = (H / lh) - headerRows - footerRows;
  const auto& groups = kernel.GetGroups();
  const auto& results = kernel.GetResults();

  int totalRows = 0;
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    totalRows += 1 + static_cast<int>(groups[gi].tests.size());
  }

  int currentPos = 0;
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    for (std::size_t ti = 0; ti < groups[gi].tests.size(); ++ti) {
      if (gi == kernel.GetCurrentGroup() && ti == kernel.GetCurrentTest()
          && !kernel.IsDiagnosticsComplete()) {
        goto found_current;
      }
      currentPos++;
    }
    currentPos++;
  }
found_current:

  if (totalRows > maxVisibleRows) {
    int targetOffset = currentPos - maxVisibleRows / 2;
    if (targetOffset < 0) targetOffset = 0;
    if (targetOffset > totalRows - maxVisibleRows) targetOffset = totalRows - maxVisibleRows;
    if (scrollOffset_ < targetOffset) scrollOffset_ = (std::min)(scrollOffset_ + 2, targetOffset);
    if (scrollOffset_ > targetOffset) scrollOffset_ = (std::max)(scrollOffset_ - 2, targetOffset);
  }

  int row = headerRows;
  int visualRow = 0;
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    if (visualRow >= scrollOffset_ && visualRow < scrollOffset_ + maxVisibleRows) {
      wchar_t groupBuf[64];
      _snwprintf_s(groupBuf, _countof(groupBuf), _TRUNCATE, L"[%hs]", groups[gi].title);
      text(0, row, groupBuf, COL_ACCENT);
      row++;
    }
    visualRow++;

    for (std::size_t ti = 0; ti < groups[gi].tests.size(); ++ti) {
      if (visualRow >= scrollOffset_ && visualRow < scrollOffset_ + maxVisibleRows) {
        bool found = false;
        const wchar_t* statusText = L"----";
        COLORREF statusColor = COL_DIM;
        COLORREF nameColor = COL_DIM;

        for (const auto& r : results) {
          if (r.groupIndex == static_cast<std::size_t>(gi) && r.testIndex == static_cast<std::size_t>(ti)) {
            switch (r.severity) {
              case monix::kernel::TestSeverity::Pass:
                statusText = L" OK  "; statusColor = COL_GREEN; nameColor = COL_WHITE; break;
              case monix::kernel::TestSeverity::Warning:
                statusText = L"WARN "; statusColor = COL_YELLOW; nameColor = COL_WHITE; break;
              case monix::kernel::TestSeverity::Fail:
                statusText = L"FAIL "; statusColor = COL_RED; nameColor = COL_RED; break;
              case monix::kernel::TestSeverity::Skipped:
                statusText = L"SKIP "; statusColor = COL_DIM; nameColor = COL_DIM; break;
              case monix::kernel::TestSeverity::Fatal:
                statusText = L"FATAL"; statusColor = COL_RED_BRIGHT; nameColor = COL_RED_BRIGHT; break;
              default: break;
            }
            found = true;
            break;
          }
        }

        if (!found && gi == kernel.GetCurrentGroup() && ti == kernel.GetCurrentTest()
            && !kernel.IsDiagnosticsComplete()) {
          statusText = L" >>  ";
          statusColor = COL_CYAN;
          nameColor = COL_CYAN;
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
          text(0, row, nameBuf, nameColor);
          text(kStatusCol, row, statusText, statusColor);
          row++;
        }
      }
      visualRow++;
    }
  }

  // Footer: progress bar
  int passed = kernel.GetPassCount();
  int failed = kernel.GetFailCount();
  int total = passed + failed + kernel.GetWarnCount() + kernel.GetSkipCount();

  int footerY = H / lh - 3;
  DrawProgressBar(dc, bx, footerY * lh + startY, totalRows, total, W, mono, lh);

  wchar_t stats[128];
  _snwprintf_s(stats, _countof(stats), _TRUNCATE,
    L"%d/%d  ", total, totalRows);
  int statsX = bx + 32 * cw;
  SetTextColor(dc, COL_WHITE);
  SetBkColor(dc, COL_BG);
  SetBkMode(dc, OPAQUE);
  SelectObject(dc, mono);
  SetTextAlign(dc, TA_LEFT | TA_TOP);
  ExtTextOutW(dc, statsX, footerY * lh + startY, ETO_OPAQUE, nullptr,
    stats, static_cast<int>(wcslen(stats)), nullptr);

  int labelX = statsX + 6 * cw;
  text(labelX / cw, footerY, L"", 0);
  DrawTextAt(dc, labelX, footerY * lh + startY, L" ", COL_GREEN, mono);
  wchar_t okBuf[16]; _snwprintf_s(okBuf, _countof(okBuf), _TRUNCATE, L"%d OK", passed);
  DrawTextAt(dc, labelX + 1 * cw, footerY * lh + startY, okBuf, COL_GREEN, mono);

  DrawTextAt(dc, labelX + 7 * cw, footerY * lh + startY, L" ", COL_RED, mono);
  wchar_t failBuf[16]; _snwprintf_s(failBuf, _countof(failBuf), _TRUNCATE, L"%d FAIL", failed);
  DrawTextAt(dc, labelX + 8 * cw, footerY * lh + startY, failBuf, COL_RED, mono);

  DrawTextAt(dc, labelX + 15 * cw, footerY * lh + startY, L" ", COL_YELLOW, mono);
  wchar_t warnBuf[16]; _snwprintf_s(warnBuf, _countof(warnBuf), _TRUNCATE, L"%d WARN", kernel.GetWarnCount());
  DrawTextAt(dc, labelX + 16 * cw, footerY * lh + startY, warnBuf, COL_YELLOW, mono);

  if (kernel.IsDiagnosticsComplete()) {
    wchar_t summary[128];
    bool hasFatals = kernel.GetFailCount() > 0;
    _snwprintf_s(summary, _countof(summary), _TRUNCATE,
      L"Diagnostics complete. %u passed, %u warnings, %u failed.",
      kernel.GetPassCount(), kernel.GetWarnCount(), kernel.GetFailCount());
    text(0, footerY + 2, summary, hasFatals ? COL_RED : COL_GREEN);
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

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_HEADER);
  text(0, 2, L"DIAGNOSTICS COMPLETE", COL_GREEN);
  text(0, 4, L"All startup tests passed successfully.", COL_WHITE);
  text(0, 5, L"Kernel status: ", COL_WHITE);
  DrawTextAt(dc, 15 * cw, 5 * lh + startY, L"READY", COL_GREEN, mono);
  text(0, 7, L"Starting authentication...", COL_DIM);
}

void KernelDisplay::DrawAuth(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                             HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_HEADER);
  text(0, 2, L"AUTHENTICATION REQUIRED", COL_YELLOW);
  text(0, 4, L"Password:", COL_WHITE);

  const auto& pwd = kernel.GetPassword();
  bool visible = kernel.IsPasswordVisible();
  int inputLen = static_cast<int>(pwd.size());
  int maxShow = 40;
  int showLen = (std::min<int>)(inputLen, maxShow);

  // Blinking cursor
  ULONGLONG now = GetTickCount64();
  bool cursorOn = ((now / 500) % 2) == 0;

  wchar_t pwdLine[80];
  if (visible) {
    wchar_t pwdBuf[64];
    int copyLen = showLen;
    for (int i = 0; i < copyLen && i < 63; i++) pwdBuf[i] = pwd[i];
    pwdBuf[copyLen] = 0;
    const wchar_t* cursor = cursorOn ? L"_" : L" ";
    _snwprintf_s(pwdLine, _countof(pwdLine), _TRUNCATE, L"  [%s%s%.*s]",
      pwdBuf, cursor, maxShow - showLen, L"                                        ");
  } else {
    wchar_t dotsBuf[64];
    for (int i = 0; i < showLen; i++) dotsBuf[i] = 0x2022;
    dotsBuf[showLen] = 0;
    const wchar_t* cursor = cursorOn ? L"_" : L" ";
    _snwprintf_s(pwdLine, _countof(pwdLine), _TRUNCATE, L"  [%s%s%.*s]",
      dotsBuf, cursor, maxShow - showLen, L"                                        ");
  }
  text(0, 5, pwdLine, COL_WHITE);

  int row = 7;
  if (!kernel.GetAuthError().empty()) {
    text(0, row, kernel.GetAuthError().c_str(), COL_RED);
    row++;
  }
  if (kernel.IsAuthLocked()) {
    text(0, row, L"Account locked. Wait or press ESC.", COL_RED);
    row++;
  }

  // Separator
  int sepLen = (W - margin * 2) / cw;
  wchar_t sep[128];
  sepLen = (std::min)(sepLen, 126);
  for (int i = 0; i < sepLen; ++i) sep[i] = 0x2500;
  sep[sepLen] = L'\0';
  text(0, 9, sep, COL_DIM);

  text(0, 10, L"TAB: Show/Hide  |  Enter: Confirm  |  ESC: Shutdown", COL_DIM);
}

void KernelDisplay::DrawAuthSuccess(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                    HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_HEADER);
  text(0, 2, L"ACCESS GRANTED", COL_GREEN);

  ULONGLONG elapsed = GetTickCount64() - kernel.GetBootTimeMs();
  wchar_t timeStr[64];
  _snwprintf_s(timeStr, _countof(timeStr), _TRUNCATE, L"Boot time: %u ms", static_cast<unsigned>(elapsed));
  text(0, 4, timeStr, COL_DIM);
  text(0, 6, L"Loading user environment...", COL_WHITE);
}

void KernelDisplay::DrawLoading(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_HEADER);
  text(0, 2, L"LOADING ENVIRONMENT", COL_CYAN);

  const wchar_t* steps[] = {
    L"System interface",
    L"GUI subsystem",
    L"Interface services",
    L"Renderer pipeline",
    L"Workspace state",
    L"MONIX GUI"
  };

  ULONGLONG now = GetTickCount64();
  int animFrame = static_cast<int>((now / 200) % 4);
  const wchar_t* spinners[] = { L"|", L"/", L"-", L"\\" };

  int step = kernel.IsLoadingComplete() ? 6 : (kernel.GetLoadingStep());
  for (int i = 0; i < 6; i++) {
    wchar_t line[80];
    if (i < step) {
      _snwprintf_s(line, _countof(line), _TRUNCATE, L"  %hs ............... [OK]", steps[i]);
      text(0, 4 + i, line, COL_GREEN);
    } else if (i == step && !kernel.IsLoadingComplete()) {
      _snwprintf_s(line, _countof(line), L"  %hs .............. [%s]", steps[i], spinners[animFrame]);
      text(0, 4 + i, line, COL_CYAN);
    } else {
      _snwprintf_s(line, _countof(line), _TRUNCATE, L"  %hs ............... [ ]", steps[i]);
      text(0, 4 + i, line, COL_DIM);
    }
  }

  // Progress bar
  DrawProgressBar(dc, bx, 11 * lh + startY, 6, step, W, mono, lh);
}

void KernelDisplay::DrawCrash(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                              HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  // Red flash border
  HBRUSH redBrush = CreateSolidBrush(RGB(60, 0, 0));
  RECT border = clientRect;
  border.right = border.left + 4;
  FillRect(dc, &border, redBrush);
  border = clientRect;
  border.left = border.right - 4;
  FillRect(dc, &border, redBrush);
  DeleteObject(redBrush);

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_RED_BRIGHT);
  text(0, 2, L"FATAL SYSTEM ERROR", COL_RED_BRIGHT);

  // Separator
  int sepLen = (W - margin * 2) / cw;
  wchar_t sep[128];
  sepLen = (std::min)(sepLen, 126);
  for (int i = 0; i < sepLen; ++i) sep[i] = 0x2588;
  sep[sepLen] = L'\0';
  text(0, 3, sep, COL_RED);

  int row = 5;
  if (kernel.GetCrashSubsystem()) {
    wchar_t subLine[128];
    _snwprintf_s(subLine, _countof(subLine), _TRUNCATE, L"Subsystem: %hs", kernel.GetCrashSubsystem());
    text(0, row, subLine, COL_YELLOW);
    row++;
  }
  if (kernel.GetCrashTest()) {
    wchar_t testLine[128];
    _snwprintf_s(testLine, _countof(testLine), _TRUNCATE, L"Failed:    %hs", kernel.GetCrashTest());
    text(0, row, testLine, COL_RED);
    row++;
  }
  if (kernel.GetCrashDetail()) {
    wchar_t detailLine[128];
    _snwprintf_s(detailLine, _countof(detailLine), _TRUNCATE, L"Detail:    %hs", kernel.GetCrashDetail());
    text(0, row, detailLine, COL_WHITE);
    row++;
  }

  row++;
  text(0, row, L"SYSTEM HALTED", COL_RED_BRIGHT);
  row += 2;
  text(0, row, L"Press any key to restart or ESC to shutdown.", COL_DIM);
}

void KernelDisplay::DrawShutdown(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                                 HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const {
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  wchar_t header[64];
  _snwprintf_s(header, _countof(header), _TRUNCATE, L"MONIX KERNEL v%hs", MONIX_VERSION);
  text(0, 0, header, COL_DIM);
  text(0, 2, L"SHUTDOWN", COL_YELLOW);
  text(0, 4, L"Stopping services................... [OK]", COL_GREEN);
  text(0, 5, L"Stopping processes.................. [OK]", COL_GREEN);
  text(0, 6, L"Flushing logs....................... [OK]", COL_GREEN);
  text(0, 7, L"Saving kernel state................. [OK]", COL_GREEN);
  text(0, 9, L"SYSTEM HALTED", COL_RED);
}

} // namespace Kernel
} // namespace Monix
