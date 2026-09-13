#include "DiagnosticsDisplay.hpp"

#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <setupapi.h>

#if defined(MONIX_ARCH_X64) || defined(MONIX_ARCH_X86)
#include <intrin.h>
#endif

#pragma comment(lib, "setupapi.lib")

static const GUID GUID_DEVCLASS_DISKDRIVE = {0x4D36E967, 0xE325, 0x11CE, {0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18}};

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

  static wchar_t s_prevFontPath[MAX_PATH] = {};
  if (s_prevFontPath[0] != L'\0') {
    RemoveFontResourceExW(s_prevFontPath, FR_PRIVATE, 0);
    s_prevFontPath[0] = L'\0';
  }

  wchar_t fontPath[MAX_PATH];
  GetModuleFileNameW(nullptr, fontPath, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(fontPath, L'\\');
  if (lastSlash) {
    wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - fontPath + 1), L"Perfect DOS VGA 437 Win.ttf");
    if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
      AddFontResourceExW(fontPath, FR_PRIVATE, 0);
      wcscpy_s(s_prevFontPath, fontPath);
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

static bool GetFirstDeviceFriendlyName(const GUID& devClass, std::wstring& outName) {
  HDEVINFO devInfo = SetupDiGetClassDevsW(&devClass, nullptr, nullptr, DIGCF_PRESENT);
  if (devInfo == INVALID_HANDLE_VALUE) return false;
  SP_DEVINFO_DATA devData{};
  devData.cbSize = sizeof(devData);
  bool found = false;
  if (SetupDiEnumDeviceInfo(devInfo, 0, &devData)) {
    wchar_t name[512]{};
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devData, SPDRP_FRIENDLYNAME,
          nullptr, reinterpret_cast<BYTE*>(name), sizeof(name), nullptr)) {
      outName = name;
      found = true;
    }
  }
  SetupDiDestroyDeviceInfoList(devInfo);
  return found;
}

void DiagnosticsDisplay::DetectHardware() const {
  if (detected_) return;

#if defined(MONIX_ARCH_X64) || defined(MONIX_ARCH_X86)
  int cpuInfo[4] = {};
  __cpuid(cpuInfo, 0x80000000);
  if (cpuInfo[0] >= 0x80000004) {
    char cpuName[49] = {};
    __cpuid(cpuInfo, 0x80000002);
    memcpy(cpuName, cpuInfo, 16);
    __cpuid(cpuInfo, 0x80000003);
    memcpy(cpuName + 16, cpuInfo, 16);
    __cpuid(cpuInfo, 0x80000004);
    memcpy(cpuName + 32, cpuInfo, 16);
    cpuName_ = std::wstring(cpuName, cpuName + strlen(cpuName));
  }
#endif

  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  cpuCores_ = std::to_wstring(si.dwNumberOfProcessors) + L" Logical Processors";

  MEMORYSTATUSEX mem{};
  mem.dwLength = sizeof(mem);
  if (GlobalMemoryStatusEx(&mem)) {
    ULONGLONG totalMB = mem.ullTotalPhys / (1024ULL * 1024ULL);
    wchar_t ram[64];
    _snwprintf_s(ram, _countof(ram), _TRUNCATE, L"%llu MB", totalMB);
    totalRam_ = ram;
  }

  wchar_t hostname[256]{};
  DWORD hostSize = 256;
  GetComputerNameW(hostname, &hostSize);
  hostname_ = hostname;

  detected_ = true;
}

void DiagnosticsDisplay::RunTests() const {
  if (testsRun_) return;
  testsRun_ = true;
  passCount_ = 0;
  failCount_ = 0;
  warnCount_ = 0;
  hasFatal_ = false;

  const auto& groups = diag_.Groups();
  for (std::size_t gi = 0; gi < groups.size(); ++gi) {
    for (std::size_t ti = 0; ti < groups[gi].tests.size(); ++ti) {
      auto result = diag_.RunTest(gi, ti, events_);
      diag_.SetTestResult(gi, ti, result.severity, result.detail);
      switch (result.severity) {
        case monix::kernel::TestSeverity::Pass: passCount_++; break;
        case monix::kernel::TestSeverity::Warning: warnCount_++; break;
        case monix::kernel::TestSeverity::Fail: failCount_++; break;
        case monix::kernel::TestSeverity::Fatal: hasFatal_ = true; failCount_++; break;
        default: break;
      }
    }
  }
}

bool DiagnosticsDisplay::IsComplete() const {
  if (!started_) return false;
  return (GetTickCount64() - startTick_) >= DIAG_DURATION_MS;
}

void DiagnosticsDisplay::Draw(HDC dc, const RECT& clientRect, const AuthManager& auth,
                              HFONT titleFont, HFONT bodyFont, HFONT smallFont,
                              int dpiScale, const std::wstring& userId) const {
  if (!started_) {
    startTick_ = GetTickCount64();
    started_ = true;
  }

  DetectHardware();
  RunTests();

  const ULONGLONG now = GetTickCount64();
  const ULONGLONG elapsed = now - startTick_;

  const int W = clientRect.right - clientRect.left;
  const int H = clientRect.bottom - clientRect.top;

  int fontHeight = 0;
  HFONT mono = LoadBootFont(dc, dpiScale, &fontHeight);
  if (!mono) mono = bodyFont;
  const int lh = fontHeight > 0 ? fontHeight : Sc(26, dpiScale);
  int cw = Sc(13, dpiScale);

  const int maxContentChars = 80;
  const int minContentChars = 60;
  const int totalChars = std::clamp<int>((W - Sc(8, dpiScale)) / (std::max<int>)(1, cw), minContentChars, maxContentChars);
  const int bx = (W - totalChars * cw) / 2;
  const int startY = (std::max<int>)(Sc(8, dpiScale), (H - 25 * lh) / 2);

  constexpr COLORREF C_WHITE  = RGB(255, 255, 255);
  constexpr COLORREF C_YELLOW = RGB(255, 255, 0);
  constexpr COLORREF C_CYAN   = RGB(0, 255, 255);
  constexpr COLORREF C_GREEN  = RGB(0, 255, 0);
  constexpr COLORREF C_RED    = RGB(255, 0, 0);
  constexpr COLORREF C_DKGRAY = RGB(85, 85, 85);

  {
    HBRUSH br = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &clientRect, br);
    DeleteObject(br);
  }

  auto drawText = [&](int col, int row, const wchar_t* text, COLORREF color) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, text, color, mono);
  };

  int row = 0;

  if (elapsed > 50) {
    drawText(0, row, L"MONIX Kernel Diagnostics v2.6.2", C_CYAN);
  }
  row += 2;

  const auto& groups = diag_.Groups();
  int testIndex = 0;
  int maxTestsToShow = 18;

  for (std::size_t gi = 0; gi < groups.size() && testIndex < maxTestsToShow; ++gi) {
    ULONGLONG groupTime = 100 + (gi * 350);
    if (elapsed < groupTime) break;

    wchar_t groupLine[64];
    _snwprintf_s(groupLine, _countof(groupLine), _TRUNCATE, L"[%hs]", groups[gi].title);
    drawText(0, row, groupLine, C_YELLOW);
    row++;
    testIndex++;

    int testsToShow = (groups[gi].tests.size() > 2) ? 2 : static_cast<int>(groups[gi].tests.size());
    for (int ti = 0; ti < testsToShow && testIndex < maxTestsToShow; ++ti) {
      ULONGLONG testTime = groupTime + 80 + (ti * 60);
      if (elapsed < testTime) break;

      auto sev = diag_.GetTestSeverity(gi, ti);
      const char* detail = diag_.GetTestDetail(gi, ti);

      wchar_t testLine[96];
      const wchar_t* status;
      COLORREF statusColor;
      switch (sev) {
        case monix::kernel::TestSeverity::Pass:
          status = L"[PASS]"; statusColor = C_GREEN; break;
        case monix::kernel::TestSeverity::Warning:
          status = L"[WARN]"; statusColor = C_YELLOW; break;
        case monix::kernel::TestSeverity::Fail:
          status = L"[FAIL]"; statusColor = C_RED; break;
        case monix::kernel::TestSeverity::Fatal:
          status = L"[CRIT]"; statusColor = C_RED; break;
        default:
          status = L"[----]"; statusColor = C_DKGRAY; break;
      }

      _snwprintf_s(testLine, _countof(testLine), _TRUNCATE,
        L"  %s %hs%s", status, groups[gi].tests[ti].name,
        detail ? detail : "");
      drawText(0, row, testLine, statusColor);
      row++;
      testIndex++;
    }
  }

  if (elapsed > 3800) {
    row = 22;
    wchar_t summary[128];
    if (hasFatal_) {
      _snwprintf_s(summary, _countof(summary), _TRUNCATE,
        L"FATAL: %d passed, %d failed", passCount_, failCount_);
      drawText(0, row, summary, C_RED);
    } else {
      _snwprintf_s(summary, _countof(summary), _TRUNCATE,
        L"Diagnostics: %d/%d passed, %d warnings",
        passCount_, passCount_ + failCount_, warnCount_);
      drawText(0, row, summary, failCount_ == 0 ? C_GREEN : C_YELLOW);
    }
  }
}

} // namespace Security
} // namespace Monix
