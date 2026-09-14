#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../core/TextUtils.hpp"
#include "../logging/LogEntry.hpp"
#include "../settings/MonixConfigTypes.hpp"
#include "../telemetry/Snapshot.hpp"
#include "AppState.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace monix::ui {

constexpr int kCoreMonitorThemeMode = 3;
constexpr int kWin98ThemeMode = 4;

struct CoreMonitorThemeFonts {
  HFONT title = nullptr;
  HFONT body = nullptr;
  HFONT smallText = nullptr;
  HFONT logText = nullptr;
  int bodyLineHeight = 28;
  int smallLineHeight = 20;
  int logLineHeight = 20;
};

struct CoreMonitorThemeContext {
  const Snapshot* snapshot = nullptr;
  const std::vector<LogEntry>* logs = nullptr;
  const SessionCounters* counters = nullptr;
  const std::vector<double>* cpuHistory = nullptr;
  const std::vector<double>* ramHistory = nullptr;
  const std::vector<double>* gpuHistory = nullptr;
  const std::vector<double>* netHistory = nullptr;
  const std::vector<double>* netUploadHistory = nullptr;
  Config config;
  CoreMonitorThemeFonts fonts;
  std::wstring rendererName;
  std::wstring shaderName;
  std::wstring fontName;
  int menuIndex = 0;
  bool livePaused = false;
  std::uint64_t frameCount = 0;
  const UpdateState* updateState = nullptr;
  int settingsCategory = 0;
  int pressedButton = -1;
  int hoveredMenuIndex = -1;
  int activeFilter = 0;
  int taskScroll = 0;
  int selectedTaskPid = 0;
};

class CoreMonitorTheme {
 public:
  static constexpr int kMenuCount = 9;

  static void Render(HDC dc, const RECT& clientRect, const CoreMonitorThemeContext& ctx) {
    const Canvas canvas = MakeCanvas(clientRect);
    Fill(dc, clientRect, RGB(0, 0, 0));
    Fill(dc, canvas.rect, kBlack);

    DrawAmbientBands(dc, canvas);
    DrawOuterFrame(dc, canvas);
    DrawHeader(dc, canvas, ctx);
    DrawSidebar(dc, canvas, ctx);
    DrawRightRail(dc, canvas, ctx);

    if (std::clamp(ctx.menuIndex, 0, kMenuCount - 1) == 0) {
      DrawOverview(dc, canvas, ctx);
    } else {
      DrawSectionPage(dc, canvas, ctx);
    }

    DrawLogPanel(dc, canvas, ctx);
    DrawFooter(dc, canvas, ctx);
  }

  static bool HitTestMenu(const RECT& clientRect, POINT point, int& menuIndex) {
    const Canvas canvas = MakeCanvas(clientRect);
    if (!PtInRect(&canvas.rect, point)) {
      return false;
    }
    const double x = (point.x - canvas.rect.left) / canvas.scale;
    const double y = (point.y - canvas.rect.top) / canvas.scale;
    if (x < 50.0 || x > 264.0 || y < 136.0 || y > 369.0) {
      return false;
    }
    const int index = static_cast<int>((y - 136.0) / 26.0);
    if (index < 0 || index >= kMenuCount) {
      return false;
    }
    menuIndex = index;
    return true;
  }

  static bool HitTestThemeControl(const RECT& clientRect, POINT point, int& direction) {
    const Canvas canvas = MakeCanvas(clientRect);
    if (!PtInRect(&canvas.rect, point)) {
      return false;
    }
    const double x = (point.x - canvas.rect.left) / canvas.scale;
    const double y = (point.y - canvas.rect.top) / canvas.scale;
    if (y < 197.0 || y > 230.0) {
      return false;
    }
    if (x >= 930.0 && x <= 966.0) {
      direction = -1;
      return true;
    }
    if (x >= 974.0 && x <= 1010.0) {
      direction = 1;
      return true;
    }
    return false;
  }

 private:
  static constexpr int kDesignWidth = 1536;
  static constexpr int kDesignHeight = 1024;
  static constexpr COLORREF kBlack = RGB(1, 6, 2);
  static constexpr COLORREF kPanel = RGB(2, 14, 4);
  static constexpr COLORREF kPanelSoft = RGB(4, 20, 6);
  static constexpr COLORREF kPanelDeep = RGB(1, 10, 2);
  static constexpr COLORREF kGreen = RGB(140, 255, 112);
  static constexpr COLORREF kGreenBright = RGB(180, 255, 150);
  static constexpr COLORREF kGreen2 = RGB(84, 184, 74);
  static constexpr COLORREF kGreen3 = RGB(48, 119, 43);
  static constexpr COLORREF kGreen4 = RGB(24, 74, 25);
  static constexpr COLORREF kGreen5 = RGB(14, 48, 16);
  static constexpr COLORREF kGlow = RGB(8, 64, 18);
  static constexpr COLORREF kGlowSoft = RGB(4, 36, 10);
  static constexpr COLORREF kBezel = RGB(18, 18, 20);
  static constexpr COLORREF kBezelEdge = RGB(30, 30, 34);
  static constexpr COLORREF kBezelInner = RGB(8, 8, 10);
  static constexpr COLORREF kWarn = RGB(230, 210, 76);
  static constexpr COLORREF kError = RGB(255, 92, 74);
  static constexpr COLORREF kAccent = RGB(116, 248, 93);

  struct Canvas {
    RECT rect {};
    double scale = 1.0;
  };

  static Canvas MakeCanvas(const RECT& clientRect) {
    const int clientW = std::max(1, static_cast<int>(clientRect.right - clientRect.left));
    const int clientH = std::max(1, static_cast<int>(clientRect.bottom - clientRect.top));
    const double scale = std::min(
      static_cast<double>(clientW) / static_cast<double>(kDesignWidth),
      static_cast<double>(clientH) / static_cast<double>(kDesignHeight));
    const int w = std::max(1, static_cast<int>(std::round(kDesignWidth * scale)));
    const int h = std::max(1, static_cast<int>(std::round(kDesignHeight * scale)));
    const int left = clientRect.left + (clientW - w) / 2;
    const int top = clientRect.top + (clientH - h) / 2;
    return Canvas { RECT { left, top, left + w, top + h }, scale };
  }

  static int X(const Canvas& c, int v) {
    return c.rect.left + static_cast<int>(std::round(v * c.scale));
  }

  static int Y(const Canvas& c, int v) {
    return c.rect.top + static_cast<int>(std::round(v * c.scale));
  }

  static int S(const Canvas& c, int v) {
    return std::max(1, static_cast<int>(std::round(v * c.scale)));
  }

  static RECT R(const Canvas& c, int x, int y, int w, int h) {
    return RECT { X(c, x), Y(c, y), X(c, x + w), Y(c, y + h) };
  }

  static RECT Pad(const RECT& r, int left, int top, int right, int bottom) {
    return RECT { r.left + left, r.top + top, r.right - right, r.bottom - bottom };
  }

  static void Fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
  }

  static void Outline(HDC dc, const RECT& rect, COLORREF color, int width = 1, int style = PS_SOLID) {
    HPEN pen = CreatePen(style, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  static void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1, int style = PS_SOLID) {
    HPEN pen = CreatePen(style, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  static std::wstring PadRight(std::wstring text, std::size_t width) {
    if (text.size() >= width) {
      return text.substr(0, width);
    }
    text.append(width - text.size(), L' ');
    return text;
  }

  static std::wstring Fixed(double value, int decimals) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
  }

  static std::wstring ClockText() {
    SYSTEMTIME time {};
    GetLocalTime(&time);
    wchar_t buffer[32];
    swprintf_s(buffer, L"%02d:%02d:%02d", time.wHour, time.wMinute, time.wSecond);
    return buffer;
  }

  static std::wstring DateText() {
    SYSTEMTIME time {};
    GetLocalTime(&time);
    wchar_t buffer[32];
    swprintf_s(buffer, L"%02d/%02d/%04d", time.wMonth, time.wDay, time.wYear);
    return buffer;
  }

  static std::wstring Hms(std::uint64_t seconds) {
    const std::uint64_t d = seconds / 86400ull;
    const std::uint64_t h = (seconds / 3600ull) % 24ull;
    const std::uint64_t m = (seconds / 60ull) % 60ull;
    const std::uint64_t s = seconds % 60ull;
    wchar_t buffer[32];
    if (d > 0) {
      swprintf_s(buffer, L"%llud %02llu:%02llu:%02llu",
        static_cast<unsigned long long>(d),
        static_cast<unsigned long long>(h),
        static_cast<unsigned long long>(m),
        static_cast<unsigned long long>(s));
    } else {
      swprintf_s(buffer, L"%02llu:%02llu:%02llu",
        static_cast<unsigned long long>(h),
        static_cast<unsigned long long>(m),
        static_cast<unsigned long long>(s));
    }
    return buffer;
  }

  static double PercentOr(double value, double fallback) {
    if (std::isfinite(value) && value > 0.001) {
      return std::clamp(value, 0.0, 100.0);
    }
    return fallback;
  }

  static double BytesToGb(std::uint64_t bytes) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
  }

  static std::wstring RateText(std::uint64_t bytesPerSec, double fallbackMb) {
    double mb = bytesPerSec > 0 ? static_cast<double>(bytesPerSec) / (1024.0 * 1024.0) : fallbackMb;
    return Fixed(mb, 1) + L" MB/s";
  }

  static std::wstring RateTextShort(std::uint64_t bytesPerSec, double fallbackMb) {
    double mb = bytesPerSec > 0 ? static_cast<double>(bytesPerSec) / (1024.0 * 1024.0) : fallbackMb;
    return Fixed(mb, 1) + L"M";
  }

  static std::wstring RateTextOrUnavailable(std::uint64_t bytesPerSec) {
    if (bytesPerSec == 0) {
      return L"N/A";
    }
    return RateText(bytesPerSec, 0.0);
  }

  static std::wstring RateTextShortOrUnavailable(std::uint64_t bytesPerSec) {
    if (bytesPerSec == 0) {
      return L"N/A";
    }
    return RateTextShort(bytesPerSec, 0.0);
  }

  static std::wstring ShortText(std::wstring value, std::size_t limit) {
    if (value.size() <= limit) {
      return value;
    }
    if (limit <= 3) {
      return value.substr(0, limit);
    }
    return value.substr(0, limit - 3) + L"...";
  }

  static void Text(HDC dc, RECT rect, const std::wstring& text, HFONT font, COLORREF color, UINT format, bool glow = true) {
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, font);
    if (glow) {
      RECT g1 = rect;
      OffsetRect(&g1, 1, 0);
      SetTextColor(dc, kGlowSoft);
      DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &g1, format);
      RECT g2 = rect;
      OffsetRect(&g2, -1, 0);
      SetTextColor(dc, kGlowSoft);
      DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &g2, format);
      RECT g3 = rect;
      OffsetRect(&g3, 0, 1);
      SetTextColor(dc, kGlow);
      DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &g3, format);
    }
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
    SelectObject(dc, oldFont);
  }

  static void TextLine(HDC dc, const Canvas& c, int x, int y, int w, const std::wstring& text, HFONT font, int lineHeight, COLORREF color = kGreen2) {
    RECT rect = R(c, x, y, w, lineHeight + 6);
    Text(dc, rect, text, font, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
  }

  static COLORREF SeverityColor(const std::wstring& text) {
    if (text.find(L"WARN") != std::wstring::npos || text.find(L"[W]") != std::wstring::npos) return kWarn;
    if (text.find(L"ERROR") != std::wstring::npos || text.find(L"FAIL") != std::wstring::npos || text.find(L"[E]") != std::wstring::npos) return kError;
    return kGreen;
  }

  static void DrawPanelBevel(HDC dc, const Canvas& c, int x, int y, int w, int h) {
    RECT outer = R(c, x, y, w, h);
    Fill(dc, outer, kPanel);
    Outline(dc, outer, kGreen3);
    Outline(dc, R(c, x + 1, y + 1, w - 2, h - 2), kGreen5);
  }

  static void DrawVignette(HDC dc, const Canvas& c) {
    const int steps = 18;
    for (int i = 0; i < steps; ++i) {
      const int a = (steps - i) * 2;
      const int inset = S(c, i * 2);
      RECT top { c.rect.left + inset, c.rect.top + inset, c.rect.right - inset, c.rect.top + inset + S(c, 3) };
      RECT bottom { c.rect.left + inset, c.rect.bottom - inset - S(c, 3), c.rect.right - inset, c.rect.bottom - inset };
      RECT left { c.rect.left + inset, c.rect.top + inset, c.rect.left + inset + S(c, 3), c.rect.bottom - inset };
      RECT right { c.rect.right - inset - S(c, 3), c.rect.top + inset, c.rect.right - inset, c.rect.bottom - inset };
      int g = std::max(0, a / 4);
      COLORREF edge = RGB(g / 3, g, g / 3);
      Fill(dc, top, edge);
      Fill(dc, bottom, edge);
      Fill(dc, left, edge);
      Fill(dc, right, edge);
    }
  }

  static void DrawAmbientBands(HDC dc, const Canvas& c) {
    for (int i = 0; i < 80; ++i) {
      const int y0 = 14 + (i * 12);
      const int base = 3 + (i % 3);
      const int bright = ((i % 7) == 0) ? 6 : 0;
      const int flicker = ((i % 13) == 0) ? 3 : 0;
      const int g = base + bright + flicker;
      Fill(dc, R(c, 20, y0, 1496, 1), RGB(0, g, 0));
    }
  }

  static void DrawOuterFrame(HDC dc, const Canvas& c) {
    RECT bezel = R(c, 20, 12, 1498, 1000);
    Fill(dc, bezel, kBezel);
    RECT bezelHi = R(c, 20, 12, 1498, 14);
    Fill(dc, bezelHi, RGB(28, 28, 32));
    RECT bezelBot = R(c, 20, 998, 1498, 14);
    Fill(dc, bezelBot, RGB(6, 6, 8));
    RECT bezelEdge = R(c, 24, 16, 1490, 992);
    Fill(dc, bezelEdge, kBezelEdge);
    RECT inner = R(c, 28, 20, 1482, 984);
    Fill(dc, inner, kBezelInner);
    RECT screen = R(c, 32, 28, 1474, 964);
    Fill(dc, screen, RGB(1, 8, 2));
    Outline(dc, R(c, 44, 72, 1446, 870), kGreen5);
    Line(dc, X(c, 50), Y(c, 76), X(c, 1486), Y(c, 76), kGreen3, 1);
    Line(dc, X(c, 49), Y(c, 948), X(c, 1486), Y(c, 948), kGreen5, 1);
    HPEN bezelPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 44));
    HGDIOBJ oldPen = SelectObject(dc, bezelPen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, X(c, 18), Y(c, 10), X(c, 1518), Y(c, 1012), S(c, 16), S(c, 16));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(bezelPen);
    HPEN innerGlow = CreatePen(PS_SOLID, 1, RGB(0, 18, 4));
    oldPen = SelectObject(dc, innerGlow);
    oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, X(c, 30), Y(c, 26), X(c, 1506), Y(c, 998), S(c, 12), S(c, 12));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(innerGlow);
    DrawVignette(dc, c);
  }

  static void DrawHeader(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    Text(dc, R(c, 66, 38, 340, 34), L"MONIX v0.8.7", ctx.fonts.title, kGreenBright, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, true);
    Text(dc, R(c, 548, 36, 440, 30), L"*** MONIX CORE SYSTEM MONITOR ***", ctx.fonts.body, kGreen2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, 1100, 38, 116, 28), ClockText(), ctx.fonts.body, kGreenBright, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, 1230, 38, 120, 28), DateText(), ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, 1377, 38, 108, 28), L"[-] [ ] [X]", ctx.fonts.smallText, kGreen2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Line(dc, X(c, 50), Y(c, 76), X(c, 1486), Y(c, 76), kGreen, 1, PS_DOT);
  }

  static void DrawPanel(HDC dc, const Canvas& c, int x, int y, int w, int h, const std::wstring& title, HFONT font, bool center = false) {
    RECT rect = R(c, x, y, w, h);
    Fill(dc, rect, kPanel);
    Outline(dc, rect, kGreen3);
    Outline(dc, R(c, x + 1, y + 1, w - 2, h - 2), kGreen5);
    HPEN glowPen = CreatePen(PS_SOLID, 1, RGB(0, 18, 4));
    HGDIOBJ oldPen = SelectObject(dc, glowPen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, X(c, x - 1), Y(c, y - 1), X(c, x + w + 1), Y(c, y + h + 1));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(glowPen);
    RECT titleRect = R(c, x + 10, y + 4, w - 20, 24);
    Text(dc, titleRect, title, font, kGreen, (center ? DT_CENTER : DT_LEFT) | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawSidebar(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    DrawPanel(dc, c, 36, 88, 230, 290, L"MAIN MENU", ctx.fonts.body);
    static const std::array<const wchar_t*, kMenuCount> items {
      L"OVERVIEW", L"LOGS", L"TASKS", L"USAGE", L"EVENTS", L"SECURITY", L"SHADERS", L"NETWORK", L"SETTINGS"
    };
    const int active = std::clamp(ctx.menuIndex, 0, kMenuCount - 1);
    for (int i = 0; i < kMenuCount; ++i) {
      const int y = 136 + i * 26;
      RECT row = R(c, 46, y, 204, 25);
      if (i == active) {
        Fill(dc, row, kGreen);
        RECT glowRow = row;
        OffsetRect(&glowRow, 0, -1);
        Fill(dc, glowRow, RGB(0, 40, 8));
        OffsetRect(&glowRow, 0, 2);
        Fill(dc, glowRow, RGB(0, 40, 8));
        Text(dc, R(c, 60, y, 190, 25), L"> " + std::wstring(items[i]), ctx.fonts.body, RGB(1, 12, 2), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
      } else {
        Text(dc, R(c, 60, y, 190, 25), L"> " + std::wstring(items[i]), ctx.fonts.body, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    }

    DrawPanel(dc, c, 36, 390, 230, 178, L"MONIX CORE", ctx.fonts.smallText);
    const Snapshot& s = SnapshotOrDefault(ctx);
    const std::wstring renderer = ctx.rendererName.empty() ? L"VULKAN 1.3" : ShortText(ctx.rendererName, 17);
    const std::wstring buildType =
#ifdef NDEBUG
      L"RELEASE"
#else
      L"DEBUG"
#endif
    ;
    const std::array<std::wstring, 7> core {
      L"ENGINE.....RUNNING",
      L"RENDERER..." + renderer,
      L"BUILD......" + buildType,
      L"VERSION....0.8.7-dev",
      L"ARCH.......x64",
      L"THREADS...." + (s.threadCount > 0 ? std::to_wstring(s.threadCount) : L"N/A"),
      L"HANDLES...." + std::to_wstring(std::max(0, s.handleCount > 0 ? s.handleCount : 31247))
    };
    for (int i = 0; i < static_cast<int>(core.size()); ++i) {
      TextLine(dc, c, 44, 420 + i * 20, 208, core[i], ctx.fonts.smallText, ctx.fonts.smallLineHeight, kGreen);
    }

    DrawPanel(dc, c, 36, 580, 230, 170, L"SYSTEM INFO", ctx.fonts.smallText);
    const std::wstring cpu = s.cpuBrand[0] ? Utf8ToWide(s.cpuBrand) : L"UNKNOWN CPU";
    const std::wstring gpu = s.gpuModel.empty() ? L"UNKNOWN GPU" : s.gpuModel;
    const int cpuCores = static_cast<int>(s.cpuCores);
    const int cpuThreads = static_cast<int>(s.cpuLogicalCpus);
    const std::array<std::wstring, 6> info {
      L"OS.......WIN 11",
      L"CPU......" + ShortText(cpu, 16),
      L"CORES...." + (cpuCores > 0 && cpuThreads > 0 ? std::to_wstring(cpuCores) + L"C/" + std::to_wstring(cpuThreads) + L"T" : L"N/A"),
      L"GPU......" + ShortText(gpu, 16),
      L"RAM......" + (s.ramTotalBytes ? Fixed(BytesToGb(s.ramTotalBytes), 0) + L" GB" : L"N/A"),
      L"DRIVER..." + (s.gpuDriverVersion.empty() ? L"UNKNOWN" : ShortText(s.gpuDriverVersion, 13))
    };
    for (int i = 0; i < static_cast<int>(info.size()); ++i) {
      TextLine(dc, c, 44, 620 + i * 20, 208, info[i], ctx.fonts.smallText, ctx.fonts.smallLineHeight, kGreen2);
    }

    DrawPanel(dc, c, 36, 762, 230, 172, L"", ctx.fonts.smallText);
    DrawPixelMonix(dc, c, R(c, 80, 802, 116, 88));
  }

  static const Snapshot& SnapshotOrDefault(const CoreMonitorThemeContext& ctx) {
    static const Snapshot fallback {};
    return ctx.snapshot ? *ctx.snapshot : fallback;
  }

  static void DrawMetricCard(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx,
                             int x, const std::wstring& title, const std::wstring& value,
                             const std::wstring& subLeft, const std::wstring& subRight,
                             const std::vector<double>* history, double maxValue, bool bar, bool isPercentage = false) {
    DrawPanel(dc, c, x, 138, 172, 156, title, ctx.fonts.smallText, true);
    double valPct = PercentTextValue(value);
    COLORREF valueColor = isPercentage ? ThresholdColor(valPct) : kGreen;
    Text(dc, R(c, x + 20, 170, 132, 42), value, ctx.fonts.title, valueColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (bar) {
      DrawSegmentedBar(dc, c, R(c, x + 16, 218, 140, 24), PercentTextValue(value), 16);
    } else {
      DrawMiniBars(dc, c, R(c, x + 14, 213, 144, 38), history, maxValue);
    }
    TextLine(dc, c, x + 12, 263, 72, subLeft, ctx.fonts.smallText, ctx.fonts.smallLineHeight, kGreen);
    Text(dc, R(c, x + 86, 263, 80, 22), subRight, ctx.fonts.smallText, kGreen, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static double PercentTextValue(const std::wstring& text) {
    try {
      return std::stod(text);
    } catch (...) {
      return 0.0;
    }
  }

  static void DrawOverview(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const double refreshRate = s.displayRefreshRateHz > 0 ? static_cast<double>(s.displayRefreshRateHz) : 60.0;
    const double cpu = PercentOr(s.cpuPct, 37.0);
    const double ram = s.ramTotalBytes ? std::clamp(BytesToGb(s.ramUsedBytes) * 100.0 / std::max(0.1, BytesToGb(s.ramTotalBytes)), 0.0, 100.0) : 36.8;
    const double gpu = PercentOr(s.gpuPct, 42.0);
    RECT healthBar = R(c, 288, 86, 772, 34);
    Fill(dc, healthBar, kPanel);
    Outline(dc, healthBar, kGreen3);
    Text(dc, healthBar, L"SYSTEM OVERVIEW",
      ctx.fonts.body, kGreen, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawPanel(dc, c, 288, 124, 772, 412, L"", ctx.fonts.body, true);
    Text(dc, R(c, 1010, 92, 50, 20), Fixed(refreshRate, 0) + L" Hz", ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const double cpuGhz = s.estimatedFrequencyMhz > 0.0 ? s.estimatedFrequencyMhz / 1000.0 : 4.82;
    const double cpuTemp = s.cpuTempC > 0.0 ? s.cpuTempC : 58.0;
    const double gpuTemp = s.gpuTempC > 0.0 ? s.gpuTempC : 61.0;
    const double ramUsed = s.ramUsedBytes ? BytesToGb(s.ramUsedBytes) : 11.8;
    const double ramTotal = s.ramTotalBytes ? BytesToGb(s.ramTotalBytes) : 32.0;
    const double vramUsed = s.gpuVramUsedBytes ? BytesToGb(s.gpuVramUsedBytes) : 8.2;

    DrawMetricCard(dc, c, ctx, 300, L"CPU", Fixed(cpu, 0) + L"%", Fixed(cpuGhz, 2) + L" GHz", Fixed(cpuTemp, 0) + L"C", ctx.cpuHistory, 100.0, false, true);
    DrawMetricCard(dc, c, ctx, 490, L"MEMORY", Fixed(ramUsed, 1) + L" GB", L"/ " + Fixed(ramTotal, 0) + L" GB", Fixed(ram, 1) + L"%", ctx.ramHistory, 100.0, true, true);
    DrawMetricCard(dc, c, ctx, 680, L"GPU", Fixed(gpu, 0) + L"%", Fixed(gpuTemp, 0) + L"C", Fixed(vramUsed, 1) + L" GB VRAM", ctx.gpuHistory, 100.0, false, true);
    DrawMetricCard(dc, c, ctx, 870, L"NETWORK", RateTextShortOrUnavailable(s.netDownBytesPerSec), L"DN " + RateTextShortOrUnavailable(s.netDownBytesPerSec), L"UP " + RateTextShortOrUnavailable(s.netUpBytesPerSec), ctx.netHistory, 70.0, false);

    DrawPerformanceGraph(dc, c, ctx, R(c, 292, 314, 768, 220));
    DrawProcessTable(dc, c, ctx, R(c, 288, 546, 526, 208), 5);
    DrawGpuFramePanel(dc, c, ctx, R(c, 815, 546, 245, 128));
    DrawMemoryPanel(dc, c, ctx, R(c, 815, 681, 245, 74));
  }

  static void DrawRightRail(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    DrawEventStream(dc, c, ctx, R(c, 1072, 86, 428, 336));
    DrawStatusPanel(dc, c, ctx, R(c, 1072, 432, 428, 166));
    DrawDiskPanel(dc, c, ctx, R(c, 1072, 608, 428, 146));
  }

  static void DrawPerformanceGraph(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Outline(dc, Pad(panel, S(c,1), S(c,1), S(c,1), S(c,1)), kGreen5);
    Text(dc, RECT { panel.left + S(c, 6), panel.top + S(c, 6), panel.right - S(c, 6), panel.top + S(c, 30) },
      L"PERFORMANCE GRAPH (60s)", ctx.fonts.body, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const int plotL = panel.left + S(c, 50);
    const int plotT = panel.top + S(c, 40);
    const int plotR = panel.right - S(c, 170);
    const int plotB = panel.bottom - S(c, 28);
    RECT plot = { plotL, plotT, plotR, plotB };
    Fill(dc, plot, RGB(0, 4, 1));
    Outline(dc, plot, kGreen4);
    for (int i = 1; i < 7; ++i) {
      const int x = plot.left + (plot.right - plot.left) * i / 7;
      Line(dc, x, plot.top, x, plot.bottom, kGreen5, 1, PS_DOT);
    }
    for (int i = 1; i < 4; ++i) {
      const int y = plot.top + (plot.bottom - plot.top) * i / 4;
      Line(dc, plot.left, y, plot.right, y, kGreen5, 1, PS_DOT);
    }
    const std::array<const wchar_t*, 5> labels { L"100%", L"75%", L"50%", L"25%", L"0%" };
    for (int i = 0; i < 5; ++i) {
      Text(dc, RECT { panel.left + S(c, 4), plot.top + (plot.bottom - plot.top) * i / 4 - S(c, 10), panel.left + S(c, 46), plot.top + (plot.bottom - plot.top) * i / 4 + S(c, 12) },
        labels[i], ctx.fonts.smallText, kGreen3, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    const std::array<const wchar_t*, 5> timeLabels { L"-60s", L"-45s", L"-30s", L"-15s", L"NOW" };
    for (int i = 0; i < 5; ++i) {
      const int tx = plot.left + (plot.right - plot.left) * i / 4 - S(c, 16);
      Text(dc, RECT { tx, panel.bottom - S(c, 22), tx + S(c, 40), panel.bottom - S(c, 2) },
        timeLabels[i], ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    DrawHistoryLine(dc, plot, ctx.cpuHistory, 100.0, kGreen, 0);
    DrawHistoryLine(dc, plot, ctx.gpuHistory, 100.0, kAccent, 5);
    DrawHistoryLine(dc, plot, ctx.ramHistory, 100.0, kGreen2, 11);
    DrawGeneratedLine(dc, plot, 51.0, 16);

    const double cpu = PercentOr(SnapshotOrDefault(ctx).cpuPct, 37.0);
    const double gpu = PercentOr(SnapshotOrDefault(ctx).gpuPct, 42.0);
    const double ram = SnapshotOrDefault(ctx).ramTotalBytes ?
      BytesToGb(SnapshotOrDefault(ctx).ramUsedBytes) * 100.0 / std::max(0.1, BytesToGb(SnapshotOrDefault(ctx).ramTotalBytes)) : 36.8;
    const double vram = SnapshotOrDefault(ctx).gpuVramTotalBytes ?
      BytesToGb(SnapshotOrDefault(ctx).gpuVramUsedBytes) * 100.0 / std::max(0.1, BytesToGb(SnapshotOrDefault(ctx).gpuVramTotalBytes)) : 51.0;
    const std::array<std::wstring, 4> legend {
      L"--- CPU   " + Fixed(cpu, 0) + L"%",
      L"--- GPU   " + Fixed(gpu, 0) + L"%",
      L"--- RAM   " + Fixed(ram, 0) + L"%",
      L"--- VRAM  " + Fixed(vram, 0) + L"%"
    };
    const std::array<COLORREF, 4> legendColors { kGreen, kAccent, kGreen2, kGreen3 };
    const int legX = panel.right - S(c, 164);
    const int legY = panel.top + S(c, 50);
    for (int i = 0; i < 4; ++i) {
      TextLine(dc, c, legX, legY + i * 18, 80, legend[i], ctx.fonts.smallText, ctx.fonts.smallLineHeight, legendColors[i]);
    }
  }

  static void DrawHistoryLine(HDC dc, const RECT& plot, const std::vector<double>* values, double maxValue, COLORREF color, int phase) {
    if (!values || values->size() < 2 || maxValue <= 0.0) {
      DrawGeneratedLine(dc, plot, 40.0 + phase, phase);
      return;
    }
    HRGN clip = CreateRectRgn(plot.left + 1, plot.top + 1, plot.right - 1, plot.bottom - 1);
    SelectClipRgn(dc, clip);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const std::size_t begin = values->size() > 72 ? values->size() - 72 : 0;
    const std::size_t count = values->size() - begin;
    for (std::size_t i = 0; i < count; ++i) {
      const double v = std::clamp((*values)[begin + i] / maxValue, 0.0, 1.0);
      const int x = plot.left + static_cast<int>((plot.right - plot.left - 2) * i / std::max<std::size_t>(1, count - 1));
      const int y = plot.bottom - 2 - static_cast<int>((plot.bottom - plot.top - 4) * v);
      if (i == 0) MoveToEx(dc, x, y, nullptr);
      else LineTo(dc, x, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    SelectClipRgn(dc, nullptr);
    DeleteObject(clip);
  }

  static void DrawGeneratedLine(HDC dc, const RECT& plot, double basePct, int phase) {
    std::vector<POINT> pts;
    pts.reserve(64);
    for (int i = 0; i < 64; ++i) {
      const double wave = std::sin((i + phase) * 0.31) * 8.0 + std::sin((i + phase) * 0.77) * 3.0;
      const double pct = std::clamp(basePct + wave, 0.0, 100.0);
      pts.push_back(POINT {
        plot.left + (plot.right - plot.left - 2) * i / 63,
        plot.bottom - 2 - static_cast<int>((plot.bottom - plot.top - 4) * pct / 100.0)
      });
    }
    HPEN pen = CreatePen(PS_SOLID, 1, kGreen2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    for (std::size_t i = 0; i < pts.size(); ++i) {
      if (i == 0) MoveToEx(dc, pts[i].x, pts[i].y, nullptr);
      else LineTo(dc, pts[i].x, pts[i].y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  static void DrawMiniBars(HDC dc, const Canvas& c, const RECT& rect, const std::vector<double>* values, double maxValue) {
    Fill(dc, rect, kPanelDeep);
    const int bars = 28;
    const int gap = S(c, 2);
    const int barW = std::max(1, static_cast<int>((rect.right - rect.left - gap * (bars - 1)) / bars));
    for (int i = 0; i < bars; ++i) {
      double value = 20.0 + std::fmod(i * 17 + 13, 46.0);
      if (values && !values->empty()) {
        const int idx = std::max(0, static_cast<int>(values->size()) - bars + i);
        value = (*values)[std::min<std::size_t>(values->size() - 1, static_cast<std::size_t>(idx))];
      }
      const double pct = std::clamp(value / std::max(1.0, maxValue), 0.0, 1.0);
      RECT bar {
        rect.left + i * (barW + gap),
        rect.bottom - std::max(2, static_cast<int>((rect.bottom - rect.top) * pct)),
        rect.left + i * (barW + gap) + barW,
        rect.bottom
      };
      COLORREF barColor = i % 5 == 0 ? kGreen : kGreen2;
      Fill(dc, bar, barColor);
      if (pct > 0.1) {
        RECT highlight { bar.left, bar.top, bar.right, bar.top + std::max(1, S(c, 2)) };
        Fill(dc, highlight, kGreenBright);
      }
    }
  }

  static void DrawSegmentedBar(HDC dc, const Canvas& c, const RECT& rect, double pct, int segments) {
    Fill(dc, rect, kPanelDeep);
    Outline(dc, rect, kGreen4);
    const int filled = std::clamp(static_cast<int>(std::round(pct * segments / 100.0)), 0, segments);
    const int gap = S(c, 2);
    const int pad = S(c, 4);
    const int segW = std::max(2, static_cast<int>((rect.right - rect.left - pad * 2 - gap * (segments - 1)) / segments));
    for (int i = 0; i < segments; ++i) {
      RECT seg {
        rect.left + pad + i * (segW + gap),
        rect.top + S(c, 5),
        rect.left + pad + i * (segW + gap) + segW,
        rect.bottom - S(c, 5)
      };
      if (i < filled) {
        COLORREF fill = (pct > 85.0 && i >= segments - 3) ? kWarn : (i < filled - 1 ? kGreen : kGreenBright);
        Fill(dc, seg, fill);
      } else {
        Fill(dc, seg, kGreen5);
      }
    }
  }

  static std::vector<ProcessInfo> FallbackProcesses() {
    return {
      { L"chrome.exe", 18472, 0, 0, 12.4, 1932735283ull, 2.0, 0, L"RUNNING", L"NORMAL", L"" },
      { L"Monix.exe", 8216, 0, 0, 4.2, 440401920ull, 8.0, 0, L"RUNNING", L"HIGH", L"" },
      { L"explorer.exe", 4920, 0, 0, 1.8, 220200960ull, 0.0, 0, L"RUNNING", L"NORMAL", L"" },
      { L"Spotify.exe", 15332, 0, 0, 2.1, 335544320ull, 1.0, 0, L"RUNNING", L"NORMAL", L"" },
      { L"Discord.exe", 11156, 0, 0, 3.7, 293601280ull, 2.0, 0, L"RUNNING", L"NORMAL", L"" },
    };
  }

  static void DrawProcessTable(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel, int rows) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 6), panel.right - S(c, 12), panel.top + S(c, 30) },
      L"TOP PROCESSES", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const int y0 = panel.top + S(c, 34);
    Text(dc, RECT { panel.left + S(c, 18), y0, panel.right - S(c, 8), y0 + S(c, 22) },
      L"PID    PROCESS          CPU%   RAM      GPU%   STATUS", ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Line(dc, panel.left + S(c, 14), y0 + S(c, 25), panel.right - S(c, 14), y0 + S(c, 25), kGreen5, 1, PS_DOT);

    std::vector<ProcessInfo> processes = SnapshotOrDefault(ctx).processes;
    if (processes.empty()) {
      processes = FallbackProcesses();
    }
    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
      return (a.cpuPct + a.gpuPct) > (b.cpuPct + b.gpuPct);
    });
    for (int i = 0; i < rows && i < static_cast<int>(processes.size()); ++i) {
      const ProcessInfo& p = processes[i];
      COLORREF bgColor = (i % 2 == 0) ? kPanel : RGB(3, 16, 5);
      RECT rowBg = { panel.left + S(c, 12), y0 + S(c, 28 + i * 23) - S(c, 2), panel.right - S(c, 8), y0 + S(c, 28 + i * 23) + S(c, 21) };
      Fill(dc, rowBg, bgColor);
      std::wostringstream row;
      row << std::setw(5) << p.pid << L"  "
          << std::left << std::setw(14) << ShortText(p.name, 14) << std::right
          << std::setw(5) << Fixed(p.cpuPct, 1) << L"  "
          << std::setw(7) << FormatBytes(p.ramBytes) << L"  "
          << std::setw(3) << Fixed(p.gpuPct, 0) << L"    "
          << (p.status.empty() ? L"RUNNING" : p.status);
      COLORREF statusColor = kGreen;
      if (p.cpuPct > 50.0) statusColor = kWarn;
      if (p.cpuPct > 80.0) statusColor = kError;
      Text(dc, RECT { panel.left + S(c, 18), y0 + S(c, 28 + i * 23), panel.right - S(c, 8), y0 + S(c, 50 + i * 23) },
        row.str(), ctx.fonts.smallText, statusColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    Text(dc, RECT { panel.right - S(c, 190), panel.bottom - S(c, 28), panel.right - S(c, 20), panel.bottom - S(c, 6) },
      L"VIEW ALL (ENTER)", ctx.fonts.smallText, kGreen, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawGpuFramePanel(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 6), panel.right - S(c, 10), panel.top + S(c, 28) },
      L"GPU FRAME TIME", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const double frame = SnapshotOrDefault(ctx).frameTimeMs > 0.01 ? SnapshotOrDefault(ctx).frameTimeMs : 3.82;
    const int refreshRate = SnapshotOrDefault(ctx).displayRefreshRateHz > 0 ? SnapshotOrDefault(ctx).displayRefreshRateHz : 60;
    const double maxFrameMs = 1000.0 / static_cast<double>(refreshRate);
    const double fps = frame > 0.01 ? 1000.0 / frame : 0.0;
    const bool frameOk = frame <= maxFrameMs;
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 28), panel.right - S(c, 12), panel.top + S(c, 52) },
      Fixed(frame, 2) + L" ms", ctx.fonts.title, frameOk ? kGreen : kError, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 52), panel.right - S(c, 12), panel.top + S(c, 70) },
      Fixed(fps, 1) + L" FPS  GPU " + Fixed(SnapshotOrDefault(ctx).gpuPct > 0 ? SnapshotOrDefault(ctx).gpuPct : 42.0, 0) + L"%",
      ctx.fonts.smallText, frameOk ? kGreen3 : kWarn, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT graph = { panel.left + S(c, 12), panel.top + S(c, 72), panel.right - S(c, 12), panel.bottom - S(c, 16) };
    Fill(dc, graph, RGB(0, 4, 1));
    Outline(dc, graph, kGreen4);
    DrawGeneratedLine(dc, graph, 45.0, static_cast<int>(ctx.frameCount % 20));
    Text(dc, RECT { panel.right - S(c, 80), graph.top - S(c, 6), panel.right - S(c, 12), graph.top + S(c, 12) },
      Fixed(maxFrameMs, 1) + L"ms", ctx.fonts.smallText, kGreen3, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawMemoryPanel(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    const Snapshot& s = SnapshotOrDefault(ctx);
    const double used = s.ramUsedBytes ? BytesToGb(s.ramUsedBytes) : 11.8;
    const double total = s.ramTotalBytes ? BytesToGb(s.ramTotalBytes) : 32.0;
    const double pct = std::clamp(used * 100.0 / std::max(0.1, total), 0.0, 100.0);
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 5), panel.right - S(c, 12), panel.top + S(c, 28) },
      L"MEMORY USAGE", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 31), panel.right - S(c, 12), panel.top + S(c, 54) },
      Fixed(used, 1) + L"/" + Fixed(total, 0) + L"GB " + Fixed(pct, 0) + L"%", ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    DrawSegmentedBar(dc, c, RECT { panel.left + S(c, 14), panel.bottom - S(c, 18), panel.right - S(c, 14), panel.bottom - S(c, 8) }, pct, 22);
  }

  static std::vector<std::wstring> SampleEventRows() {
    return {
      L"22:59 [I] PROC  chrome.exe started",
      L"22:59 [I] MEM   alloc +84 MB",
      L"22:59 [D] GPU   queue submitted",
      L"22:59 [W] NET   latency high",
      L"22:59 [I] SHADER cache hit",
      L"22:59 [I] SYS   telemetry updated",
      L"22:59 [D] TASKS 2 procs updated",
      L"22:59 [I] SEC   heuristic scan done",
      L"22:59 [W] DISK  usage above 85%",
      L"22:59 [I] NET   connection ok",
      L"22:59 [D] GPU   frame: 3.82 ms",
      L"22:59 [I] SYS   backup done"
    };
  }

  static std::vector<std::wstring> BuildEventRows(const CoreMonitorThemeContext& ctx, int maxRows) {
    std::vector<std::wstring> rows;
    if (ctx.logs && !ctx.logs->empty()) {
      const int begin = std::max(0, static_cast<int>(ctx.logs->size()) - maxRows);
      for (int i = begin; i < static_cast<int>(ctx.logs->size()); ++i) {
        const LogEntry& e = (*ctx.logs)[i];
      std::wstring time = e.time.empty() ? ClockText() : e.time;
      if (time.size() > 5) {
        time = time.substr(time.size() - 5);
      }
        std::wstring sev = e.severity.empty() ? L"I" : e.severity;
        if (sev.size() > 1) {
          sev = sev.substr(0, 1);
        }
        std::wstring dom = e.domain.empty() ? L"SYS" : ShortText(e.domain, 5);
        rows.push_back(time + L" [" + sev + L"] " + PadRight(dom, 5) + L" " + ShortText(e.message, 22));
      }
    }
    if (rows.empty()) {
      rows = SampleEventRows();
    }
    if (static_cast<int>(rows.size()) > maxRows) {
      rows.erase(rows.begin(), rows.end() - maxRows);
    }
    return rows;
  }

  static void DrawEventStream(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Outline(dc, Pad(panel, S(c,2), S(c,2), S(c,2), S(c,2)), kGreen5);
    Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 8), panel.right - S(c, 12), panel.top + S(c, 34) },
      ctx.livePaused ? L"EVENT STREAM [PAUSED]" : L"EVENT STREAM [LIVE]", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const auto rows = BuildEventRows(ctx, 13);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      COLORREF color = SeverityColor(rows[i]);
      Text(dc, RECT { panel.left + S(c, 12), panel.top + S(c, 38 + i * 20), panel.right - S(c, 12), panel.top + S(c, 58 + i * 20) },
        rows[i], ctx.fonts.smallText, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
    Text(dc, RECT { panel.left + S(c, 12), panel.bottom - S(c, 28), panel.right - S(c, 12), panel.bottom - S(c, 8) },
      L"VIEW ALL (ENTER)", ctx.fonts.smallText, kGreen3, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static std::wstring StatusLine(const std::wstring& label) {
    const int target = 33;
    std::wstring out = label;
    if (static_cast<int>(out.size()) < target) {
      out.append(target - out.size(), L'.');
    }
    return out + L"[ OK ]";
  }

  static void DrawStatusPanel(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Outline(dc, Pad(panel, S(c,2), S(c,2), S(c,2), S(c,2)), kGreen5);
    Text(dc, RECT { panel.left + S(c, 14), panel.top + S(c, 6), panel.right - S(c, 12), panel.top + S(c, 30) },
      L"SYSTEM STATUS", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const Snapshot& s = SnapshotOrDefault(ctx);
    const double cpuPct = PercentOr(s.cpuPct, 37.0);
    const double ramPct = s.ramTotalBytes ? BytesToGb(s.ramUsedBytes) * 100.0 / std::max(0.1, BytesToGb(s.ramTotalBytes)) : 36.8;
    const double gpuPct = PercentOr(s.gpuPct, 42.0);
    const double diskPct = s.diskPctUsed > 0.0 ? s.diskPctUsed : 48.0;
    const double gpuTemp = s.gpuTempC > 0.0 ? s.gpuTempC : 61.0;
    const double netDownMB = s.netDownBytesPerSec > 0 ? static_cast<double>(s.netDownBytesPerSec) / (1024.0 * 1024.0) : 0.0;
    const double netPct = std::min(100.0, netDownMB * 2.0);
    struct SubStatus { const wchar_t* name; double pct; const std::wstring detail; };
    const std::array<SubStatus, 6> subs {{
      { L"CPU",     cpuPct,  Fixed(cpuPct, 0) + L"%  " + Fixed(s.cpuTempC > 0.0 ? s.cpuTempC : 58.0, 0) + L"C" },
      { L"MEMORY",  ramPct,  Fixed(ramPct, 0) + L"%  " + (s.ramTotalBytes ? Fixed(BytesToGb(s.ramUsedBytes), 1) + L"/" + Fixed(BytesToGb(s.ramTotalBytes), 0) + L"GB" : L"") },
      { L"GPU",     gpuPct,  Fixed(gpuPct, 0) + L"%  " + Fixed(gpuTemp, 0) + L"C" },
      { L"STORAGE", diskPct, Fixed(diskPct, 0) + L"%  " + (s.diskTotalBytes ? Fixed(BytesToGb(s.diskTotalBytes - s.diskFreeBytes), 0) + L"/" + Fixed(BytesToGb(s.diskTotalBytes), 0) + L"GB" : L"") },
      { L"NETWORK", netPct,  L"DN " + RateTextShort(s.netDownBytesPerSec, 0.0) + L" UP " + RateTextShort(s.netUpBytesPerSec, 0.0) },
      { L"SECURITY", s.selfSignatureValid == 0 ? 90.0 : s.selfSignatureValid < 0 ? 50.0 : 5.0, s.selfSignatureValid == 0 ? L"SIGN FAIL" : s.selfSignatureValid < 0 ? L"UNKNOWN" : L"VERIFIED" }
    }};
    for (int i = 0; i < static_cast<int>(subs.size()); ++i) {
      const bool crit = subs[i].pct >= 85.0;
      const bool warn = subs[i].pct >= 65.0 && !crit;
      const wchar_t* badge = crit ? L"[CRIT]" : warn ? L"[WARN]" : L"[ OK ]";
      COLORREF color = crit ? kError : warn ? kWarn : kGreen2;
      Text(dc, RECT { panel.left + S(c, 14), panel.top + S(c, 34 + i * 22), panel.left + S(c, 110), panel.top + S(c, 56 + i * 22) },
        std::wstring(subs[i].name), ctx.fonts.smallText, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, RECT { panel.left + S(c, 112), panel.top + S(c, 34 + i * 22), panel.left + S(c, 186), panel.top + S(c, 56 + i * 22) },
        badge, ctx.fonts.smallText, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, RECT { panel.left + S(c, 190), panel.top + S(c, 34 + i * 22), panel.right - S(c, 12), panel.top + S(c, 56 + i * 22) },
        subs[i].detail, ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
  }

  static void DrawDiskPanel(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& panel) {
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Outline(dc, Pad(panel, S(c,2), S(c,2), S(c,2), S(c,2)), kGreen5);
    Text(dc, RECT { panel.left + S(c, 14), panel.top + S(c, 6), panel.right - S(c, 12), panel.top + S(c, 30) },
      L"DISK USAGE", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const Snapshot& s = SnapshotOrDefault(ctx);
    const double diskPct = s.diskPctUsed > 0.0 ? s.diskPctUsed : 48.0;
    const double diskUsedGB = s.diskTotalBytes ? BytesToGb(s.diskTotalBytes - s.diskFreeBytes) : 231.0;
    const double diskTotalGB = s.diskTotalBytes ? BytesToGb(s.diskTotalBytes) : 476.0;
    const std::array<std::wstring, 3> labels {
      L"C:\\  " + Fixed(diskUsedGB, 0) + L" / " + Fixed(diskTotalGB, 0) + L" GB [" + Fixed(diskPct, 0) + L"%]",
      L"READ.." + RateTextShort(s.diskReadBytesPerSec, 0.0),
      L"WRITE." + RateTextShort(s.diskWriteBytesPerSec, 0.0)
    };
    const std::array<double, 3> pcts { diskPct,
      std::min(100.0, static_cast<double>(s.diskReadBytesPerSec) / (50.0 * 1024.0 * 1024.0) * 100.0),
      std::min(100.0, static_cast<double>(s.diskWriteBytesPerSec) / (50.0 * 1024.0 * 1024.0) * 100.0)
    };
    for (int i = 0; i < 3; ++i) {
      const int y = panel.top + S(c, 38) + i * S(c, 36);
      Text(dc, RECT { panel.left + S(c, 16), y, panel.left + S(c, 220), y + S(c, 22) }, labels[i], ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      DrawSegmentedBar(dc, c, RECT { panel.left + S(c, 225), y + S(c, 4), panel.right - S(c, 16), y + S(c, 16) }, pcts[i], 20);
    }
  }

  static std::vector<std::wstring> SampleLogRows() {
    return {
      L"22:59:38.421  [INFO ] [ProcessMonitor ] Process 'chrome.exe' (PID: 18472) started successfully.",
      L"22:59:38.742  [INFO ] [MemoryManager ] Allocated 84 MB to process 'chrome.exe' (PID: 18472).",
      L"22:59:39.105  [DEBUG] [GPUQueue      ] Command buffer submitted to graphics queue (ID: 0x7f2a).",
      L"22:59:40.225  [WARN ] [NetworkMonitor] Latency 128ms is above threshold (100ms).",
      L"22:59:40.982  [INFO ] [ShaderCache   ] Pipeline 'Base.slangp' cache hit (98.7%).",
      L"22:59:41.337  [INFO ] [Telemetry     ] System telemetry data updated successfully.",
      L"22:59:41.884  [DEBUG] [TaskManager   ] 2 processes updated. Total: 128 active.",
      L"22:59:42.201  [WARN ] [DiskMonitor   ] Drive D:\\ usage is above 85% (812 GB / 931 GB)."
    };
  }

  static std::vector<std::wstring> BuildLogRows(const CoreMonitorThemeContext& ctx, int maxRows) {
    std::vector<std::wstring> rows;
    if (ctx.logs && !ctx.logs->empty()) {
      const int begin = std::max(0, static_cast<int>(ctx.logs->size()) - maxRows);
      for (int i = begin; i < static_cast<int>(ctx.logs->size()); ++i) {
        const LogEntry& e = (*ctx.logs)[i];
        const std::wstring time = e.time.empty() ? ClockText() : e.time;
        rows.push_back(time + L"  [" + PadRight(e.severity.empty() ? L"INFO" : ShortText(e.severity, 5), 5) + L"] [" +
          PadRight(ShortText(e.module.empty() ? e.domain : e.module, 15), 15) + L"] " + e.message);
      }
    }
    if (rows.empty()) {
      rows = SampleLogRows();
    }
    if (static_cast<int>(rows.size()) > maxRows) {
      rows.erase(rows.begin(), rows.end() - maxRows);
    }
    return rows;
  }

  static void DrawLogPanel(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    RECT panel = R(c, 288, 762, 1200, 176);
    Fill(dc, panel, kPanel);
    Outline(dc, panel, kGreen3);
    Outline(dc, Pad(panel, S(c,1), S(c,1), S(c,1), S(c,1)), kGreen5);
    Text(dc, R(c, 292, 770, 180, 24), L"LOGS [ALL]", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, 1210, 770, 180, 24), L"(F1=DEBUG) (F5=CLEAR)", ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    const auto rows = BuildLogRows(ctx, 7);
    int infoCount = 0, warnCount = 0, errCount = 0;
    for (const auto& r : rows) {
      if (r.find(L"WARN") != std::wstring::npos) ++warnCount;
      else if (r.find(L"ERROR") != std::wstring::npos) ++errCount;
      else ++infoCount;
    }
    Line(dc, X(c, 292), Y(c, 789), X(c, 1210), Y(c, 789), kGreen5, 1, PS_DOT);
    RECT infoBar = R(c, 1220, 773, 80, 16);
    RECT warnBar = R(c, 1308, 773, 50, 16);
    RECT errBar = R(c, 1366, 773, 40, 16);
    if (infoCount > 0) { Fill(dc, infoBar, RGB(0, 28, 6)); Text(dc, infoBar, L"I:" + std::to_wstring(infoCount), ctx.fonts.smallText, kGreen2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    if (warnCount > 0) { Fill(dc, warnBar, RGB(30, 28, 4)); Text(dc, warnBar, L"W:" + std::to_wstring(warnCount), ctx.fonts.smallText, kWarn, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    if (errCount > 0) { Fill(dc, errBar, RGB(30, 8, 4)); Text(dc, errBar, L"E:" + std::to_wstring(errCount), ctx.fonts.smallText, kError, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      COLORREF color = SeverityColor(rows[i]);
      Text(dc, R(c, 294, 798 + i * 20, 1170, 20), rows[i], ctx.fonts.logText, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
  }

  static void DrawFooter(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    RECT footer = R(c, 48, 952, 1438, 36);
    Fill(dc, footer, kPanel);
    Outline(dc, footer, kGreen4);
    const Snapshot& s = SnapshotOrDefault(ctx);
    const int events = ctx.counters ? ctx.counters->info + ctx.counters->debug + ctx.counters->warnings + ctx.counters->errors + ctx.counters->critical : 0;
    const int warnings = ctx.counters ? ctx.counters->warnings : 0;
    const int errors = ctx.counters ? ctx.counters->errors : 0;
    const std::array<std::wstring, 7> cells {
      L"MONIX CORE: RUNNING",
      L"EVENTS: " + std::to_wstring(events),
      L"WARNINGS: " + FixedWidthNumber(warnings, 2),
      L"ERRORS: " + FixedWidthNumber(errors, 2),
      s.uptimeSeconds > 0 ? L"UPTIME: " + Hms(s.uptimeSeconds) : L"UPTIME: N/A",
      ctx.rendererName.empty() ? L"VULKAN 1.3" : ShortText(ctx.rendererName, 18),
      L"READY"
    };
    const std::array<int, 8> stops { 48, 248, 448, 618, 768, 978, 1208, 1486 };
    for (int i = 1; i < static_cast<int>(stops.size()) - 1; ++i) {
      Line(dc, X(c, stops[i]), footer.top, X(c, stops[i]), footer.bottom, kGreen5, 1);
    }
    for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
      COLORREF cellColor = kGreen2;
      if (i == 4) cellColor = kGreen;
      if (i == 6 && errors == 0) cellColor = kGreenBright;
      Text(dc, RECT { X(c, stops[i] + 6), footer.top + S(c, 2), X(c, stops[i + 1] - 6), footer.bottom - S(c, 2) },
        cells[i], ctx.fonts.smallText, cellColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }
  }

  static std::wstring FixedWidthNumber(int value, int width) {
    std::wostringstream out;
    out << std::setw(width) << std::setfill(L'0') << std::max(0, value);
    return out.str();
  }

  static void DrawPixelMonix(HDC dc, const Canvas& c, const RECT& area) {
    static const std::array<const wchar_t*, 7> glyph {
      L"##   ##  ###",
      L"### ### #   #",
      L"####### #   #",
      L"## # ## #   #",
      L"##   ## #   #",
      L"##   ## #   #",
      L"##   ##  ###"
    };
    const int rows = static_cast<int>(glyph.size());
    int cols = 0;
    for (auto row : glyph) {
      cols = std::max(cols, static_cast<int>(wcslen(row)));
    }
    const int cell = std::max(S(c, 3), std::min(static_cast<int>((area.right - area.left) / std::max(1, cols)), static_cast<int>((area.bottom - area.top) / rows)));
    const int left = area.left + ((area.right - area.left) - cols * cell) / 2;
    const int top = area.top + ((area.bottom - area.top) - rows * cell) / 2;
    for (int y = 0; y < rows; ++y) {
      for (int x = 0; glyph[y][x] != 0; ++x) {
        if (glyph[y][x] == L'#') {
          RECT px { left + x * cell, top + y * cell, left + x * cell + cell - 1, top + y * cell + cell - 1 };
          Fill(dc, px, kGreen);
        }
      }
    }
  }

  static void DrawTaskManagerPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    std::vector<ProcessInfo> processes = s.processes;
    if (processes.empty()) processes = FallbackProcesses();
    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
      return (a.cpuPct + a.gpuPct) > (b.cpuPct + b.gpuPct);
    });
    double totalCpu = 0.0, totalRam = 0.0;
    for (const auto& p : processes) { totalCpu += p.cpuPct; totalRam += BytesToGb(p.ramBytes); }
    const int pad = S(c, 16);
    RECT summaryBar = Pad(main, pad, S(c, 54), pad, main.bottom - main.top - S(c, 80));
    summaryBar.bottom = summaryBar.top + S(c, 26);
    Fill(dc, summaryBar, RGB(3, 18, 5));
    Text(dc, Pad(summaryBar, S(c, 6), 0, S(c, 6), 0),
      L"PROCESSES: " + std::to_wstring(processes.size()) + L"   TOTAL CPU: " + Fixed(totalCpu, 1) + L"%   TOTAL RAM: " + Fixed(totalRam, 1) + L" GB",
      ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT table = Pad(main, pad, S(c, 86), pad, S(c, 20));
    DrawProcessTable(dc, c, ctx, table, 18);
  }

  static void DrawSectionPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    static const std::array<const wchar_t*, kMenuCount> titles {
      L"SYSTEM OVERVIEW", L"LOG VIEW", L"TASK MANAGER", L"USAGE", L"EVENT STREAM", L"SECURITY", L"SHADERS", L"NETWORK", L"SETTINGS"
    };
    static const std::array<const wchar_t*, kMenuCount> subtitles {
      L"", L"RECENT SYSTEM LOG ENTRIES",
      L"ACTIVE PROCESSES SORTED BY RESOURCE USAGE",
      L"SYSTEM RESOURCE UTILIZATION OVERVIEW",
      L"REAL-TIME EVENT MONITORING",
      L"SYSTEM INTEGRITY AND SECURITY CHECKS",
      L"GPU SHADER PIPELINE CONFIGURATION",
      L"NETWORK ACTIVITY AND CONNECTION STATUS",
      L"APPLICATION CONFIGURATION AND PREFERENCES"
    };
    const int index = std::clamp(ctx.menuIndex, 0, kMenuCount - 1);
    RECT main = R(c, 288, 86, 772, 666);
    Fill(dc, main, kPanel);
    Outline(dc, main, kGreen3);
    Outline(dc, Pad(main, S(c,1), S(c,1), S(c,1), S(c,1)), kGreen5);
    Text(dc, RECT { main.left + S(c, 12), main.top + S(c, 8), main.right - S(c, 12), main.top + S(c, 36) },
      titles[index], ctx.fonts.body, kGreen, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (index > 0) {
      Text(dc, RECT { main.left + S(c, 20), main.top + S(c, 36), main.right - S(c, 20), main.top + S(c, 52) },
        subtitles[index], ctx.fonts.smallText, kGreen3, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Line(dc, main.left + S(c, 30), main.top + S(c, 54), main.right - S(c, 30), main.top + S(c, 54), kGreen4, 1, PS_DOT);
    }

    switch (index) {
      case 1:
        DrawLogRowsPage(dc, c, ctx, main);
        break;
      case 2:
        DrawTaskManagerPage(dc, c, ctx, main);
        break;
      case 3:
        DrawUsagePage(dc, c, ctx, main);
        break;
      case 4:
        DrawEventRowsPage(dc, c, ctx, main);
        break;
      case 5:
        DrawSecurityPage(dc, c, ctx, main);
        break;
      case 6:
        DrawShadersPage(dc, c, ctx, main);
        break;
      case 7:
        DrawNetworkPage(dc, c, ctx, main);
        break;
      case 8:
        DrawSettingsPage(dc, c, ctx, main);
        break;
      default:
        DrawUsagePage(dc, c, ctx, main);
        break;
    }
  }

  static void DrawLogRowsPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const auto rows = BuildLogRows(ctx, 24);
    int warnCount = 0, errCount = 0, infoCount = 0;
    for (const auto& r : rows) {
      if (r.find(L"WARN") != std::wstring::npos) ++warnCount;
      else if (r.find(L"ERROR") != std::wstring::npos) ++errCount;
      else ++infoCount;
    }
    RECT filterBar = { main.left + S(c, 16), main.top + S(c, 54), main.right - S(c, 16), main.top + S(c, 80) };
    Fill(dc, filterBar, RGB(3, 18, 5));
    Text(dc, { main.left + S(c, 20), main.top + S(c, 54), main.right - S(c, 20), main.top + S(c, 80) },
      L"SHOWING " + std::to_wstring(rows.size()) + L" ENTRIES",
      ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT infoBadge = { main.left + S(c, 280), main.top + S(c, 56), main.left + S(c, 360), main.top + S(c, 76) };
    RECT warnBadge = { main.left + S(c, 370), main.top + S(c, 56), main.left + S(c, 450), main.top + S(c, 76) };
    RECT errBadge = { main.left + S(c, 450), main.top + S(c, 56), main.left + S(c, 530), main.top + S(c, 76) };
    Fill(dc, infoBadge, RGB(0, 24, 6));
    Text(dc, infoBadge, L"I: " + std::to_wstring(infoCount), ctx.fonts.smallText, kGreen2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
    if (warnCount > 0) { Fill(dc, warnBadge, RGB(30, 26, 4)); Text(dc, warnBadge, L"W: " + std::to_wstring(warnCount), ctx.fonts.smallText, kWarn, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    if (errCount > 0) { Fill(dc, errBadge, RGB(30, 8, 4)); Text(dc, errBadge, L"E: " + std::to_wstring(errCount), ctx.fonts.smallText, kError, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    Line(dc, main.left + S(c, 16), main.top + S(c, 82), main.right - S(c, 16), main.top + S(c, 82), kGreen5, 1, PS_DOT);
    int y = main.top + S(c, 90);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      COLORREF bgColor = (i % 2 == 0) ? kPanel : RGB(3, 16, 5);
      RECT rowBg = { main.left + S(c, 14), y - S(c, 2), main.right - S(c, 14), y + S(c, 20) };
      Fill(dc, rowBg, bgColor);
      COLORREF textColor = SeverityColor(rows[i]);
      Text(dc, RECT { main.left + S(c, 18), y, main.right - S(c, 18), y + S(c, 21) },
        rows[i], ctx.fonts.logText, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += S(c, 24);
      if (y > main.bottom - S(c, 18)) break;
    }
    if (static_cast<int>(rows.size()) >= 24) {
      Text(dc, RECT { main.right - S(c, 120), main.bottom - S(c, 20), main.right - S(c, 18), main.bottom - S(c, 4) },
        L"SCROLL FOR MORE", ctx.fonts.smallText, kGreen4, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
  }

  static void DrawEventRowsPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const auto rows = BuildEventRows(ctx, 24);
    int infoC = 0, warnC = 0, errC = 0, debugC = 0;
    for (const auto& r : rows) {
      if (r.find(L"[W]") != std::wstring::npos) ++warnC;
      else if (r.find(L"[E]") != std::wstring::npos) ++errC;
      else if (r.find(L"[D]") != std::wstring::npos) ++debugC;
      else ++infoC;
    }
    RECT filterBar = { main.left + S(c, 16), main.top + S(c, 54), main.right - S(c, 16), main.top + S(c, 80) };
    Fill(dc, filterBar, RGB(3, 18, 5));
    Text(dc, { main.left + S(c, 20), main.top + S(c, 54), main.right - S(c, 20), main.top + S(c, 80) },
      L"EVENTS: " + std::to_wstring(rows.size()), ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT iB = { main.left + S(c, 160), main.top + S(c, 56), main.left + S(c, 224), main.top + S(c, 76) };
    RECT wB = { main.left + S(c, 234), main.top + S(c, 56), main.left + S(c, 298), main.top + S(c, 76) };
    RECT eB = { main.left + S(c, 308), main.top + S(c, 56), main.left + S(c, 372), main.top + S(c, 76) };
    RECT dB = { main.left + S(c, 382), main.top + S(c, 56), main.left + S(c, 446), main.top + S(c, 76) };
    Fill(dc, iB, RGB(0, 24, 6)); Text(dc, iB, L"I:" + std::to_wstring(infoC), ctx.fonts.smallText, kGreen2, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
    if (warnC > 0) { Fill(dc, wB, RGB(30, 26, 4)); Text(dc, wB, L"W:" + std::to_wstring(warnC), ctx.fonts.smallText, kWarn, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    if (errC > 0) { Fill(dc, eB, RGB(30, 8, 4)); Text(dc, eB, L"E:" + std::to_wstring(errC), ctx.fonts.smallText, kError, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false); }
    Fill(dc, dB, RGB(4, 20, 8)); Text(dc, dB, L"D:" + std::to_wstring(debugC), ctx.fonts.smallText, kGreen4, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
    Line(dc, main.left + S(c, 16), main.top + S(c, 82), main.right - S(c, 16), main.top + S(c, 82), kGreen5, 1, PS_DOT);
    int y = main.top + S(c, 90);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      COLORREF bgColor = (i % 2 == 0) ? kPanel : RGB(3, 16, 5);
      RECT rowBg = { main.left + S(c, 14), y - S(c, 2), main.right - S(c, 14), y + S(c, 22) };
      Fill(dc, rowBg, bgColor);
      COLORREF textColor = SeverityColor(rows[i]);
      Text(dc, RECT { main.left + S(c, 22), y, main.right - S(c, 22), y + S(c, 24) },
        rows[i], ctx.fonts.smallText, textColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += S(c, 25);
      if (y > main.bottom - S(c, 18)) break;
    }
  }

  static COLORREF ThresholdColor(double pct) {
    if (pct >= 85.0) return kError;
    if (pct >= 65.0) return kWarn;
    return kGreen;
  }

  static std::wstring UsageStatus(double pct) {
    if (pct >= 85.0) return L"CRITICAL";
    if (pct >= 65.0) return L"ELEVATED";
    if (pct >= 30.0) return L"NOMINAL";
    return L"IDLE";
  }

  static void DrawUsagePage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const double cpu = PercentOr(s.cpuPct, 37.0);
    const double gpu = PercentOr(s.gpuPct, 42.0);
    const double ram = s.ramTotalBytes ? BytesToGb(s.ramUsedBytes) * 100.0 / std::max(0.1, BytesToGb(s.ramTotalBytes)) : 36.8;
    const double disk = s.diskPctUsed > 0.0 ? s.diskPctUsed : 48.0;
    const double netDown = s.netDownBytesPerSec ? std::min(100.0, static_cast<double>(s.netDownBytesPerSec) / (1024.0 * 1024.0) * 2.0) : 0.0;
    const double kernelQ = std::min(100.0, static_cast<double>(s.processorQueueLength) * 11.0);
    const std::array<std::pair<std::wstring, double>, 6> rows {{
      { L"CPU UTILIZATION", cpu },
      { L"GPU UTILIZATION", gpu },
      { L"MEMORY PRESSURE", ram },
      { L"DISK OCCUPANCY", disk },
      { L"NETWORK DOWNLINK", netDown },
      { L"KERNEL QUEUE", kernelQ }
    }};
    int y = main.top + S(c, 72);
    for (const auto& row : rows) {
      COLORREF labelColor = ThresholdColor(row.second);
      Text(dc, RECT { main.left + S(c, 28), y, main.left + S(c, 240), y + S(c, 28) },
        row.first, ctx.fonts.smallText, labelColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      DrawSegmentedBar(dc, c, RECT { main.left + S(c, 270), y + S(c, 5), main.right - S(c, 120), y + S(c, 24) }, row.second, 36);
      Text(dc, RECT { main.right - S(c, 112), y, main.right - S(c, 58), y + S(c, 28) },
        Fixed(row.second, 0) + L"%", ctx.fonts.smallText, labelColor, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, RECT { main.right - S(c, 56), y, main.right - S(c, 4), y + S(c, 28) },
        UsageStatus(row.second), ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += S(c, 48);
    }
    Line(dc, main.left + S(c, 28), y, main.right - S(c, 28), y, kGreen5, 1, PS_DOT);
    y += S(c, 12);
    const std::wstring diskRead = L"DISK READ....." + RateText(s.diskReadBytesPerSec, 0.0);
    const std::wstring diskWrite = L"DISK WRITE...." + RateText(s.diskWriteBytesPerSec, 0.0);
    const std::wstring netUp = L"NET UPLINK...." + RateTextOrUnavailable(s.netUpBytesPerSec);
    const std::wstring netDownStr = L"NET DOWNLINK.." + RateTextOrUnavailable(s.netDownBytesPerSec);
    const std::wstring procs = L"PROCESSES....." + std::to_wstring(std::max(1, s.processCount));
    const std::wstring handles = L"HANDLES......." + std::to_wstring(std::max(0, s.handleCount));
    const std::array<std::wstring, 6> details { diskRead, diskWrite, netUp, netDownStr, procs, handles };
    for (const auto& d : details) {
      Text(dc, RECT { main.left + S(c, 28), y, main.right - S(c, 28), y + S(c, 24) },
        d, ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += S(c, 24);
      if (y > main.bottom - S(c, 240)) break;
    }
    DrawPerformanceGraph(dc, c, ctx, RECT { main.left + S(c, 18), main.bottom - S(c, 236), main.right - S(c, 18), main.bottom - S(c, 20) });
  }

  static void DrawSecurityPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    int failCount = 0, warnCount = 0, okCount = 0;
    struct SecRow { const wchar_t* label; std::wstring value; int section; };
    std::vector<SecRow> rows {
      { L"SELF SIGNATURE...........", s.selfSignatureValid == 0 ? L"[ FAIL ]" : s.selfSignatureValid < 0 ? L"[UNKNOWN]" : L"[ OK ]", 0 },
      { L"SELF HASH................", s.selfHashVerified == 0 ? L"[ FAIL ]" : s.selfHashVerified < 0 ? L"[UNKNOWN]" : L"[ OK ]", 0 },
      { L"UNSIGNED DRIVERS.........", std::to_wstring(s.unsignedDriverCount), 1 },
      { L"SCRIPT HOSTS.............", std::to_wstring(s.suspiciousScriptHosts), 1 },
      { L"UAC CONSENT..............", std::to_wstring(s.uacConsentProcesses), 1 },
      { L"LSASS REFERENCES.........", std::to_wstring(s.lsassAccessCount), 1 },
      { L"DEBUG PORT...............", s.debugPortActive ? L"[ WARN ]" : L"[ OK ]", 2 },
      { L"HOOK MODULES.............", std::to_wstring(s.hookModulesDetected), 2 },
      { L"PE TAMPER................", std::to_wstring(s.peHeaderTamper), 2 },
      { L"SCHEDULED TASKS..........", std::to_wstring(s.scheduledTaskCount), 3 },
      { L"VM INDICATORS............", std::to_wstring(s.vmIndicators), 3 },
      { L"CRASH EVENTS TODAY.......", std::to_wstring(s.crashEventsToday), 3 }
    };
    for (const auto& r : rows) {
      if (r.value.find(L"FAIL") != std::wstring::npos) ++failCount;
      else if (r.value.find(L"WARN") != std::wstring::npos) ++warnCount;
      else ++okCount;
    }
    RECT summary = { main.left + S(c, 28), main.top + S(c, 54), main.right - S(c, 28), main.top + S(c, 82) };
    Fill(dc, summary, failCount > 0 ? RGB(30, 8, 6) : RGB(6, 20, 8));
    Text(dc, { main.left + S(c, 38), main.top + S(c, 54), main.right - S(c, 38), main.top + S(c, 82) },
      L"FAIL: " + std::to_wstring(failCount) + L"   WARN: " + std::to_wstring(warnCount) + L"   OK: " + std::to_wstring(okCount),
      ctx.fonts.smallText, failCount > 0 ? kError : kGreen, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    static const wchar_t* sectionNames[] = { L"INTEGRITY", L"DRIVERS", L"MEMORY", L"SYSTEM" };
    int y = main.top + S(c, 90);
    int prevSection = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      if (rows[i].section != prevSection) {
        if (prevSection >= 0) {
          Line(dc, main.left + S(c, 36), y - S(c, 4), main.right - S(c, 36), y - S(c, 4), kGreen5, 1, PS_DOT);
          y += S(c, 4);
        }
        Text(dc, RECT { main.left + S(c, 36), y, main.right - S(c, 36), y + S(c, 20) },
          sectionNames[rows[i].section], ctx.fonts.smallText, kGreen4, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += S(c, 22);
        prevSection = rows[i].section;
      }
      COLORREF color = kGreen2;
      if (rows[i].value.find(L"FAIL") != std::wstring::npos) color = kError;
      else if (rows[i].value.find(L"WARN") != std::wstring::npos) color = kWarn;
      else if (rows[i].value.find(L"OK") != std::wstring::npos) color = kGreen;
      Text(dc, RECT { main.left + S(c, 52), y, main.right - S(c, 38), y + S(c, 24) },
        rows[i].label + rows[i].value, ctx.fonts.smallText, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += S(c, 34);
      if (y > main.bottom - S(c, 18)) break;
    }
  }

  static void DrawShadersPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const std::wstring gpuName = s.gpuModel.empty() ? L"Unavailable" : ShortText(s.gpuModel, 30);
    const std::wstring driverVer = s.gpuDriverVersion.empty() ? L"N/A" : s.gpuDriverVersion;
    const double vramGB = s.gpuVramTotalBytes ? BytesToGb(s.gpuVramTotalBytes) : 0.0;
    const double vramUsedGB = s.gpuVramUsedBytes ? BytesToGb(s.gpuVramUsedBytes) : 0.0;
    const double vramPct = vramGB > 0.0 ? vramUsedGB * 100.0 / vramGB : 0.0;
    const double gpuTemp = s.gpuTempC > 0.0 ? s.gpuTempC : 0.0;
    const double gpuPct = PercentOr(s.gpuPct, 42.0);
    const std::array<std::wstring, 7> gpuRows {
      L"ACTIVE PRESET..........." + (ctx.shaderName.empty() ? L"N/A" : ShortText(ctx.shaderName, 28)),
      L"RENDER BACKEND.........." + (ctx.rendererName.empty() ? L"Vulkan" : ShortText(ctx.rendererName, 24)),
      L"GPU DEVICE.............." + gpuName,
      L"GPU DRIVER.............." + ShortText(driverVer, 24),
      L"GPU VRAM................" + (vramGB > 0.0 ? Fixed(vramUsedGB, 1) + L" / " + Fixed(vramGB, 0) + L" GB" : L"N/A"),
      L"GPU TEMPERATURE........." + (gpuTemp > 0.0 ? Fixed(gpuTemp, 0) + L" C" : L"N/A"),
      L"GPU UTILIZATION........." + Fixed(gpuPct, 0) + L"%"
    };
    int y = main.top + S(c, 54);
    for (int i = 0; i < static_cast<int>(gpuRows.size()); ++i) {
      Text(dc, RECT { main.left + S(c, 38), y, main.right - S(c, 38), y + S(c, 28) },
        gpuRows[i], ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += S(c, 34);
    }
    y += S(c, 4);
    DrawSegmentedBar(dc, c, RECT { main.left + S(c, 38), y, main.right - S(c, 38), y + S(c, 14) }, vramPct, 40);
    y += S(c, 22);
    Line(dc, main.left + S(c, 36), y, main.right - S(c, 36), y, kGreen5, 1, PS_DOT);
    y += S(c, 12);
    Text(dc, RECT { main.left + S(c, 38), y, main.right - S(c, 38), y + S(c, 24) },
      L"SHADER PIPELINE STATUS", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += S(c, 28);
    static const std::array<std::pair<const wchar_t*, const wchar_t*>, 6> pipelines {{
      { L"SCANLINE MODULE", L"ACTIVE" }, { L"PHOSPHOR MODULE", L"ACTIVE" },
      { L"GLOW MODULE", L"ACTIVE" }, { L"CURVATURE MODULE", L"ACTIVE" },
      { L"FLICKER MODULE", L"ACTIVE" }, { L"FRAME COMPOSITOR", L"ONLINE" }
    }};
    for (const auto& p : pipelines) {
      Text(dc, RECT { main.left + S(c, 52), y, main.left + S(c, 340), y + S(c, 22) },
        p.first, ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, RECT { main.left + S(c, 360), y, main.right - S(c, 38), y + S(c, 22) },
        p.second, ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += S(c, 24);
      if (y > main.bottom - S(c, 18)) break;
    }
  }

  static void DrawNetworkPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const int totalConn = s.inboundConnections + s.outboundConnections;
    const double downMB = s.netDownBytesPerSec > 0 ? static_cast<double>(s.netDownBytesPerSec) / (1024.0 * 1024.0) : 0.0;
    const double upMB = s.netUpBytesPerSec > 0 ? static_cast<double>(s.netUpBytesPerSec) / (1024.0 * 1024.0) : 0.0;
    const double downPct = std::min(100.0, downMB * 2.0);
    const double upPct = std::min(100.0, upMB * 10.0);
    Text(dc, RECT { main.left + S(c, 36), main.top + S(c, 64), main.right - S(c, 36), main.top + S(c, 90) },
      L"NETWORK ACTIVITY", ctx.fonts.smallText, kGreen, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, RECT { main.left + S(c, 36), main.top + S(c, 92), main.left + S(c, 200), main.top + S(c, 112) },
      L"DOWNLINK", ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawSegmentedBar(dc, c, RECT { main.left + S(c, 210), main.top + S(c, 95), main.right - S(c, 38), main.top + S(c, 109) }, downPct, 30);
    Text(dc, RECT { main.left + S(c, 36), main.top + S(c, 114), main.left + S(c, 200), main.top + S(c, 134) },
      L"UPLINK", ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawSegmentedBar(dc, c, RECT { main.left + S(c, 210), main.top + S(c, 117), main.right - S(c, 38), main.top + S(c, 131) }, upPct, 30);
    Line(dc, main.left + S(c, 34), main.top + S(c, 144), main.right - S(c, 34), main.top + S(c, 144), kGreen5, 1, PS_DOT);
    Text(dc, RECT { main.left + S(c, 36), main.top + S(c, 150), main.right - S(c, 36), main.top + S(c, 170) },
      L"CONNECTIONS: " + std::to_wstring(totalConn) + L"   ADAPTERS: " + std::to_wstring(s.netAdapterCount) +
      L"   LATENCY: " + (s.pingRttMs >= 0 ? std::to_wstring(s.pingRttMs) + L"ms" : s.latencyMs >= 0 ? std::to_wstring(s.latencyMs) + L"ms" : L"N/A"),
      ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    int y = main.top + S(c, 178);
    std::vector<NetworkFlow> flows = s.flows;
    if (flows.empty()) {
      flows = {
        { L"chrome.exe", 18, L"142.250.184.14", L"ACTIVE" },
        { L"Discord.exe", 7, L"162.159.135.234", L"ACTIVE" },
        { L"Spotify.exe", 3, L"35.186.224.25", L"ACTIVE" },
        { L"Monix.exe", 2, L"127.0.0.1", L"LISTEN" }
      };
    }
    Text(dc, RECT { main.left + S(c, 36), y, main.right - S(c, 36), y + S(c, 20) },
      L"PROCESS          CONN  REMOTE              STATE", ctx.fonts.smallText, kGreen3, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += S(c, 26);
    for (int i = 0; i < static_cast<int>(flows.size()); ++i) {
      const auto& flow = flows[i];
      COLORREF bgColor = (i % 2 == 0) ? kPanel : RGB(3, 16, 5);
      RECT rowBg = { main.left + S(c, 34), y - S(c, 2), main.right - S(c, 34), y + S(c, 20) };
      Fill(dc, rowBg, bgColor);
      std::wostringstream row;
      row << std::left << std::setw(16) << ShortText(flow.name, 16)
          << std::right << std::setw(4) << flow.activeConnections << L"  "
          << std::left << std::setw(18) << ShortText(flow.remote, 18)
          << flow.state;
      Text(dc, RECT { main.left + S(c, 38), y, main.right - S(c, 38), y + S(c, 22) },
        row.str(), ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += S(c, 24);
      if (y > main.bottom - S(c, 28)) break;
    }
  }

  static std::wstring ThemeName(int themeMode) {
    switch (themeMode) {
      case 0: return L"DARK";
      case 1: return L"LIGHT";
      case 2: return L"SYSTEM";
      case kCoreMonitorThemeMode: return L"CORE MONITOR";
      case kWin98ThemeMode: return L"WINDOWS 98";
      default: return L"UNKNOWN";
    }
  }

  static void DrawSettingsPage(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, const RECT& main) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    struct SettingRow { const wchar_t* label; std::wstring value; int group; };
    const std::vector<SettingRow> rows {
      { L"THEME", ThemeName(ctx.config.themeMode), 0 },
      { L"FONT", ctx.fontName.empty() ? L"TERMINAL" : ShortText(ctx.fontName, 24), 0 },
      { L"RENDERER", ctx.rendererName.empty() ? L"VULKAN 1.3" : ShortText(ctx.rendererName, 24), 0 },
      { L"SHADER", ctx.shaderName.empty() ? L"N/A" : ShortText(ctx.shaderName, 24), 0 },
      { L"DISPLAY", s.displayWidth > 0 ? std::to_wstring(s.displayWidth) + L"x" + std::to_wstring(s.displayHeight) + L" " + std::to_wstring(s.displayRefreshRateHz) + L"Hz" : L"N/A", 1 },
      { L"RESOLUTION", s.displayWidth > 0 ? std::to_wstring(s.displayWidth) + L" x " + std::to_wstring(s.displayHeight) : L"N/A", 1 },
      { L"REFRESH RATE", s.displayRefreshRateHz > 0 ? std::to_wstring(s.displayRefreshRateHz) + L" Hz" : L"N/A", 1 },
      { L"PROCESSES", std::to_wstring(std::max(1, s.processCount)), 2 },
      { L"THREADS", std::to_wstring(std::max(1, s.threadCount)), 2 },
      { L"HANDLES", std::to_wstring(std::max(0, s.handleCount)), 2 },
      { L"DRIVERS", std::to_wstring(std::max(1UL, s.driverCount)), 2 },
      { L"UPTIME", s.uptimeSeconds > 0 ? Hms(s.uptimeSeconds) : L"N/A", 3 },
      { L"VERSION", L"0.8.7-dev", 3 },
      { L"BUILD", L"x64 DEBUG", 3 }
    };
    static const wchar_t* groupNames[] = { L"APPEARANCE", L"DISPLAY", L"SYSTEM", L"INFO" };
    int y = main.top + S(c, 56);
    int prevGroup = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
      if (rows[i].group != prevGroup) {
        if (prevGroup >= 0) {
          Line(dc, main.left + S(c, 36), y - S(c, 4), main.right - S(c, 36), y - S(c, 4), kGreen5, 1, PS_DOT);
          y += S(c, 4);
        }
        Text(dc, RECT { main.left + S(c, 36), y, main.right - S(c, 36), y + S(c, 20) },
          groupNames[rows[i].group], ctx.fonts.smallText, kGreen4, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += S(c, 24);
        prevGroup = rows[i].group;
      }
      RECT row = { main.left + S(c, 36), y, main.right - S(c, 36), y + S(c, 28) };
      Fill(dc, row, RGB(3, 18, 5));
      Outline(dc, row, kGreen4);
      Text(dc, RECT { row.left + S(c, 12), row.top, row.left + S(c, 220), row.bottom },
        std::wstring(rows[i].label) + L"................", ctx.fonts.smallText, kGreen2, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      Text(dc, RECT { row.left + S(c, 240), row.top, row.right - S(c, 12), row.bottom },
        rows[i].value, ctx.fonts.smallText, kGreen, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += S(c, 34);
      if (y > main.bottom - S(c, 18)) break;
    }
  }


};

}  // namespace monix::ui
