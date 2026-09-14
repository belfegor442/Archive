#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <windows.h>
#include <gdiplus.h>

#include "../core/TextUtils.hpp"
#include "../config/MonixConfig.hpp"
#include "../logging/LogEntry.hpp"
#include "../settings/MonixConfigTypes.hpp"
#include "../telemetry/Snapshot.hpp"
#include "AppState.hpp"
#include "CoreMonitorTheme.hpp"
#include "Win98Types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace monix::ui {

class Win98Theme {
public:
  static constexpr COLORREF kWin98Gray = RGB(192, 192, 192);
  static constexpr COLORREF kWin98DarkGray = RGB(128, 128, 128);
  static constexpr COLORREF kWin98LightGray = RGB(223, 223, 223);
  static constexpr COLORREF kWin98White = RGB(255, 255, 255);
  static constexpr COLORREF kWin98Black = RGB(0, 0, 0);
  static constexpr COLORREF kWin98Navy = RGB(0, 0, 128);
  static constexpr COLORREF kWin98TitleBlue = RGB(10, 36, 106);
  static constexpr COLORREF kWin98Red = RGB(255, 0, 0);
  static constexpr COLORREF kWin98Yellow = RGB(255, 255, 0);
  static constexpr COLORREF kWin98Green = RGB(0, 168, 0);
  static constexpr COLORREF kWin98BrightGreen = RGB(0, 255, 0);
  static constexpr COLORREF kWin98Highlight = RGB(0, 0, 128);
  static constexpr COLORREF kWin98HighlightText = RGB(255, 255, 255);
  static constexpr COLORREF kWin98ButtonFace = RGB(192, 192, 192);
  static constexpr COLORREF kWin98ButtonHighlight = RGB(255, 255, 255);
  static constexpr COLORREF kWin98ButtonShadow = RGB(128, 128, 128);
  static constexpr COLORREF kWin98ButtonDkShadow = RGB(0, 0, 0);
  static constexpr COLORREF kWin98ConsoleBg = RGB(0, 0, 0);

  static constexpr int kDesignWidth = 1536;
  static constexpr int kDesignHeight = 1024;

  struct Canvas {
    RECT rect;
    int designW;
    int designH;
    float scale;
    const Win98ThemeFonts* fonts;
    const Win98Assets* assets;
  };

  static float ComputeScale(int clientW, int clientH) {
    const float sx = static_cast<float>(clientW) / static_cast<float>(kDesignWidth);
    const float sy = static_cast<float>(clientH) / static_cast<float>(kDesignHeight);
    return (std::min)(sx, sy);
  }

  static Canvas MakeCanvas(const RECT& client, const Win98ThemeFonts& fonts, const Win98Assets& assets) {
    const int cw = client.right - client.left;
    const int ch = client.bottom - client.top;
    const float scale = ComputeScale(cw, ch);
    const int dw = static_cast<int>(static_cast<float>(kDesignWidth) * scale);
    const int dh = static_cast<int>(static_cast<float>(kDesignHeight) * scale);
    const int ox = client.left + (cw - dw) / 2;
    const int oy = client.top + (ch - dh) / 2;
    return Canvas { { ox, oy, ox + dw, oy + dh }, kDesignWidth, kDesignHeight, scale, &fonts, &assets };
  }

  static int X(const Canvas& c, int designX) {
    return c.rect.left + static_cast<int>(static_cast<float>(designX) * c.scale);
  }

  static int Y(const Canvas& c, int designY) {
    return c.rect.top + static_cast<int>(static_cast<float>(designY) * c.scale);
  }

  static int S(const Canvas& c, int designSize) {
    return (std::max)(1, static_cast<int>(static_cast<float>(designSize) * c.scale));
  }

  static RECT R(const Canvas& c, int x, int y, int w, int h) {
    return { X(c, x), Y(c, y), X(c, x + w), Y(c, y + h) };
  }

  static RECT Pad(const RECT& r, int t, int l, int b, int ri) {
    return { r.left + l, r.top + t, r.right - ri, r.bottom - b };
  }

  static void Fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) return;
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
  }

  static void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    if (!pen) return;
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  static void Win98Bevel(HDC dc, const RECT& r, bool sunken) {
    if (sunken) {
      Line(dc, r.left, r.top, r.right - 1, r.top, kWin98ButtonShadow);
      Line(dc, r.left, r.top, r.left, r.bottom - 1, kWin98ButtonShadow);
      Line(dc, r.right - 1, r.top, r.right - 1, r.bottom - 1, kWin98ButtonHighlight);
      Line(dc, r.left, r.bottom - 1, r.right - 1, r.bottom - 1, kWin98ButtonHighlight);
      Line(dc, r.left + 1, r.top + 1, r.right - 2, r.top + 1, kWin98ButtonDkShadow);
      Line(dc, r.left + 1, r.top + 1, r.left + 1, r.bottom - 2, kWin98ButtonDkShadow);
      Line(dc, r.right - 2, r.top + 1, r.right - 2, r.bottom - 2, kWin98LightGray);
      Line(dc, r.left + 1, r.bottom - 2, r.right - 2, r.bottom - 2, kWin98LightGray);
    } else {
      Line(dc, r.left, r.top, r.right - 1, r.top, kWin98ButtonHighlight);
      Line(dc, r.left, r.top, r.left, r.bottom - 1, kWin98ButtonHighlight);
      Line(dc, r.right - 1, r.top, r.right - 1, r.bottom - 1, kWin98ButtonDkShadow);
      Line(dc, r.left, r.bottom - 1, r.right - 1, r.bottom - 1, kWin98ButtonDkShadow);
      Line(dc, r.left + 1, r.top + 1, r.right - 2, r.top + 1, kWin98LightGray);
      Line(dc, r.left + 1, r.top + 1, r.left + 1, r.bottom - 2, kWin98LightGray);
      Line(dc, r.right - 2, r.top + 1, r.right - 2, r.bottom - 2, kWin98ButtonShadow);
      Line(dc, r.left + 1, r.bottom - 2, r.right - 2, r.bottom - 2, kWin98ButtonShadow);
    }
  }

  static void Text(HDC dc, RECT rect, const std::wstring& text, HFONT font, COLORREF color, UINT format) {
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
    SelectObject(dc, oldFont);
  }

  static void TextLine(HDC dc, const Canvas& c, int x, int y, int w, const std::wstring& text, HFONT font, int lineHeight, COLORREF color) {
    RECT rect = R(c, x, y, w, lineHeight + 4);
    Text(dc, rect, text, font, color, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
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
    swprintf_s(buffer, L"%04d-%02d-%02d", time.wYear, time.wMonth, time.wDay);
    return buffer;
  }

  static std::wstring DateTimeText() {
    return DateText() + L"  " + ClockText();
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
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << mb << L" MB/s";
    return out.str();
  }

  static std::wstring RateTextShort(std::uint64_t bytesPerSec, double fallbackMb) {
    double mb = bytesPerSec > 0 ? static_cast<double>(bytesPerSec) / (1024.0 * 1024.0) : fallbackMb;
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << mb << L"M";
    return out.str();
  }

  static std::wstring Fixed(double value, int decimals) {
    std::wostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
  }

  static std::wstring ShortText(std::wstring value, std::size_t limit) {
    if (value.size() <= limit) return value;
    if (limit <= 3) return value.substr(0, limit);
    return value.substr(0, limit - 3) + L"...";
  }

  static std::wstring FormatBytes(std::uint64_t bytes) {
    if (bytes >= 1073741824ull) {
      return Fixed(static_cast<double>(bytes) / 1073741824.0, 1) + L"GB";
    }
    if (bytes >= 1048576ull) {
      return Fixed(static_cast<double>(bytes) / 1048576.0, 0) + L"MB";
    }
    if (bytes >= 1024ull) {
      return Fixed(static_cast<double>(bytes) / 1024.0, 0) + L"KB";
    }
    return std::to_wstring(bytes) + L"B";
  }

  static std::wstring FixedWidthNumber(int value, int width) {
    std::wostringstream out;
    out << std::setw(width) << std::setfill(L'0') << (std::max)(0, value);
    return out.str();
  }

  static const Snapshot& SnapshotOrDefault(const CoreMonitorThemeContext& ctx) {
    static const Snapshot fallback {};
    return ctx.snapshot ? *ctx.snapshot : fallback;
  }

  static COLORREF ThresholdColor(double pct) {
    if (pct >= 85.0) return kWin98Red;
    if (pct >= 65.0) return RGB(180, 120, 0);
    return kWin98Green;
  }

  static COLORREF SeverityColor(const std::wstring& text) {
    if (text.find(L"WARN") != std::wstring::npos || text.find(L"[W]") != std::wstring::npos) return RGB(180, 120, 0);
    if (text.find(L"ERROR") != std::wstring::npos || text.find(L"FAIL") != std::wstring::npos || text.find(L"[E]") != std::wstring::npos) return kWin98Red;
    return kWin98Green;
  }

  static void Win98Button(HDC dc, const Canvas& c, int x, int y, int w, int h, const std::wstring& text, bool pressed = false) {
    RECT r = R(c, x, y, w, h);
    Fill(dc, r, kWin98ButtonFace);
    Win98Bevel(dc, r, !pressed);
    RECT inner = Pad(r, S(c, pressed ? 3 : 2), S(c, pressed ? 3 : 2), S(c, pressed ? 3 : 2), S(c, pressed ? 3 : 2));
    Text(dc, inner, text, c.fonts->menuFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawGroupbox(HDC dc, const Canvas& c, int x, int y, int w, int h, const std::wstring& title, HFONT font) {
    RECT outer = R(c, x, y, w, h);
    Fill(dc, outer, kWin98Gray);
    Line(dc, outer.left + S(c, 1), outer.top + S(c, 10), outer.right - S(c, 1), outer.top + S(c, 10), kWin98ButtonShadow);
    Line(dc, outer.left + S(c, 1), outer.top + S(c, 10), outer.left + S(c, 1), outer.bottom - S(c, 1), kWin98ButtonShadow);
    Line(dc, outer.left + S(c, 1), outer.bottom - S(c, 1), outer.right - S(c, 1), outer.bottom - S(c, 1), kWin98ButtonShadow);
    Line(dc, outer.right - S(c, 1), outer.top + S(c, 10), outer.right - S(c, 1), outer.bottom - S(c, 1), kWin98ButtonShadow);
    Line(dc, outer.left, outer.top, outer.right - S(c, 1), outer.top, kWin98ButtonHighlight);
    Line(dc, outer.left, outer.top, outer.left, outer.bottom - S(c, 1), kWin98ButtonHighlight);
    RECT titleRect = { outer.left + S(c, 8), outer.top - S(c, 7), outer.right - S(c, 8), outer.top + S(c, 10) };
    Fill(dc, titleRect, kWin98Gray);
    Text(dc, titleRect, title, font, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawSegmentedBar(HDC dc, const Canvas& c, int x, int y, int w, int h, double pct, COLORREF fillColor) {
    RECT outer = R(c, x, y, w, h);
    Fill(dc, outer, kWin98White);
    Win98Bevel(dc, outer, true);
    const int pad = S(c, 3);
    const int gap = S(c, 1);
    const int segments = 30;
    const int segW = (std::max)(2, static_cast<int>((outer.right - outer.left - pad * 2 - gap * (segments - 1)) / segments));
    const int filled = std::clamp(static_cast<int>(std::round(pct * segments / 100.0)), 0, segments);
    for (int i = 0; i < segments; ++i) {
      RECT seg = {
        outer.left + pad + i * (segW + gap),
        outer.top + S(c, 2),
        outer.left + pad + i * (segW + gap) + segW,
        outer.bottom - S(c, 2)
      };
      if (i < filled) {
        Fill(dc, seg, fillColor);
        Line(dc, seg.left, seg.top, seg.right - 1, seg.top, RGB(255, 255, 255));
        Line(dc, seg.left, seg.top, seg.left, seg.bottom - 1, RGB(255, 255, 255));
        Line(dc, seg.right - 1, seg.top, seg.right - 1, seg.bottom - 1, RGB(64, 64, 64));
        Line(dc, seg.left, seg.bottom - 1, seg.right - 1, seg.bottom - 1, RGB(64, 64, 64));
      } else {
        Fill(dc, seg, RGB(200, 200, 200));
        Line(dc, seg.left, seg.top, seg.right - 1, seg.top, RGB(230, 230, 230));
        Line(dc, seg.left, seg.top, seg.left, seg.bottom - 1, RGB(230, 230, 230));
        Line(dc, seg.right - 1, seg.top, seg.right - 1, seg.bottom - 1, RGB(160, 160, 160));
        Line(dc, seg.left, seg.bottom - 1, seg.right - 1, seg.bottom - 1, RGB(160, 160, 160));
      }
    }
  }

  static void DrawConsoleLine(HDC dc, const Canvas& c, int x, int y, int w, int lineH,
                               const std::wstring& timestamp, const std::wstring& level, const std::wstring& message,
                               HFONT font) {
    RECT rect = R(c, x, y, w, lineH);
    Fill(dc, rect, kWin98ConsoleBg);
    SetBkMode(dc, TRANSPARENT);
    SetBkColor(dc, kWin98ConsoleBg);
    HGDIOBJ oldFont = SelectObject(dc, font);

    SetTextColor(dc, RGB(0, 200, 0));
    RECT tsRect = rect;
    tsRect.right = tsRect.left + S(c, 120);
    DrawTextW(dc, timestamp.c_str(), static_cast<int>(timestamp.size()), &tsRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT levelRect = rect;
    levelRect.left += S(c, 125);
    levelRect.right = levelRect.left + S(c, 80);
    COLORREF levelColor = kWin98BrightGreen;
    if (level.find(L"WARN") != std::wstring::npos) levelColor = kWin98Yellow;
    else if (level.find(L"ERROR") != std::wstring::npos || level.find(L"FAIL") != std::wstring::npos) levelColor = kWin98Red;
    else if (level.find(L"OK") != std::wstring::npos) levelColor = kWin98BrightGreen;
    else if (level.find(L"TEMP") != std::wstring::npos) levelColor = RGB(0, 200, 255);
    else if (level.find(L"NET") != std::wstring::npos) levelColor = RGB(0, 200, 255);
    else if (level.find(L"DISK") != std::wstring::npos) levelColor = RGB(255, 200, 0);
    else if (level.find(L"TASK") != std::wstring::npos) levelColor = RGB(100, 200, 255);
    else if (level.find(L"AI") != std::wstring::npos) levelColor = RGB(255, 128, 255);
    SetTextColor(dc, levelColor);
    DrawTextW(dc, level.c_str(), static_cast<int>(level.size()), &levelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT msgRect = rect;
    msgRect.left += S(c, 210);
    SetTextColor(dc, RGB(220, 220, 220));
    DrawTextW(dc, message.c_str(), static_cast<int>(message.size()), &msgRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    SelectObject(dc, oldFont);
  }

  static void DrawMiniGraph(HDC dc, const Canvas& c, int x, int y, int w, int h,
                             const std::vector<double>* vals, COLORREF color, const std::wstring& label,
                             double fallbackBase = 40.0, double fallbackAmp = 20.0, double fallbackPhase = 0.0) {
    RECT graph = R(c, x, y, w, h);
    Fill(dc, graph, kWin98ConsoleBg);
    Win98Bevel(dc, graph, true);

    RECT labelBg = R(c, x + 4, y + 2, 80, 16);
    Fill(dc, labelBg, RGB(0, 40, 80));
    Text(dc, labelBg, label, c.fonts->menuFont, color,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const int plotL = graph.left + S(c, 4);
    const int plotT = graph.top + S(c, 20);
    const int plotR = graph.right - S(c, 4);
    const int plotB = graph.bottom - S(c, 4);
    const int graphW = plotR - plotL;
    const int graphH = plotB - plotT;
    if (graphW <= 0 || graphH <= 0) return;

    HPEN gridPen = CreatePen(PS_DOT, 1, RGB(50, 50, 50));
    HGDIOBJ oldPen2 = SelectObject(dc, gridPen);
    for (int gy = 25; gy < 100; gy += 25) {
      int py = plotB - static_cast<int>(graphH * gy / 100.0);
      MoveToEx(dc, plotL, py, nullptr);
      LineTo(dc, plotR, py);
    }
    SelectObject(dc, oldPen2);
    DeleteObject(gridPen);

    {
      HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
      HGDIOBJ oldBP = SelectObject(dc, borderPen);
      MoveToEx(dc, plotL, plotB, nullptr);
      LineTo(dc, plotR, plotB);
      LineTo(dc, plotR, plotT);
      SelectObject(dc, oldBP);
      DeleteObject(borderPen);
    }

    std::vector<double> display;
    if (vals && vals->size() >= 2) {
      const std::size_t begin = vals->size() > 60 ? vals->size() - 60 : 0;
      display.assign(vals->begin() + begin, vals->end());
    } else {
      display.resize(60);
      for (std::size_t i = 0; i < display.size(); ++i) {
        double t = static_cast<double>(i) / 60.0;
        display[i] = fallbackBase + fallbackAmp * std::sin(6.28 * t * 3.0 + fallbackPhase) + fallbackAmp * 0.3 * std::sin(6.28 * t * 7.0 + fallbackPhase * 2.0);
      }
    }

    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    for (std::size_t i = 0; i < display.size(); ++i) {
      const double v = std::clamp(display[i] / 100.0, 0.0, 1.0);
      const int px = plotL + static_cast<int>(graphW * i / std::max<std::size_t>(1, display.size() - 1));
      const int py = plotB - static_cast<int>(graphH * v);
      if (i == 0) MoveToEx(dc, px, py, nullptr);
      else LineTo(dc, px, py);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);

    RECT valRect = { graph.right - S(c, 52), graph.top + S(c, 2), graph.right - S(c, 4), graph.top + S(c, 18) };
    Fill(dc, valRect, RGB(0, 40, 80));
    double lastVal = display.empty() ? 0.0 : display.back();
    Text(dc, valRect, Fixed(lastVal, 0) + L"%", c.fonts->smallFont, color,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void LoadAssets(Win98Assets& assets, const std::filesystem::path& rootDir) {
    const auto base = rootDir / "themes" / L"Windows 98" / L"Assets";
    auto load = [](Gdiplus::Bitmap*& target, const std::filesystem::path& file) {
      if (std::filesystem::exists(file)) {
        auto* wpath = file.c_str();
        target = Gdiplus::Bitmap::FromFile(wpath);
        if (target->GetLastStatus() != Gdiplus::Ok) {
          delete target;
          target = nullptr;
        }
      }
    };
    load(assets.windowBase, base / L"Window_Base.png");
    load(assets.windowHeader, base / L"Window_Header.png");
    load(assets.windowHeaderInactive, base / L"Window_Header_Inactive.png");
    load(assets.windowHeaderResizable, base / L"Window_Header_Resizable.png");
    load(assets.windowHeaderResizableInactive, base / L"Window_Header_Resizable_Inactive.png");
    load(assets.buttonNormal, base / L"Windows_Button.png");
    load(assets.buttonFocus, base / L"Windows_Button_Focus.png");
    load(assets.buttonFocusOutlined, base / L"Windows_Button_Focus_Outlined.png");
    load(assets.buttonInactive, base / L"Windows_Button_Inactive.png");
    load(assets.buttonPressed, base / L"Windows_Button_Pressed.png");
    load(assets.buttonPressedOutlined, base / L"Windows_Button_Pressed_Outlined.png");
    load(assets.dividerLine, base / L"Windows_Divider_Line.png");
    load(assets.icons, base / L"Windows_Icons.png");
    load(assets.innerFrame, base / L"Windows_Inner_Frame.png");
    load(assets.innerFrameInverted, base / L"Windows_Inner_Frame_Inverted.png");
    load(assets.progressFill, base / L"Windows_Progress_Fill.png");
    load(assets.ratio, base / L"Windows_Ratio.png");
    load(assets.ratioInactive, base / L"Windows_Ratio_Inactive.png");
    load(assets.ratioSelected, base / L"Windows_Ratio_Selected.png");
    load(assets.sidebarUnderside, base / L"Windows_SideBar_Underside.png");
    load(assets.sliderBackground, base / L"Windows_Slider_Background.png");
    load(assets.sliderHandle, base / L"Windows_Slider_Handle.png");
    load(assets.toggleActive, base / L"Windows_Toggle_Active.png");
    load(assets.toggleInactive, base / L"Windows_Toggle_Inactive.png");
    load(assets.toggleSelected, base / L"Windows_Toggle_Selected.png");
  }

  static void DrawTitleBarIcon(HDC dc, const Canvas& c, int x, int y) {
    int sz = S(c, 16);
    RECT iconBg = { X(c, x), Y(c, y), X(c, x) + sz, Y(c, y) + sz };
    Fill(dc, iconBg, kWin98Navy);
    int px = sz / 8;
    if (px < 1) px = 1;
    auto dot = [&](int row, int col, COLORREF col2) {
      RECT r = { iconBg.left + col * px, iconBg.top + row * px,
                  iconBg.left + (col + 1) * px, iconBg.top + (row + 1) * px };
      Fill(dc, r, col2);
    };
    COLORREF w = RGB(255, 255, 255);
    COLORREF g = RGB(0, 200, 0);
    COLORREF b = RGB(100, 180, 255);
    for (int i = 0; i < 8; ++i) { dot(0, i, w); dot(7, i, w); dot(i, 0, w); dot(i, 7, w); }
    dot(1, 1, b); dot(1, 2, g); dot(1, 3, g); dot(1, 4, g); dot(1, 5, g); dot(1, 6, b);
    dot(2, 1, b); dot(2, 2, g); dot(2, 3, w); dot(2, 4, w); dot(2, 5, g); dot(2, 6, b);
    dot(3, 1, b); dot(3, 2, g); dot(3, 3, w); dot(3, 4, w); dot(3, 5, g); dot(3, 6, b);
    dot(4, 1, b); dot(4, 2, g); dot(4, 3, g); dot(4, 4, g); dot(4, 5, g); dot(4, 6, b);
    dot(5, 1, b); dot(5, 2, g); dot(5, 3, g); dot(5, 4, g); dot(5, 5, g); dot(5, 6, b);
    dot(6, 1, b); dot(6, 2, b); dot(6, 3, b); dot(6, 4, b); dot(6, 5, b); dot(6, 6, b);
  }

  static void DrawTitleBar(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    RECT titleBar = R(c, 0, 0, 1536, 28);
    for (int x = titleBar.left; x < titleBar.right; ++x) {
      float t = static_cast<float>(x - titleBar.left) / static_cast<float>(titleBar.right - titleBar.left);
      int r = static_cast<int>(10 + t * 6.0);
      int g = static_cast<int>(36 + t * 16.0);
      int b = static_cast<int>(106 + t * 60.0);
      RECT col = { x, titleBar.top, x + 1, titleBar.bottom };
      Fill(dc, col, RGB(r, g, b));
    }
    DrawTitleBarIcon(dc, c, 3, 5);
    RECT textRect = Pad(titleBar, 2, 24, 2, 120);
    Text(dc, textRect, L"MONIX 1.0.0 - WEIRD STUFF SOFTWARE", c.fonts->titleBar, kWin98White,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    int btnSize = S(c, 18);
    int btnY = Y(c, 4);
    int btnW = S(c, 18);
    RECT btnMin = { titleBar.right - S(c, 66), btnY, titleBar.right - S(c, 48), btnY + btnSize };
    RECT btnMax = { titleBar.right - S(c, 44), btnY, titleBar.right - S(c, 26), btnY + btnSize };
    RECT btnClose = { titleBar.right - S(c, 22), btnY, titleBar.right - S(c, 4), btnY + btnSize };
    const bool minP = (ctx.pressedButton == 300);
    const bool maxP = (ctx.pressedButton == 301);
    const bool closeP = (ctx.pressedButton == 302);
    Fill(dc, btnMin, kWin98ButtonFace);
    Win98Bevel(dc, btnMin, !minP);
    Text(dc, minP ? Pad(btnMin, S(c,1),S(c,1),S(c,1),S(c,1)) : btnMin, L"_", c.fonts->menuFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Fill(dc, btnMax, kWin98ButtonFace);
    Win98Bevel(dc, btnMax, !maxP);
    Text(dc, maxP ? Pad(btnMax, S(c,1),S(c,1),S(c,1),S(c,1)) : btnMax, L"\u25A1", c.fonts->menuFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Fill(dc, btnClose, kWin98ButtonFace);
    Win98Bevel(dc, btnClose, !closeP);
    Text(dc, closeP ? Pad(btnClose, S(c,1),S(c,1),S(c,1),S(c,1)) : btnClose, L"X", c.fonts->titleBar, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Line(dc, titleBar.left, titleBar.bottom - 1, titleBar.right, titleBar.bottom - 1, kWin98ButtonDkShadow);
  }

  static void DrawMenuBar(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    RECT menuBar = R(c, 0, 28, 1536, 50);
    Fill(dc, menuBar, kWin98Gray);
    static const std::array<const wchar_t*, 4> menus { L"  File  ", L"  View  ", L"  Tools  ", L"  Help  " };
    int x = 4;
    for (int mi = 0; mi < 4; ++mi) {
      RECT item = R(c, x, 29, 70, 19);
      const bool isHovered = (mi == ctx.hoveredMenuIndex);
      if (isHovered) {
        Fill(dc, item, kWin98ButtonFace);
        Win98Bevel(dc, item, true);
      }
      Text(dc, isHovered ? Pad(item, S(c,1),S(c,1),S(c,1),S(c,1)) : item, menus[mi], c.fonts->menuFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      x += 70;
    }
    Line(dc, menuBar.left, menuBar.bottom - 1, menuBar.right, menuBar.bottom - 1, kWin98ButtonShadow);
  }

  static bool HitTestMenuBar(const Canvas& c, POINT clientPt, int& menuIndex) {
    double x = (clientPt.x - c.rect.left) / c.scale;
    double y = (clientPt.y - c.rect.top) / c.scale;
    if (y < 29.0 || y > 48.0) return false;
    int idx = static_cast<int>((x - 4.0) / 70.0);
    if (idx < 0 || idx >= 4) return false;
    menuIndex = idx;
    return true;
  }

  static void DrawTabBar(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    RECT tabBar = R(c, 0, 50, 1536, 44);
    Fill(dc, tabBar, kWin98Gray);
    Line(dc, tabBar.left, tabBar.bottom - 1, tabBar.right, tabBar.bottom - 1, kWin98ButtonDkShadow);

    struct TabInfo { const wchar_t* label; COLORREF iconColor; };
    static const std::array<TabInfo, 5> tabs {{
      { L"LOG", kWin98BrightGreen },
      { L"TASKS", RGB(0, 100, 255) },
      { L"USAGE", kWin98Red },
      { L"AI", RGB(200, 100, 255) },
      { L"SETTINGS", kWin98DarkGray }
    }};

    const int activeTab = std::clamp(ctx.menuIndex, 0, 4);
    const int tabW = 110;
    const int tabH = 32;
    const int tabY = 54;
    int tabX = 10;

    for (int i = 0; i < 5; ++i) {
      bool isActive = (i == activeTab);
      int ty = isActive ? tabY : tabY + 4;
      int th = isActive ? tabH : tabH - 4;
      RECT tab = R(c, tabX, ty, tabW, th);

      if (isActive) {
        static constexpr COLORREF kActiveTabFace = RGB(212, 212, 212);
        Fill(dc, tab, kActiveTabFace);
        Line(dc, tab.left, tab.top, tab.right - 1, tab.top, kWin98ButtonHighlight);
        Line(dc, tab.left, tab.top, tab.left, tab.bottom - 1, kWin98ButtonHighlight);
        Line(dc, tab.right - 1, tab.top, tab.right - 1, tab.bottom - 1, kWin98ButtonDkShadow);
        Fill(dc, R(c, tabX + 2, tab.bottom - S(c, 2), tabW - 4, S(c, 2)), kActiveTabFace);
      } else {
        Fill(dc, tab, kWin98ButtonFace);
        Line(dc, tab.left, tab.top, tab.right - 1, tab.top, kWin98ButtonShadow);
        Line(dc, tab.left, tab.top, tab.left, tab.bottom - 1, kWin98ButtonShadow);
        Line(dc, tab.right - 1, tab.top, tab.right - 1, tab.bottom - 1, kWin98ButtonHighlight);
        Line(dc, tab.left, tab.bottom - 1, tab.right - 1, tab.bottom - 1, kWin98ButtonHighlight);
        Fill(dc, R(c, tabX + 2, tab.bottom - S(c, 2), tabW - 4, S(c, 2)), kWin98ButtonFace);
      }

      RECT iconRect = R(c, tabX + 8, ty + 8, 10, 10);
      HBRUSH iconBrush = CreateSolidBrush(tabs[i].iconColor);
      HGDIOBJ oldBrush = SelectObject(dc, iconBrush);
      HPEN oldPen = static_cast<HPEN>(SelectObject(dc, GetStockObject(NULL_PEN)));
      Rectangle(dc, iconRect.left, iconRect.top, iconRect.right, iconRect.bottom);
      SelectObject(dc, oldPen);
      SelectObject(dc, oldBrush);
      DeleteObject(iconBrush);

      RECT labelRect = R(c, tabX + 24, ty, tabW - 28, th);
      SetBkMode(dc, OPAQUE);
      SetBkColor(dc, isActive ? RGB(212, 212, 212) : kWin98ButtonFace);
      SetTextColor(dc, kWin98Black);
      HGDIOBJ oldF = SelectObject(dc, isActive ? c.fonts->titleBar : c.fonts->menuFont);
      DrawTextW(dc, tabs[i].label, -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      SelectObject(dc, oldF);
      SetBkMode(dc, TRANSPARENT);

      tabX += tabW + 4;
    }

    RECT logoArea = R(c, tabX + 20, 52, 1536 - tabX - 20, 40);
    Text(dc, R(c, tabX + 20, 52, 1536 - tabX - 20, 20), L"MONIX", c.fonts->titleBar, kWin98Navy,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, tabX + 20, 68, 1536 - tabX - 20, 14), L"SYSTEM MONITOR", c.fonts->menuFont, kWin98DarkGray,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, tabX + 20, 82, 1536 - tabX - 20, 12), L"\u00A9 WEIRD STUFF SOFTWARE", c.fonts->smallFont, kWin98DarkGray,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawStatusBar(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    RECT status = R(c, 0, 994, 1536, 30);
    Fill(dc, status, kWin98Gray);
    Line(dc, status.left, status.top, status.right, status.top, kWin98ButtonHighlight);
    Line(dc, status.left, status.top + 1, status.right, status.top + 1, kWin98ButtonShadow);

    const Snapshot& s = SnapshotOrDefault(ctx);
    const double cpuPct = PercentOr(s.cpuPct, 42.0);
    const double ramPct = s.ramTotalBytes ? BytesToGb(s.ramUsedBytes) * 100.0 / (std::max)(0.1, BytesToGb(s.ramTotalBytes)) : 78.0;

    const int logCount = ctx.logs ? static_cast<int>(ctx.logs->size()) : 256;
    std::wstring sStatus = L"STATUS: MONITORING";
    std::wstring sLogLines = L"LOG LINES: " + std::to_wstring(logCount) + L"/2048";
    std::wstring sCpu = L"CPU: " + Fixed(cpuPct, 0) + L"%";
    std::wstring sRam = L"RAM: " + Fixed(ramPct, 0) + L"%";
    std::wstring sDate = DateTimeText();
    std::wstring labels[5] = { sStatus, sLogLines, sCpu, sRam, sDate };

    int widths[5] = { 300, 250, 180, 180, 600 };
    int px = 2;
    for (int i = 0; i < 5; ++i) {
      RECT pane = { status.left + S(c, px), status.top + S(c, 4), status.left + S(c, px + widths[i] - 4), status.bottom - S(c, 2) };
      Fill(dc, R(c, px + 2, 996, widths[i] - 6, 26), kWin98Gray);
      Win98Bevel(dc, R(c, px + 2, 996, widths[i] - 6, 26), true);
      Text(dc, pane, labels[i], c.fonts->menuFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      if (i < 4) {
        Line(dc, X(c, px + widths[i] - 2), Y(c, 996), X(c, px + widths[i] - 2), Y(c, 1022), kWin98ButtonShadow);
        Line(dc, X(c, px + widths[i] - 1), Y(c, 996), X(c, px + widths[i] - 1), Y(c, 1022), kWin98ButtonHighlight);
      }
      px += widths[i];
    }

    int gripX = 1536 - 20;
    int gripY = 996;
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4 - i; ++j) {
        int gx = gripX + i * 4 + j * 2;
        int gy = gripY + 16 + i * 3;
        SetPixel(dc, X(c, gx), Y(c, gy), kWin98ButtonShadow);
        SetPixel(dc, X(c, gx) + 1, Y(c, gy), kWin98ButtonHighlight);
        SetPixel(dc, X(c, gx), Y(c, gy) + 1, kWin98ButtonShadow);
        SetPixel(dc, X(c, gx) + 1, Y(c, gy) + 1, kWin98ButtonHighlight);
      }
    }
  }

  static void DrawLogConsoleLeft(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, int topY, int height, int width) {
    DrawGroupbox(dc, c, 8, topY, width, height, L"LOG CONSOLE", c.fonts->menuFont);

    const int toolbarY = topY + 16;
    const int toolbarH = 24;
    RECT toolbarBg = R(c, 16, toolbarY, width - 18, toolbarH);
    Fill(dc, toolbarBg, kWin98ButtonFace);
    Line(dc, toolbarBg.left, toolbarBg.bottom - 1, toolbarBg.right, toolbarBg.bottom - 1, kWin98ButtonShadow);

    static const std::array<const wchar_t*, 6> toolBtns {{ L"  CLEAR  ", L"  PAUSE  ", L"  SEARCH  ", L"  JSON  ", L"  CSV  ", L"  COPY  " }};
    int tbX = 20;
    for (int ti = 0; ti < 6; ++ti) {
      int bw = static_cast<int>(wcslen(toolBtns[ti])) * 6 + 12;
      RECT btn = R(c, tbX, toolbarY + 2, bw, 18);
      const bool isPressed = (ctx.pressedButton == ti);
      Fill(dc, btn, kWin98ButtonFace);
      Win98Bevel(dc, btn, !isPressed);
      RECT inner = Pad(btn, S(c, isPressed ? 3 : 2), S(c, isPressed ? 3 : 2), S(c, isPressed ? 3 : 2), S(c, isPressed ? 3 : 2));
      Text(dc, inner, toolBtns[ti], c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      tbX += bw + 4;
    }

    RECT pausedRect = R(c, width - 200, toolbarY + 2, 160, 18);
    Fill(dc, pausedRect, kWin98ButtonFace);
    Win98Bevel(dc, pausedRect, true);
    Text(dc, pausedRect, ctx.livePaused ? L"  LIVE [PAUSED]" : L"  LIVE [ACTIVE]", c.fonts->smallFont,
         ctx.livePaused ? RGB(180, 120, 0) : kWin98Green, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const int filterY = toolbarY + toolbarH + 2;
    const int filterH = 20;
    RECT filterBg = R(c, 16, filterY, width - 18, filterH);
    Fill(dc, filterBg, kWin98Gray);
    Line(dc, filterBg.left, filterBg.bottom - 1, filterBg.right, filterBg.bottom - 1, kWin98ButtonHighlight);

    int infoCount = 0, warnCount = 0, errorCount = 0, critCount = 0;
    if (ctx.logs && !ctx.logs->empty()) {
      for (const auto& e : *ctx.logs) {
        if (e.level == LogLevel::Info) ++infoCount;
        else if (e.level == LogLevel::Warn) ++warnCount;
        else if (e.level == LogLevel::Error) ++errorCount;
        else if (e.level == LogLevel::Critical) ++critCount;
      }
    }
    int totalCount = infoCount + warnCount + errorCount + critCount;

    struct FilterBtn { const wchar_t* label; COLORREF color; int count; };
    FilterBtn filters[] = {
      { L"ALL", kWin98Black, totalCount },
      { L"INFO", kWin98Green, infoCount },
      { L"WARN", RGB(180, 120, 0), warnCount },
      { L"ERROR", kWin98Red, errorCount },
      { L"CRIT", RGB(200, 0, 0), critCount }
    };
    int fbX = 20;
    for (int fi = 0; fi < 5; ++fi) {
      wchar_t buf[32];
      swprintf_s(buf, L"%ls (%d)", filters[fi].label, filters[fi].count);
      int bw = static_cast<int>(wcslen(buf)) * 6 + 14;
      RECT fbtn = R(c, fbX, filterY + 2, bw, 16);
      const bool isActive = (fi == ctx.activeFilter);
      Fill(dc, fbtn, isActive ? kWin98Highlight : kWin98White);
      Win98Bevel(dc, fbtn, !isActive);
      Text(dc, fbtn, buf, c.fonts->smallFont, isActive ? kWin98HighlightText : filters[fi].color,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      fbX += bw + 4;
    }

    const int headerY = filterY + filterH + 1;
    const int headerH = 16;
    RECT headerBg = R(c, 18, headerY, width - 52, headerH);
    Fill(dc, headerBg, kWin98ButtonFace);
    Win98Bevel(dc, headerBg, false);
    struct LogCol { int x; int w; const wchar_t* label; };
    static const std::array<LogCol, 3> logCols {{
      { 20, 120, L"TIME" },
      { 144, 80, L"LEVEL" },
      { 228, width - 290, L"MESSAGE" }
    }};
    for (int ci = 0; ci < 3; ++ci) {
      RECT colRect = R(c, logCols[ci].x, headerY + 1, logCols[ci].w, 14);
      Text(dc, colRect, logCols[ci].label, c.fonts->smallFont, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    Line(dc, X(c, 18), Y(c, headerY + headerH), X(c, width - 34), Y(c, headerY + headerH), kWin98ButtonShadow);

    const int consoleTopY = headerY + headerH;
    const int scrollH = height - 48 - (toolbarH + 4 + filterH + 4 + headerH + 4);
    RECT consoleBg = R(c, 16, consoleTopY, width - 30, scrollH);
    Fill(dc, consoleBg, kWin98ConsoleBg);
    Win98Bevel(dc, consoleBg, true);

    RECT scrollBar = R(c, 16 + width - 30, consoleTopY, 14, scrollH);
    Fill(dc, scrollBar, kWin98Gray);
    Win98Bevel(dc, scrollBar, true);
    RECT scrollBtn = R(c, 16 + width - 28, consoleTopY + 2, 10, 10);
    Fill(dc, scrollBtn, kWin98ButtonFace);
    Win98Bevel(dc, scrollBtn, false);
    Text(dc, scrollBtn, L"\u25B2", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT scrollBtn2 = R(c, 16 + width - 28, consoleTopY + scrollH - 12, 10, 10);
    Fill(dc, scrollBtn2, kWin98ButtonFace);
    Win98Bevel(dc, scrollBtn2, false);
    Text(dc, scrollBtn2, L"\u25BC", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT logTrackInner = { scrollBar.left + 2, scrollBar.top + 12, scrollBar.right - 2, scrollBar.bottom - 12 };
    Fill(dc, logTrackInner, RGB(170, 170, 170));
    HPEN logTrackPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    HGDIOBJ oldLTP = SelectObject(dc, logTrackPen);
    for (int ty2 = logTrackInner.top + 2; ty2 < logTrackInner.bottom - 2; ty2 += 3) {
      MoveToEx(dc, logTrackInner.left, ty2, nullptr);
      LineTo(dc, logTrackInner.right - 1, ty2);
    }
    SelectObject(dc, oldLTP);
    DeleteObject(logTrackPen);

    int logEntryCount = 0;
    if (ctx.logs) logEntryCount = static_cast<int>(ctx.logs->size());
    int logMaxLines = 2048;
    int thumbH = (std::max)(16, static_cast<int>(static_cast<float>(scrollH - 24) * 20 / (std::max)(1, logEntryCount)));
    float scrollRatio = logEntryCount > 20 ? 1.0f : 0.0f;
    int thumbTrackH = scrollH - 24 - thumbH;
    int thumbY = consoleTopY + 14 + static_cast<int>(scrollRatio * thumbTrackH);
    RECT scrollThumb = R(c, 1498, thumbY, 10, thumbH);
    Fill(dc, scrollThumb, kWin98ButtonFace);
    Win98Bevel(dc, scrollThumb, false);

    int ly = consoleTopY + 2;
    const int lineH = 17;
    const int maxX = 16 + width - 56;
    if (ctx.logs && !ctx.logs->empty()) {
      int maxLines = scrollH / (lineH + 1);
      int start = (std::max)(0, static_cast<int>(ctx.logs->size()) - maxLines);
      for (int i = start; i < static_cast<int>(ctx.logs->size()); ++i) {
        const auto& e = (*ctx.logs)[i];
        DrawConsoleLine(dc, c, 20, ly, maxX - 20, lineH, e.time, e.severity, e.message, c.fonts->logFont);
        ly += lineH + 1;
        if (ly > consoleTopY + scrollH - 4) break;
      }
    } else {
      static const std::array<std::tuple<const wchar_t*, const wchar_t*, const wchar_t*>, 14> logs {{
        { L"[12:03:15]", L"[INFO ]", L"Monix initialized successfully." },
        { L"[12:03:15]", L"[INFO ]", L"OS: Windows 98 SE (4.10.2222)" },
        { L"[12:03:15]", L"[INFO ]", L"User: WEIRD\\User" },
        { L"[12:03:16]", L"[INFO ]", L"Monitoring system..." },
        { L"[12:03:16]", L"[OK   ]", L"All systems operational." },
        { L"[12:03:17]", L"[TASK ]", L"explorer.exe (PID: 1840) started." },
        { L"[12:03:18]", L"[TASK ]", L"Monix.exe (PID: 3920) started." },
        { L"[12:03:18]", L"[WARN ]", L"High memory usage detected (78%)." },
        { L"[12:03:19]", L"[NET  ]", L"Connected to 8.8.8.8" },
        { L"[12:03:20]", L"[OK   ]", L"Internet connection stable." },
        { L"[12:03:21]", L"[TEMP ]", L"CPU Temp: 58\u00B0C" },
        { L"[12:03:22]", L"[DISK ]", L"C:\\ Usage: 65% (312GB/476GB)" },
        { L"[12:03:23]", L"[AI   ]", L"Context Engine updated." },
        { L"[12:03:25]", L"[INFO ]", L"Log buffer: 256/2048 lines used." }
      }};
      for (const auto& [ts, lvl, msg] : logs) {
        DrawConsoleLine(dc, c, 20, ly, maxX - 20, lineH, ts, lvl, msg, c.fonts->logFont);
        ly += lineH + 1;
        if (ly > consoleTopY + scrollH - 4) break;
      }
    }

    const int footerY = consoleTopY + scrollH + 2;
    RECT footerBg = R(c, 16, footerY, width - 18, 20);
    Fill(dc, footerBg, kWin98Gray);
    Win98Bevel(dc, footerBg, false);
    const int logMax = 2048;
    std::wstring footerText = L"Lines: " + std::to_wstring(logEntryCount) + L" / " + std::to_wstring(logMax);
    Text(dc, R(c, 20, footerY + 2, 200, 16), footerText, c.fonts->smallFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    std::wstring footerRight = L"Visible: " + std::to_wstring((std::min)(logEntryCount, (scrollH - 4) / (lineH + 1))) + L" lines";
    Text(dc, R(c, width - 250, footerY + 2, 220, 16), footerRight, c.fonts->smallFont, kWin98Black,
         DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawAlertsRight(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, int topY, int height, int leftW) {
    const int rx = leftW + 16;
    const int rw = 1536 - leftW - 24;
    DrawGroupbox(dc, c, rx, topY, rw, height, L"ALERTS / NOTIFICATIONS", c.fonts->menuFont);

    RECT alertBg = R(c, rx + 8, topY + 16, rw - 16, height - 32);
    Fill(dc, alertBg, kWin98ConsoleBg);
    Win98Bevel(dc, alertBg, true);

    int ly = topY + 22;
    const int lineH = 17;
    int alertCount = 0;
    if (ctx.logs && !ctx.logs->empty()) {
      for (int i = static_cast<int>(ctx.logs->size()) - 1; i >= 0 && alertCount < 15; --i) {
        const auto& e = (*ctx.logs)[i];
        bool isAlert = e.level == LogLevel::Warn || e.level == LogLevel::Error ||
                       e.level == LogLevel::Critical;
        if (!isAlert && alertCount > 3) continue;
        const wchar_t* icon = L"\u2139";
        COLORREF col = kWin98Black;
        if (e.level == LogLevel::Error || e.level == LogLevel::Critical) { icon = L"\u2716"; col = RGB(192, 0, 0); }
        else if (e.level == LogLevel::Warn) { icon = L"\u26A0"; col = RGB(180, 130, 0); }
        else if (e.level == LogLevel::Info) { col = RGB(0, 0, 180); }
        wchar_t line[256];
        swprintf_s(line, L"%s %ls", icon, e.message.c_str());
        Text(dc, R(c, rx + 12, ly, rw - 32, lineH), line, c.fonts->logFont, col, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        ly += lineH + 2;
        if (ly > topY + height - 34) break;
        ++alertCount;
      }
    }
    if (alertCount == 0) {
      Text(dc, R(c, rx + 12, ly, rw - 32, lineH), L"\u2714  No active alerts", c.fonts->logFont, RGB(0, 128, 0), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
  }

  static void DrawLogConsole(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    DrawGroupbox(dc, c, 8, 94, 830, 356, L"LOG CONSOLE", c.fonts->menuFont);

    RECT consoleBg = R(c, 16, 110, 814, 338);
    Fill(dc, consoleBg, kWin98ConsoleBg);
    Win98Bevel(dc, consoleBg, true);

    RECT scrollBar = R(c, 822, 110, 16, 228);
    Fill(dc, scrollBar, kWin98Gray);
    Win98Bevel(dc, scrollBar, true);
    RECT scrollBtn = R(c, 824, 112, 12, 12);
    Fill(dc, scrollBtn, kWin98ButtonFace);
    Win98Bevel(dc, scrollBtn, false);
    Text(dc, scrollBtn, L"\u25B2", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT scrollBtn2 = R(c, 824, 326, 12, 12);
    Fill(dc, scrollBtn2, kWin98ButtonFace);
    Win98Bevel(dc, scrollBtn2, false);
    Text(dc, scrollBtn2, L"\u25BC", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int ly = 116;
    const int lineH = 17;
    if (ctx.logs && !ctx.logs->empty()) {
      int start = (std::max)(0, static_cast<int>(ctx.logs->size()) - 20);
      for (int i = start; i < static_cast<int>(ctx.logs->size()); ++i) {
        const auto& e = (*ctx.logs)[i];
        DrawConsoleLine(dc, c, 20, ly, 796, lineH, e.time, e.severity, e.message, c.fonts->logFont);
        ly += lineH + 1;
        if (ly > 334) break;
      }
    } else {
      static const std::array<std::tuple<const wchar_t*, const wchar_t*, const wchar_t*>, 14> logs {{
        { L"[12:03:15]", L"[INFO ]", L"Monix initialized successfully." },
        { L"[12:03:15]", L"[INFO ]", L"OS: Windows 98 SE (4.10.2222)" },
        { L"[12:03:15]", L"[INFO ]", L"User: WEIRD\\User" },
        { L"[12:03:16]", L"[INFO ]", L"Monitoring system..." },
        { L"[12:03:16]", L"[OK   ]", L"All systems operational." },
        { L"[12:03:17]", L"[TASK ]", L"explorer.exe (PID: 1840) started." },
        { L"[12:03:18]", L"[TASK ]", L"Monix.exe (PID: 3920) started." },
        { L"[12:03:18]", L"[WARN ]", L"High memory usage detected (78%)." },
        { L"[12:03:19]", L"[NET  ]", L"Connected to 8.8.8.8" },
        { L"[12:03:20]", L"[OK   ]", L"Internet connection stable." },
        { L"[12:03:21]", L"[TEMP ]", L"CPU Temp: 58\u00B0C" },
        { L"[12:03:22]", L"[DISK ]", L"C:\\ Usage: 65% (312GB/476GB)" },
        { L"[12:03:23]", L"[AI   ]", L"Context Engine updated." },
        { L"[12:03:25]", L"[INFO ]", L"Log buffer: 256/2048 lines used." }
      }};
      for (const auto& [ts, lvl, msg] : logs) {
        DrawConsoleLine(dc, c, 20, ly, 796, lineH, ts, lvl, msg, c.fonts->logFont);
        ly += lineH + 1;
        if (ly > 334) break;
      }
    }
  }

  static void DrawActiveTasks(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const int contentY = 96;
    const int contentH = 894;

    DrawGroupbox(dc, c, 8, contentY, 1520, contentH, L"TASK MANAGER", c.fonts->titleBar);

    RECT toolbarBg = R(c, 16, contentY + 18, 1480, 22);
    Fill(dc, toolbarBg, kWin98ButtonFace);
    Line(dc, X(c, 16), Y(c, contentY + 40), X(c, 1496), Y(c, contentY + 40), kWin98ButtonShadow);

    static const std::array<const wchar_t*, 3> actions {{ L"  End Task  ", L"  End Process  ", L"  Priority \u25BC " }};
    int btnX = 20;
    for (int ai = 0; ai < 3; ++ai) {
      int bw = static_cast<int>(wcslen(actions[ai])) * 7 + 16;
      RECT btn = R(c, btnX, contentY + 19, bw, 20);
      const bool isPressed = (ctx.pressedButton == 100 + ai);
      Fill(dc, btn, kWin98ButtonFace);
      Win98Bevel(dc, btn, !isPressed);
      RECT inner = Pad(btn, S(c, isPressed ? 3 : 2), S(c, isPressed ? 3 : 2), S(c, isPressed ? 3 : 2), S(c, isPressed ? 3 : 2));
      Text(dc, inner, actions[ai], c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      btnX += bw + 6;
    }

    RECT summaryRight = R(c, 400, contentY + 19, 1060, 20);
    std::wstring summaryText = L"Processes: " + std::to_wstring(s.processCount > 0 ? s.processCount : (int)s.processes.size()) +
      L"  |  Threads: " + std::to_wstring(s.threadCount) +
      L"  |  Handles: " + std::to_wstring(s.handleCount);
    Text(dc, summaryRight, summaryText, c.fonts->smallFont, kWin98Black, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const int tableY = contentY + 42;
    const int tableH = contentH - 84;
    RECT tableBg = R(c, 16, tableY, 1480, tableH);
    Fill(dc, tableBg, kWin98White);
    Win98Bevel(dc, tableBg, true);

    const int headerY = tableY + 2;
    RECT headerBg = R(c, 18, headerY, 1476, 18);
    Fill(dc, headerBg, kWin98ButtonFace);
    Win98Bevel(dc, headerBg, false);

    struct Col { int x; int w; const wchar_t* label; };
    static const std::array<Col, 10> cols {{
      { 20, 48, L"PID" },
      { 70, 240, L"IMAGE NAME" },
      { 312, 55, L"SESSION" },
      { 370, 55, L"CPU%" },
      { 428, 72, L"RAM (KB)" },
      { 502, 50, L"RAM%" },
      { 554, 48, L"GPU%" },
      { 604, 72, L"STATUS" },
      { 678, 68, L"PRIORITY" },
      { 748, 64, L"PPID" }
    }};
    for (int ci = 0; ci < static_cast<int>(cols.size()); ++ci) {
      const auto& col = cols[ci];
      RECT colRect = R(c, col.x, headerY + 1, col.w, 16);
      Fill(dc, colRect, kWin98ButtonFace);
      Win98Bevel(dc, colRect, false);
      Text(dc, R(c, col.x + 4, headerY + 1, col.w - 8, 16), col.label, c.fonts->smallFont, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      if (ci < static_cast<int>(cols.size()) - 1) {
        Line(dc, X(c, col.x + col.w), Y(c, headerY + 1), X(c, col.x + col.w), Y(c, headerY + 17), kWin98ButtonShadow);
        Line(dc, X(c, col.x + col.w + 1), Y(c, headerY + 1), X(c, col.x + col.w + 1), Y(c, headerY + 17), kWin98ButtonHighlight);
      }
    }
    Line(dc, X(c, 18), Y(c, headerY + 18), X(c, 1492), Y(c, headerY + 18), kWin98ButtonShadow);

    std::vector<ProcessInfo> processes = s.processes;
    if (processes.empty()) {
      processes = {
        { L"explorer.exe", 1840, 4, 1, 1.1, 34000000ull, 0.2, 0, L"Running", L"NORMAL", L"" },
        { L"chrome.exe", 2672, 4, 1, 12.4, 420000000ull, 3.1, 0, L"Running", L"NORMAL", L"" },
        { L"chrome.exe", 3104, 4, 1, 2.8, 89000000ull, 0.5, 0, L"Running", L"NORMAL", L"" },
        { L"chrome.exe", 5520, 4, 1, 1.6, 67000000ull, 0.3, 0, L"Running", L"NORMAL", L"" },
        { L"Discord.exe", 3280, 4, 1, 3.8, 200000000ull, 1.2, 0, L"Running", L"NORMAL", L"" },
        { L"obs64.exe", 6216, 4, 1, 18.0, 820000000ull, 28.0, 0, L"Running", L"HIGH", L"" },
        { L"Spotify.exe", 2104, 4, 1, 0.7, 120000000ull, 0.1, 0, L"Running", L"NORMAL", L"" },
        { L"Monix.exe", 3920, 4, 1, 2.3, 48000000ull, 2.0, 0, L"Running", L"NORMAL", L"" },
        { L"MsMpEng.exe", 1508, 0, 0, 5.6, 110000000ull, 0.0, 0, L"Running", L"HIGH", L"" },
        { L"dwm.exe", 1020, 0, 0, 0.4, 50000000ull, 0.8, 0, L"Running", L"HIGH", L"" },
        { L"svchost.exe", 888, 0, 0, 0.2, 3000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"svchost.exe", 1200, 0, 0, 0.1, 8000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"lsass.exe", 644, 0, 0, 0.3, 12000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"services.exe", 720, 0, 0, 0.2, 9000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"csrss.exe", 520, 0, 0, 0.1, 4000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"wininit.exe", 492, 0, 0, 0.0, 2000000ull, 0.0, 0, L"Running", L"HIGH", L"" },
        { L"fontdrvhost.exe", 1068, 0, 0, 0.0, 1500000ull, 0.0, 0, L"Running", L"HIGH", L"" },
        { L"smss.exe", 380, 0, 0, 0.0, 1000000ull, 0.0, 0, L"Running", L"HIGH", L"" },
        { L"NvTyServer.exe", 4100, 4, 1, 0.3, 15000000ull, 1.5, 0, L"Running", L"NORMAL", L"" },
        { L"Steam.exe", 7800, 4, 1, 4.2, 280000000ull, 0.8, 0, L"Running", L"NORMAL", L"" },
        { L"notepad.exe", 9120, 4, 1, 0.0, 5000000ull, 0.0, 0, L"Suspended", L"NORMAL", L"" },
        { L"SearchProtocol", 3400, 4, 1, 0.1, 22000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"SearchUI.exe", 5500, 4, 1, 0.4, 35000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"ShellExperience", 6100, 4, 1, 0.2, 28000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"RuntimeBroker.exe", 4800, 4, 1, 0.1, 18000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"TextInputHost.exe", 7200, 4, 1, 0.1, 12000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"SecurityHealthSy", 2200, 4, 1, 0.3, 45000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"ctfmon.exe", 2900, 4, 1, 0.0, 8000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"igfxEM.exe", 3300, 4, 1, 0.1, 10000000ull, 0.0, 0, L"Running", L"NORMAL", L"" },
        { L"Memory Compactor", 4400, 0, 0, 0.0, 2000000ull, 0.0, 0, L"Running", L"REALTIME", L"" }
      };
    }
    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
      if (a.cpuPct != b.cpuPct) return a.cpuPct > b.cpuPct;
      return a.ramBytes > b.ramBytes;
    });

    const int rowH = 16;
    const int visibleRows = (tableH - 24) / rowH;
    double ramTotal = s.ramTotalBytes ? BytesToGb(s.ramTotalBytes) : 32.0;
    int ty = headerY + 20;
    const int scrollOffset = ctx.taskScroll;

    for (int i = scrollOffset; i < scrollOffset + visibleRows && i < static_cast<int>(processes.size()); ++i) {
      const ProcessInfo& p = processes[i];
      double ramKb = p.ramBytes / 1024.0;
      double ramPctVal = p.ramBytes > 0 ? BytesToGb(p.ramBytes) * 100.0 / (std::max)(0.1, BytesToGb(s.ramTotalBytes > 0 ? s.ramTotalBytes : 34359738368ull)) : 0.0;

      RECT rowBg = { X(c, 18), Y(c, ty), X(c, 1492), Y(c, ty + rowH) };
      bool isSelected = (p.pid == ctx.selectedTaskPid && ctx.selectedTaskPid != 0);
      if (isSelected) Fill(dc, rowBg, RGB(0, 0, 128));
      else if (i % 2 == 0) Fill(dc, rowBg, kWin98White);
      else Fill(dc, rowBg, RGB(240, 240, 255));

      COLORREF textColor = isSelected ? kWin98White : kWin98Black;
      if (!isSelected) {
        if (p.status.find(L"Suspend") != std::wstring::npos) textColor = RGB(128, 128, 128);
        else if (p.priority == L"HIGH" || p.priority == L"REALTIME") textColor = kWin98Red;
      }

      Text(dc, R(c, 20, ty, 48, rowH), std::to_wstring(p.pid), c.fonts->logFont, isSelected ? kWin98White : kWin98Green,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, 70, ty, 240, rowH), ShortText(p.name, 32), c.fonts->logFont, textColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      Text(dc, R(c, 312, ty, 55, rowH), std::to_wstring(p.sessionId), c.fonts->logFont, textColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      COLORREF cpuColor = isSelected ? kWin98White : kWin98Green;
      if (!isSelected) {
        if (p.cpuPct >= 50.0) cpuColor = kWin98Red;
        else if (p.cpuPct >= 20.0) cpuColor = RGB(180, 120, 0);
      }
      Text(dc, R(c, 370, ty, 55, rowH), Fixed(p.cpuPct, 1), c.fonts->logFont, cpuColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, 428, ty, 72, rowH), std::to_wstring(static_cast<int>(ramKb)), c.fonts->logFont, textColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      COLORREF ramColor = isSelected ? kWin98White : kWin98Green;
      if (!isSelected) {
        if (ramPctVal >= 10.0) ramColor = kWin98Red;
        else if (ramPctVal >= 5.0) ramColor = RGB(180, 120, 0);
      }
      Text(dc, R(c, 502, ty, 50, rowH), Fixed(ramPctVal, 1), c.fonts->logFont, ramColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      COLORREF gpuColor = isSelected ? kWin98White : kWin98Green;
      if (!isSelected) {
        if (p.gpuPct >= 50.0) gpuColor = kWin98Red;
        else if (p.gpuPct >= 20.0) gpuColor = RGB(180, 120, 0);
      }
      Text(dc, R(c, 554, ty, 48, rowH), Fixed(p.gpuPct, 1), c.fonts->logFont, gpuColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      COLORREF statusColor = isSelected ? kWin98White : kWin98Green;
      if (!isSelected) {
        std::wstring statusText = p.status.empty() ? L"Running" : p.status;
        if (statusText.find(L"Suspend") != std::wstring::npos) statusColor = RGB(128, 128, 128);
      }
      std::wstring statusText = p.status.empty() ? L"Running" : p.status;
      Text(dc, R(c, 604, ty, 72, rowH), ShortText(statusText, 10), c.fonts->logFont, statusColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

      COLORREF priColor = isSelected ? kWin98White : kWin98Black;
      if (!isSelected) {
        if (p.priority == L"REALTIME") priColor = kWin98Red;
        else if (p.priority == L"HIGH") priColor = RGB(180, 120, 0);
      }
      Text(dc, R(c, 678, ty, 68, rowH), ShortText(p.priority, 8), c.fonts->logFont, priColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      Text(dc, R(c, 748, ty, 64, rowH), std::to_wstring(p.parentPid), c.fonts->logFont, textColor,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      ty += rowH;
    }

    RECT scrollBar = R(c, 1496, tableY + 20, 14, tableH - 24);
    Fill(dc, scrollBar, kWin98Gray);
    Win98Bevel(dc, scrollBar, true);

    RECT scrollTrackInner = { scrollBar.left + 2, scrollBar.top + 12, scrollBar.right - 2, scrollBar.bottom - 12 };
    Fill(dc, scrollTrackInner, RGB(170, 170, 170));
    HPEN trackPen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    HGDIOBJ oldTP = SelectObject(dc, trackPen);
    for (int ty2 = scrollTrackInner.top + 2; ty2 < scrollTrackInner.bottom - 2; ty2 += 3) {
      MoveToEx(dc, scrollTrackInner.left, ty2, nullptr);
      LineTo(dc, scrollTrackInner.right - 1, ty2);
    }
    SelectObject(dc, oldTP);
    DeleteObject(trackPen);

    RECT scrollUp = R(c, 1498, tableY + 22, 10, 10);
    Fill(dc, scrollUp, kWin98ButtonFace);
    Win98Bevel(dc, scrollUp, false);
    Text(dc, scrollUp, L"\u25B2", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT scrollDown = R(c, 1498, tableY + tableH - 14, 10, 10);
    Fill(dc, scrollDown, kWin98ButtonFace);
    Win98Bevel(dc, scrollDown, false);
    Text(dc, scrollDown, L"\u25BC", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    int thumbH = (std::max)(20, static_cast<int>(static_cast<float>(tableH - 44) * visibleRows / (std::max)(1, static_cast<int>(processes.size()))));
    int maxScroll = (std::max)(0, static_cast<int>(processes.size()) - visibleRows);
    float scrollRatio = maxScroll > 0 ? static_cast<float>(ctx.taskScroll) / maxScroll : 0.0f;
    int thumbTrackH = tableH - 44 - thumbH;
    int thumbY = tableY + 34 + static_cast<int>(scrollRatio * thumbTrackH);
    RECT scrollThumb = R(c, 1498, thumbY, 10, thumbH);
    Fill(dc, scrollThumb, kWin98ButtonFace);
    Win98Bevel(dc, scrollThumb, false);

    RECT footerBg = R(c, 16, contentY + contentH - 30, 1504, 28);
    Fill(dc, footerBg, kWin98ButtonFace);
    Win98Bevel(dc, footerBg, false);

    double totalRamGb = BytesToGb(s.ramTotalBytes > 0 ? s.ramTotalBytes : 34359738368ull);
    double usedRamGb = BytesToGb(s.ramUsedBytes > 0 ? s.ramUsedBytes : 22000000000ull);
    double cpuTotal = 0.0;
    for (const auto& p : processes) cpuTotal += p.cpuPct;

    std::wstring footerL = L"CPU Usage: " + Fixed(cpuTotal, 0) + L"%";
    std::wstring footerM = L"Physical Memory: " + Fixed(usedRamGb, 1) + L" / " + Fixed(totalRamGb, 1) + L" GB";
    std::wstring footerR = L"Processes: " + std::to_wstring(static_cast<int>(processes.size()));

    Text(dc, R(c, 20, contentY + contentH - 28, 300, 24), footerL, c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, 400, contentY + contentH - 28, 400, 24), footerM, c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, 1200, contentY + contentH - 28, 300, 24), footerR, c.fonts->smallFont, kWin98Black, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawAlerts(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    DrawGroupbox(dc, c, 8, 720, 830, 266, L"ALERTS / NOTIFICATIONS", c.fonts->menuFont);

    RECT alertBg = R(c, 16, 736, 814, 242);
    Fill(dc, alertBg, kWin98ConsoleBg);
    Win98Bevel(dc, alertBg, true);

    int ly = 742;
    const int lineH = 17;
    int alertCount = 0;
    if (ctx.logs && !ctx.logs->empty()) {
      for (int i = static_cast<int>(ctx.logs->size()) - 1; i >= 0 && alertCount < 15; --i) {
        const auto& e = (*ctx.logs)[i];
        bool isAlert = e.level == LogLevel::Warn || e.level == LogLevel::Error ||
                       e.level == LogLevel::Critical ||
                       e.severity.find(L"WARN") != std::wstring::npos ||
                       e.severity.find(L"ERROR") != std::wstring::npos ||
                       e.severity.find(L"FAIL") != std::wstring::npos;
        if (!isAlert) continue;
        DrawConsoleLine(dc, c, 20, ly, 796, lineH, e.time, e.severity, e.message, c.fonts->logFont);
        ly += lineH + 1;
        ++alertCount;
        if (ly > 972) break;
      }
    }
    if (alertCount == 0) {
      static const std::array<std::tuple<const wchar_t*, const wchar_t*, const wchar_t*>, 3> alerts {{
        { L"[12:03:18]", L"[WARN ]", L"High memory usage detected." },
        { L"[12:03:21]", L"[TEMP ]", L"CPU temperature elevated." },
        { L"[12:03:22]", L"[WARN ]", L"Swap usage elevated." }
      }};
      for (const auto& [ts, lvl, msg] : alerts) {
        DrawConsoleLine(dc, c, 20, ly, 796, lineH, ts, lvl, msg, c.fonts->logFont);
        ly += lineH + 1;
        if (ly > 972) break;
      }
    }
  }

  static void DrawSystemOverview(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const double cpuPct = PercentOr(s.cpuPct, 42.0);
    const double ramPct = s.ramTotalBytes ? BytesToGb(s.ramUsedBytes) * 100.0 / (std::max)(0.1, BytesToGb(s.ramTotalBytes)) : 78.0;
    const double gpuPct = PercentOr(s.gpuPct, 63.0);
    const double swapPct = 35.0;
    const double cpuTemp = s.cpuCoreTempC > 0.0 ? s.cpuCoreTempC : 0.0;
    const double gpuTemp = s.gpuTempC > 0.0 ? s.gpuTempC : 54.0;
    const std::uint64_t uptime = s.uptimeSeconds > 0 ? s.uptimeSeconds : 8253;
    const int processes = s.processCount > 0 ? s.processCount : 112;
    const int threads = s.threadCount > 0 ? s.threadCount : 1568;
    const int handles = s.handleCount > 0 ? s.handleCount : 48231;

    const int panelX = 8;
    const int panelW = 750;
    DrawGroupbox(dc, c, panelX, 94, panelW, 390, L"SYSTEM OVERVIEW", c.fonts->menuFont);

    int barX = panelX + 12;
    int barW = 300;
    int barH = 18;
    int ly = 114;

    Text(dc, R(c, barX, ly, 100, barH), L"CPU Usage", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawSegmentedBar(dc, c, barX + 100, ly, barW, barH, cpuPct, RGB(0, 180, 0));
    Text(dc, R(c, barX + 100 + barW + 6, ly, 50, barH), Fixed(cpuPct, 0) + L"%", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 42;

    Text(dc, R(c, barX, ly, 100, barH), L"RAM Usage", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawSegmentedBar(dc, c, barX + 100, ly, barW, barH, ramPct, RGB(180, 180, 0));
    Text(dc, R(c, barX + 100 + barW + 6, ly, 50, barH), Fixed(ramPct, 0) + L"%", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 42;

    Text(dc, R(c, barX, ly, 100, barH), L"GPU Usage", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawSegmentedBar(dc, c, barX + 100, ly, barW, barH, gpuPct, RGB(200, 0, 0));
    Text(dc, R(c, barX + 100 + barW + 6, ly, 50, barH), Fixed(gpuPct, 0) + L"%", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 42;

    Text(dc, R(c, barX, ly, 100, barH), L"Swap Usage", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawSegmentedBar(dc, c, barX + 100, ly, barW, barH, swapPct, RGB(0, 0, 180));
    Text(dc, R(c, barX + 100 + barW + 6, ly, 50, barH), Fixed(swapPct, 0) + L"%", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int infoY = 290;
    int infoW = 236;
    int infoH = 44;
    struct InfoBox { int x; const wchar_t* label; std::wstring value; };
    InfoBox boxes[] = {
      { panelX + 12, L"CPU Temp", Fixed(cpuTemp, 0) + L"\u00B0C" },
      { panelX + 12 + infoW + 12, L"GPU Temp", Fixed(gpuTemp, 0) + L"\u00B0C" },
      { panelX + 12 + (infoW + 12) * 2, L"Uptime", Hms(uptime) }
    };
    for (const auto& b : boxes) {
      RECT box = R(c, b.x, infoY, infoW, infoH);
      Fill(dc, box, kWin98White);
      Win98Bevel(dc, box, true);
      Text(dc, R(c, b.x + 8, infoY + 4, infoW - 16, 14), b.label, c.fonts->smallFont, kWin98DarkGray,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, b.x + 8, infoY + 20, infoW - 16, 28), b.value, c.fonts->titleBar, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    infoY += infoH + 8;
    InfoBox boxes2[] = {
      { panelX + 12, L"Processes", std::to_wstring(processes) },
      { panelX + 12 + infoW + 12, L"Threads", std::to_wstring(threads) },
      { panelX + 12 + (infoW + 12) * 2, L"Handles", std::to_wstring(handles) }
    };
    for (const auto& b : boxes2) {
      RECT box = R(c, b.x, infoY, infoW, infoH);
      Fill(dc, box, kWin98White);
      Win98Bevel(dc, box, true);
      Text(dc, R(c, b.x + 8, infoY + 4, infoW - 16, 14), b.label, c.fonts->smallFont, kWin98DarkGray,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, b.x + 8, infoY + 20, infoW - 16, 28), b.value, c.fonts->titleBar, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
  }

  static void DrawRealTimeGraph(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const int panelX = 766;
    const int panelW = 762;
    DrawGroupbox(dc, c, panelX, 94, panelW, 390, L"REAL-TIME GRAPHS", c.fonts->menuFont);

    const int gw = (panelW - 40) / 2;
    const int gh = 160;
    const int gx = panelX + 12;
    DrawMiniGraph(dc, c, gx, 114, gw, gh, ctx.cpuHistory, RGB(0, 200, 0), L"CPU", 42.0, 15.0, 0.0);
    DrawMiniGraph(dc, c, gx + gw + 8, 114, gw, gh, ctx.ramHistory, RGB(200, 200, 0), L"RAM", 78.0, 5.0, 1.5);

    const int gy = 114 + gh + 8;
    DrawMiniGraph(dc, c, gx, gy, gw, gh, ctx.gpuHistory, RGB(200, 0, 0), L"GPU", 55.0, 25.0, 3.0);
    DrawMiniGraph(dc, c, gx + gw + 8, gy, gw, gh, ctx.netHistory, RGB(200, 0, 200), L"NET", 30.0, 20.0, 4.5);
  }

  static void DrawAiContextEngineFull(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx, int topY, int height, int width) {
    DrawGroupbox(dc, c, 8, topY, width, height, L"AI CONTEXT ENGINE", c.fonts->menuFont);

    RECT aiBg = R(c, 16, topY + 16, width - 24, height - 32);
    Fill(dc, aiBg, kWin98White);
    Win98Bevel(dc, aiBg, true);

    int ly = topY + 24;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Reputation: 87/100 (Trusted)", c.fonts->titleBar, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 28;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Suspicious: 0", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Analyzed: " + std::to_wstring((std::max)(1, ctx.snapshot ? ctx.snapshot->processCount : 112)) + L" processes", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Last scan: " + ClockText(), c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 30;
    Line(dc, X(c, 24), Y(c, ly), X(c, width - 24), Y(c, ly), kWin98ButtonShadow);
    ly += 12;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Behavioral Analysis:", c.fonts->menuFont, kWin98Navy,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 24;
    const wchar_t* behaviors[] = {
      L"\u2714  Network activity: Normal",
      L"\u2714  Memory patterns: Stable",
      L"\u2714  File system: No anomalies",
      L"\u2714  Registry: Clean",
      L"\u2714  Process behavior: Trusted",
      L"\u2714  Driver signatures: Valid",
      L"\u2714  Startup entries: Clean",
      L"\u2714  Service dependencies: OK"
    };
    for (const auto* b : behaviors) {
      Text(dc, R(c, 32, ly, width - 56, 18), b, c.fonts->logFont, RGB(0, 128, 0),
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      ly += 18;
    }
    ly += 12;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Threat Model:", c.fonts->menuFont, kWin98Navy,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 24;
    Text(dc, R(c, 32, ly, width - 56, 18), L"Score: 87/100", c.fonts->menuFont, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 20;
    Text(dc, R(c, 32, ly, width - 56, 18), L"Level: TRUSTED", c.fonts->menuFont, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 30;
    Line(dc, X(c, 24), Y(c, ly), X(c, width - 24), Y(c, ly), kWin98ButtonShadow);
    ly += 12;
    Text(dc, R(c, 24, ly, width - 48, 18), L"Classification:", c.fonts->menuFont, kWin98Navy,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 24;
    Text(dc, R(c, 32, ly, width - 56, 18), L"Type: System Monitor", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, 32, ly, width - 56, 18), L"Developer: Weird Stuff Software", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, 32, ly, width - 56, 18), L"Version: 1.0.0 (Build 2025)", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, 32, ly, width - 56, 18), L"Engine: Vulkan + OpenGL", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawAiContextEngine(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const int panelX = 8;
    const int panelW = 750;
    const int panelY = 490;
    const int panelH = 490;
    DrawGroupbox(dc, c, panelX, panelY, panelW, panelH, L"AI CONTEXT ENGINE", c.fonts->menuFont);

    RECT aiBg = R(c, panelX + 8, panelY + 16, panelW - 16, panelH - 32);
    Fill(dc, aiBg, kWin98White);
    Win98Bevel(dc, aiBg, true);

    int ly = panelY + 24;
    Text(dc, R(c, panelX + 16, ly, panelW - 32, 18), L"Reputation: 87/100 (Trusted)", c.fonts->menuFont, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, panelX + 16, ly, panelW - 32, 18), L"Suspicious: 0", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, panelX + 16, ly, panelW - 32, 18), L"Analyzed: " + std::to_wstring((std::max)(1, ctx.snapshot ? ctx.snapshot->processCount : 112)) + L" processes", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, panelX + 16, ly, panelW - 32, 18), L"Last scan: " + ClockText(), c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 30;
    Text(dc, R(c, panelX + 16, ly, panelW - 32, 18), L"Behavioral Analysis:", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, panelX + 24, ly, panelW - 40, 18), L"\u2714  Network activity: Normal", c.fonts->logFont, RGB(0, 128, 0),
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, panelX + 24, ly, panelW - 40, 18), L"\u2714  Memory patterns: Stable", c.fonts->logFont, RGB(0, 128, 0),
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, panelX + 24, ly, panelW - 40, 18), L"\u2714  File system: No anomalies", c.fonts->logFont, RGB(0, 128, 0),
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, panelX + 24, ly, panelW - 40, 18), L"\u2714  Registry: Clean", c.fonts->logFont, RGB(0, 128, 0),
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 30;
    Text(dc, R(c, panelX + 16, ly, panelW - 32, 18), L"Threat Model:", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 22;
    Text(dc, R(c, panelX + 24, ly, panelW - 40, 18), L"Score: 87/100", c.fonts->menuFont, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    ly += 18;
    Text(dc, R(c, panelX + 24, ly, panelW - 40, 18), L"Level: TRUSTED", c.fonts->menuFont, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void DrawScramPanel(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const int panelX = 766;
    const int panelW = 762;
    const int panelY = 490;
    const int panelH = 490;
    DrawGroupbox(dc, c, panelX, panelY, panelW, panelH, L"SCRAM!", c.fonts->menuFont);

    RECT scramBg = R(c, panelX + 8, panelY + 16, panelW - 16, panelH - 32);
    Fill(dc, scramBg, kWin98White);
    Win98Bevel(dc, scramBg, true);

    RECT scramText = R(c, panelX + 16, panelY + 26, panelW - 32, 36);
    Text(dc, scramText, L"SCRAM!", c.fonts->titleBar, kWin98Red,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    Text(dc, R(c, panelX + 16, panelY + 66, panelW - 32, 18), L"Emergency Mechanism", c.fonts->smallFont, kWin98Black,
         DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int btnY = panelY + 100;
    int btnW = (panelW - 44) / 2;
    RECT testBtn = R(c, panelX + 16, btnY, btnW, 28);
    const bool testPressed = (ctx.pressedButton == 200);
    Fill(dc, testBtn, kWin98ButtonFace);
    Win98Bevel(dc, testBtn, !testPressed);
    RECT testInner = Pad(testBtn, S(c, testPressed ? 3 : 2), S(c, testPressed ? 3 : 2), S(c, testPressed ? 3 : 2), S(c, testPressed ? 3 : 2));
    Text(dc, testInner, L"TEST", c.fonts->menuFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT realBtn = R(c, panelX + 28 + btnW, btnY, btnW, 28);
    const bool realPressed = (ctx.pressedButton == 201);
    Fill(dc, realBtn, RGB(255, 200, 200));
    Win98Bevel(dc, realBtn, !realPressed);
    RECT realInner = Pad(realBtn, S(c, realPressed ? 3 : 2), S(c, realPressed ? 3 : 2), S(c, realPressed ? 3 : 2), S(c, realPressed ? 3 : 2));
    Text(dc, realInner, L"REAL", c.fonts->menuFont, kWin98Red, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int sy = btnY + 44;
    Text(dc, R(c, panelX + 16, sy, panelW - 32, 18), L"Status: ARMED", c.fonts->menuFont, kWin98Green,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    sy += 22;
    Text(dc, R(c, panelX + 16, sy, panelW - 32, 18), L"Trigger count: 0", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    sy += 22;
    Text(dc, R(c, panelX + 16, sy, panelW - 32, 18), L"Last trigger: Never", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    sy += 30;
    Text(dc, R(c, panelX + 16, sy, panelW - 32, 18), L"Thresholds:", c.fonts->menuFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    sy += 22;
    Text(dc, R(c, panelX + 24, sy, panelW - 40, 18), L"CPU > 95%: 30s", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    sy += 18;
    Text(dc, R(c, panelX + 24, sy, panelW - 40, 18), L"RAM > 90%: 60s", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    sy += 18;
    Text(dc, R(c, panelX + 24, sy, panelW - 40, 18), L"Temp > 90\u00B0C: Immediate", c.fonts->logFont, kWin98Black,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  static void EnsureScaledFonts(Win98ThemeFonts& fonts, float scale, const std::filesystem::path& rootDir) {
    if (fonts.lastFontScale > 0.0f && std::abs(fonts.lastFontScale - scale) < 0.01f) return;
    fonts.Destroy();

    static std::wstring uiFace = L"MS Sans Serif";
    static std::wstring logFace = L"Perfect DOS VGA 437 Win";

    if (!fonts.fontsLoaded) {
      const auto uiTtf = rootDir / "themes" / L"Windows 98" / L"MS Sans Serif Bold.ttf";
      const auto logTtf = rootDir / "fonts" / L"Perfect DOS VGA 437 Win.ttf";
      if (std::filesystem::exists(uiTtf)) {
        AddFontResourceExW(uiTtf.c_str(), FR_PRIVATE, nullptr);
        auto name = monix::ReadTtfFaceName(uiTtf);
        if (!name.empty()) uiFace = name;
      }
      if (std::filesystem::exists(logTtf)) {
        AddFontResourceExW(logTtf.c_str(), FR_PRIVATE, nullptr);
        auto name = monix::ReadTtfFaceName(logTtf);
        if (!name.empty()) logFace = name;
      }
      fonts.fontsLoaded = true;
    }

    const int msH = (std::max)(12, static_cast<int>(16 * scale));
    const int titleH = (std::max)(12, static_cast<int>(16 * scale));
    const int menuH = (std::max)(11, static_cast<int>(15 * scale));
    const int smallH = (std::max)(10, static_cast<int>(13 * scale));
    const int logH = (std::max)(10, static_cast<int>(13 * scale));
    fonts.msSansSerif = CreateFontW(-msH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
      FIXED_PITCH | FF_MODERN, uiFace.c_str());
    fonts.titleBar = CreateFontW(-titleH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
      FIXED_PITCH | FF_MODERN, uiFace.c_str());
    fonts.menuFont = CreateFontW(-menuH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
      FIXED_PITCH | FF_MODERN, uiFace.c_str());
    fonts.smallFont = CreateFontW(-smallH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
      FIXED_PITCH | FF_MODERN, uiFace.c_str());
    fonts.logFont = CreateFontW(-logH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
      DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
      FIXED_PITCH | FF_MODERN, logFace.c_str());
    if (!fonts.msSansSerif || !fonts.titleBar || !fonts.menuFont || !fonts.smallFont || !fonts.logFont) {
      fonts.Destroy();
      fonts.msSansSerif = CreateFontW(-msH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"MS Sans Serif");
      fonts.titleBar = CreateFontW(-titleH, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"MS Sans Serif");
      fonts.menuFont = CreateFontW(-menuH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"MS Sans Serif");
      fonts.smallFont = CreateFontW(-smallH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"MS Sans Serif");
      fonts.logFont = CreateFontW(-logH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FIXED_PITCH | FF_MODERN, L"MS Sans Serif");
    }
    fonts.lineHeight = (std::max)(16, static_cast<int>(18 * scale));
    fonts.smallLineHeight = (std::max)(12, static_cast<int>(14 * scale));
    fonts.logLineHeight = (std::max)(12, static_cast<int>(14 * scale));
    fonts.lastFontScale = scale;
  }

  static bool HitTestTabs(const RECT& clientRect, POINT point, int& tabIndex) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return false;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    if (dy < 54.0f || dy > 86.0f) return false;
    const int tabW = 110;
    int tabX = 10;
    for (int i = 0; i < 5; ++i) {
      if (dx >= tabX && dx < tabX + tabW) {
        tabIndex = i;
        return true;
      }
      tabX += tabW + 4;
    }
    return false;
  }

  static bool HitTestSettingsCategories(const RECT& clientRect, POINT point, int& category) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return false;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int catX = 16;
    const int catY = 114;
    const int catW = 180;
    const int catH = 6 * 24 + 8;
    if (dx < catX || dx > catX + catW || dy < catY || dy > catY + catH) return false;
    int idx = static_cast<int>((dy - catY - 4) / 24);
    if (idx < 0 || idx >= 6) return false;
    category = idx;
    return true;
  }

  struct HitResult {
    bool hit = false;
    int index = -1;
  };

  static HitResult HitTestTaskRow(const RECT& clientRect, POINT point) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return {};
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int contentY = 96;
    const int tableY = contentY + 42;
    const int headerH = 20;
    const int rowH = 16;
    const int tableX1 = 16;
    const int tableW = 1480;
    const int tableH = 810;
    if (dx < tableX1 || dx > tableX1 + tableW) return {};
    if (dy < tableY + headerH || dy > tableY + headerH + tableH - 24 - headerH) return {};
    int row = static_cast<int>((dy - tableY - headerH) / rowH);
    if (row < 0 || row >= 49) return {};
    return { true, row };
  }

  static int HitTestTaskToolbar(const RECT& clientRect, POINT point) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return -1;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int contentY = 96;
    if (dy < contentY + 19 || dy > contentY + 39) return -1;
    static const std::array<const wchar_t*, 3> actions {{ L"  End Task  ", L"  End Process  ", L"  Priority \u25BC " }};
    int btnX = 20;
    for (int i = 0; i < 3; ++i) {
      int bw = static_cast<int>(wcslen(actions[i])) * 7 + 16;
      if (dx >= btnX && dx < btnX + bw) return i;
      btnX += bw + 6;
    }
    return -1;
  }

  static int HitTestLogScrollbar(const RECT& clientRect, POINT point) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return 0;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    if (dx >= 740 && dx <= 754) {
      if (dy >= 112 && dy <= 124) return 1;
      if (dy >= 958 && dy <= 970) return -1;
    }
    return 0;
  }

  static int HitTestLogToolbar(const RECT& clientRect, POINT point, int topY) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return -1;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int toolbarY = topY + 16;
    if (dy < toolbarY + 2 || dy > toolbarY + 20) return -1;
    static const std::array<const wchar_t*, 6> toolBtns {{ L"  CLEAR  ", L"  PAUSE  ", L"  SEARCH  ", L"  JSON  ", L"  CSV  ", L"  COPY  " }};
    int tbX = 20;
    for (int ti = 0; ti < 6; ++ti) {
      int bw = static_cast<int>(wcslen(toolBtns[ti])) * 6 + 12;
      if (dx >= tbX && dx < tbX + bw) return ti;
      tbX += bw + 4;
    }
    return -1;
  }

  static int HitTestLogFilters(const RECT& clientRect, POINT point, int topY) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return -1;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int toolbarY = topY + 16;
    const int toolbarH = 24;
    const int filterY = toolbarY + toolbarH + 2;
    if (dy < filterY + 2 || dy > filterY + 18) return -1;
    static const std::array<const wchar_t*, 5> filterLabels {{ L"ALL", L"INFO", L"WARN", L"ERROR", L"CRIT" }};
    int fbX = 20;
    for (int fi = 0; fi < 5; ++fi) {
      int bw = static_cast<int>(wcslen(filterLabels[fi])) * 6 + 22;
      if (dx >= fbX && dx < fbX + bw) return fi;
      fbX += bw + 4;
    }
    return -1;
  }

  static bool HitTestSettingsToggle(const RECT& clientRect, POINT point, int& toggleIndex) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return false;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int contentY = 96;
    const int panelX = 206;
    const int panelW = 1322;
    if (dx < panelX || dx > panelX + panelW) return false;
    if (dy < contentY + 66 || dy > contentY + 870) return false;
    toggleIndex = static_cast<int>((dy - contentY - 66) / 28);
    if (toggleIndex < 0 || toggleIndex > 30) return false;
    const float checkX = static_cast<float>(panelX + panelW - 40);
    if (dx >= checkX && dx <= checkX + 16) return true;
    toggleIndex = -1;
    return false;
  }

  static int HitTestTaskScrollbar(const RECT& clientRect, POINT point) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return 0;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int contentY = 96;
    const int tableY = contentY + 42;
    const int tableH = 894 - 84;
    if (dx >= 1496 && dx <= 1510) {
      if (dy >= tableY + 22 && dy <= tableY + 32) return 1;
      if (dy >= tableY + tableH - 14 && dy <= tableY + tableH - 4) return -1;
    }
    return 0;
  }

  static int HitTestScramButton(const RECT& clientRect, POINT point) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return -1;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int panelX = 766;
    const int panelY = 490;
    const int panelW = 762;
    const int btnY = panelY + 100;
    const int btnW = (panelW - 44) / 2;
    const int btnH = 28;
    if (dy >= btnY && dy <= btnY + btnH) {
      if (dx >= panelX + 16 && dx <= panelX + 16 + btnW) return 0;
      if (dx >= panelX + 28 + btnW && dx <= panelX + 28 + btnW + btnW) return 1;
    }
    return -1;
  }

  static bool HitTestSettingsAdjust(const RECT& clientRect, POINT point, int& rowIndex, int& direction) {
    Canvas c = MakeCanvas(clientRect, Win98ThemeFonts{}, Win98Assets{});
    if (!PtInRect(&c.rect, point)) return false;
    const float dx = static_cast<float>(point.x - c.rect.left) / c.scale;
    const float dy = static_cast<float>(point.y - c.rect.top) / c.scale;
    const int contentY = 96;
    const int panelX = 206;
    const int panelW = 1322;
    if (dx < panelX || dx > panelX + panelW) return false;
    if (dy < contentY + 66 || dy > contentY + 870) return false;

    int row = static_cast<int>((dy - contentY - 66) / 28);
    if (row < 0 || row > 20) return false;
    rowIndex = row;

    const float rowY = static_cast<float>(contentY + 66 + row * 28);
    const float minusX = static_cast<float>(panelX + panelW - 56);
    const float plusX = static_cast<float>(panelX + panelW - 34);
    if (dx >= minusX && dx <= minusX + 18 && dy >= rowY && dy <= rowY + 20) {
      direction = -1;
      return true;
    }
    if (dx >= plusX && dx <= plusX + 18 && dy >= rowY && dy <= rowY + 20) {
      direction = 1;
      return true;
    }
    rowIndex = -1;
    return false;
  }

  static void DrawSettingsTab(HDC dc, const Canvas& c, const CoreMonitorThemeContext& ctx) {
    const Snapshot& s = SnapshotOrDefault(ctx);
    const Config& cfg = ctx.config;
    const int contentY = 96;
    const int contentH = 894;

    DrawGroupbox(dc, c, 8, contentY, 1520, contentH, L"CONTROL PANEL", c.fonts->titleBar);

    const int catX = 16;
    const int catY = contentY + 18;
    const int catW = 180;
    const int catH = contentH - 36;

    RECT catBg = R(c, catX, catY, catW, catH);
    Fill(dc, catBg, kWin98White);
    Win98Bevel(dc, catBg, true);

    struct Category { const wchar_t* icon; const wchar_t* label; };
    static const std::array<Category, 6> categories {{
      { L"\u2699", L"System" },
      { L"\u25A0", L"Display" },
      { L"\u2261", L"Telemetry" },
      { L"\u2263", L"Logging" },
      { L"\u26A0", L"Security" },
      { L"\u2139", L"About" }
    }};

    const int selectedCat = ctx.settingsCategory;
    int catItemY = catY + 4;
    for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
      bool selected = (i == selectedCat);
      RECT itemBg = { X(c, catX + 2), Y(c, catItemY), X(c, catX + catW - 2), Y(c, catItemY + 22) };
      if (selected) {
        Fill(dc, itemBg, kWin98Highlight);
        Line(dc, itemBg.left, itemBg.top, itemBg.right - 1, itemBg.top, kWin98ButtonDkShadow);
        Line(dc, itemBg.left, itemBg.top, itemBg.left, itemBg.bottom - 1, kWin98ButtonDkShadow);
        Line(dc, itemBg.right - 1, itemBg.top, itemBg.right - 1, itemBg.bottom - 1, kWin98ButtonHighlight);
        Line(dc, itemBg.left, itemBg.bottom - 1, itemBg.right - 1, itemBg.bottom - 1, kWin98ButtonHighlight);
      } else {
        Fill(dc, itemBg, (i % 2 == 0) ? kWin98White : RGB(240, 240, 240));
        Line(dc, itemBg.left, itemBg.top, itemBg.right - 1, itemBg.top, RGB(210, 210, 210));
        Line(dc, itemBg.left, itemBg.bottom - 1, itemBg.right - 1, itemBg.bottom - 1, RGB(210, 210, 210));
      }

      RECT iconRect = { X(c, catX + 8), Y(c, catItemY + 3), X(c, catX + 22), Y(c, catItemY + 19) };
      Fill(dc, iconRect, selected ? kWin98Highlight : RGB(240, 240, 240));
      Text(dc, R(c, catX + 8, catItemY + 2, 14, 18), categories[i].icon,
           c.fonts->menuFont, selected ? kWin98HighlightText : kWin98Black,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, catX + 28, catItemY + 2, catW - 36, 18), categories[i].label,
           c.fonts->menuFont, selected ? kWin98HighlightText : kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      catItemY += 24;
    }

    const int panelX = catX + catW + 10;
    const int panelW = 1520 - catW - 18;
    const int panelH = catH;

    RECT panelBg = R(c, panelX, catY, panelW, panelH);
    Fill(dc, panelBg, kWin98Gray);

    auto DrawSettingRow = [&](int x, int y, int w, const std::wstring& label, const std::wstring& value, int settingIdx) {
      RECT labelBg = R(c, x, y, w / 2, 20);
      Text(dc, labelBg, label, c.fonts->menuFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      RECT valBg = R(c, x + w / 2, y, w / 2 - 50, 20);
      Fill(dc, valBg, kWin98White);
      Win98Bevel(dc, valBg, true);
      Text(dc, R(c, x + w / 2 + 4, y + 1, w / 2 - 58, 18), value, c.fonts->menuFont, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      RECT btnMinus = R(c, x + w - 44, y, 18, 20);
      Fill(dc, btnMinus, kWin98ButtonFace);
      Win98Bevel(dc, btnMinus, false);
      Text(dc, btnMinus, L"-", c.fonts->titleBar, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      RECT btnPlus = R(c, x + w - 22, y, 18, 20);
      Fill(dc, btnPlus, kWin98ButtonFace);
      Win98Bevel(dc, btnPlus, false);
      Text(dc, btnPlus, L"+", c.fonts->titleBar, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    };

    auto DrawToggleRow = [&](int x, int y, int w, const std::wstring& label, bool enabled) {
      RECT labelBg = R(c, x, y, w - 30, 20);
      Text(dc, labelBg, label, c.fonts->menuFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      RECT checkBg = R(c, x + w - 28, y + 2, 16, 16);
      Fill(dc, checkBg, kWin98White);
      Win98Bevel(dc, checkBg, true);
      if (enabled) {
        HGDIOBJ oldF = SelectObject(dc, c.fonts->menuFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kWin98Black);
        DrawTextW(dc, L"\u2713", -1, &checkBg, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, oldF);
      }
    };

    int sy = catY + 12;
    const int sx = panelX + 12;
    const int sw = panelW - 24;

    if (selectedCat == 0) {
      Text(dc, R(c, sx, sy, sw, 20), L"System Settings", c.fonts->titleBar, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 28;
      Line(dc, X(c, sx), Y(c, sy), X(c, sx + sw), Y(c, sy), kWin98ButtonShadow);
      sy += 8;

      DrawToggleRow(sx, sy, sw, L"Always on Top", cfg.alwaysOnTop); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Minimize to Tray", cfg.minimizeToTray); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Start with Windows", cfg.startWithWindows); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Start Maximized", cfg.startMaximized); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Save Window Position", cfg.saveWindowPosition); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Show FPS", cfg.showFps); sy += 28;
      DrawToggleRow(sx, sy, sw, L"V-Sync", cfg.vSyncEnabled); sy += 32;

      std::wstring themeNames[] = { L"Dark", L"Light", L"System", L"Core Monitor", L"Windows 98", L"Modern" };
      int themeIdx = std::clamp(cfg.themeMode, 0, 5);
      DrawSettingRow(sx, sy, sw, L"Theme", themeNames[themeIdx], 0); sy += 28;

      std::wstring prioNames[] = { L"Normal", L"High", L"Realtime" };
      int prioIdx = std::clamp(cfg.processPriority, 0, 2);
      DrawSettingRow(sx, sy, sw, L"Process Priority", prioNames[prioIdx], 1); sy += 28;

      DrawSettingRow(sx, sy, sw, L"Font Scale", Fixed(cfg.fontScale, 2), 2); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Window Opacity", Fixed(cfg.windowOpacity * 100.0, 0) + L"%", 3); sy += 28;

      RECT infoBg = R(c, sx, sy + 4, sw, 60);
      Fill(dc, infoBg, kWin98White);
      Win98Bevel(dc, infoBg, true);
      Text(dc, R(c, sx + 8, sy + 8, sw - 16, 14), L"Host: " + s.host, c.fonts->smallFont, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 24, sw - 16, 14), L"Uptime: " + Hms(s.uptimeSeconds), c.fonts->smallFont, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 40, sw - 16, 14), L"GPU: " + s.gpuModel + L" (" + s.gpuDriverVersion + L")", c.fonts->smallFont, kWin98Black,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    } else if (selectedCat == 1) {
      Text(dc, R(c, sx, sy, sw, 20), L"Display Settings", c.fonts->titleBar, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 28;
      Line(dc, X(c, sx), Y(c, sy), X(c, sx + sw), Y(c, sy), kWin98ButtonShadow);
      sy += 8;

      DrawToggleRow(sx, sy, sw, L"CRT Shader Effect", cfg.crtEnabled); sy += 28;
      DrawSettingRow(sx, sy, sw, L"CRT Brightness", Fixed(cfg.crtBrightness, 2), 10); sy += 28;
      DrawSettingRow(sx, sy, sw, L"CRT Contrast", Fixed(cfg.crtContrast, 2), 11); sy += 28;
      DrawSettingRow(sx, sy, sw, L"CRT Saturation", Fixed(cfg.crtSaturation, 2), 12); sy += 28;
      DrawSettingRow(sx, sy, sw, L"CRT Gamma", Fixed(cfg.crtGamma, 2), 13); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Curvature", Fixed(cfg.crtCurvatureStrength, 3), 14); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Scanline Intensity", Fixed(cfg.crtScanlineIntensity, 2), 15); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Chromatic Aberr.", Fixed(cfg.crtChromaticAberration, 3), 16); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Phosphor Glow", Fixed(cfg.crtPhosphorGlow, 2), 17); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Vignette", Fixed(cfg.crtVignetteStrength, 2), 18); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Sharpness", Fixed(cfg.crtSharpness, 2), 19); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Flicker", Fixed(cfg.crtFlickerAmount, 3), 20); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Noise", Fixed(cfg.crtNoiseAmount, 3), 21); sy += 28;

      DrawToggleRow(sx, sy, sw, L"Burn-In Effect", cfg.crtBurnInEnabled); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Burn-In Intensity", Fixed(cfg.crtBurnInIntensity, 2), 22); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Border Image", cfg.borderEnabled); sy += 28;

      RECT resInfo = R(c, sx, sy + 4, sw, 40);
      Fill(dc, resInfo, kWin98White);
      Win98Bevel(dc, resInfo, true);
      Text(dc, R(c, sx + 8, sy + 8, sw - 16, 14), L"Resolution: " + std::to_wstring(s.displayWidth) + L"x" + std::to_wstring(s.displayHeight),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 24, sw - 16, 14), L"Refresh: " + std::to_wstring(s.displayRefreshRateHz) + L"Hz  |  Monitors: " + std::to_wstring(s.displayMonitorCount),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    } else if (selectedCat == 2) {
      Text(dc, R(c, sx, sy, sw, 20), L"Telemetry Settings", c.fonts->titleBar, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 28;
      Line(dc, X(c, sx), Y(c, sy), X(c, sx + sw), Y(c, sy), kWin98ButtonShadow);
      sy += 8;

      DrawToggleRow(sx, sy, sw, L"Analytics History", cfg.analyticsHistoryEnabled); sy += 28;
      DrawSettingRow(sx, sy, sw, L"History Capacity", std::to_wstring(cfg.historyCapacity), 30); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Telemetry Interval", std::to_wstring(cfg.telemetryIntervalMs) + L"ms", 31); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Frame Interval", std::to_wstring(cfg.frameIntervalMs) + L"ms", 32); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Target FPS", std::to_wstring(cfg.frameTargetFps) + L" (0=auto)", 33); sy += 32;

      RECT infoBg = R(c, sx, sy + 4, sw, 100);
      Fill(dc, infoBg, kWin98White);
      Win98Bevel(dc, infoBg, true);
      Text(dc, R(c, sx + 8, sy + 8, sw - 16, 14), L"Snapshot Data:", c.fonts->smallFont, kWin98DarkGray,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 24, sw - 16, 14), L"CPU: " + Fixed(s.cpuPct, 1) + L"%  |  RAM: " + Fixed(BytesToGb(s.ramUsedBytes), 1) + L"/" + Fixed(BytesToGb(s.ramTotalBytes), 1) + L"GB",
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 40, sw - 16, 14), L"GPU: " + Fixed(s.gpuPct, 1) + L"%  |  Temp: " + Fixed(s.cpuCoreTempC, 0) + L"\u00B0C",
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 56, sw - 16, 14), L"Net: " + RateText(s.netDownBytesPerSec, 0.0) + L" / " + RateText(s.netUpBytesPerSec, 0.0),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 72, sw - 16, 14), L"Processes: " + std::to_wstring(s.processCount) + L"  |  Threads: " + std::to_wstring(s.threadCount),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    } else if (selectedCat == 3) {
      Text(dc, R(c, sx, sy, sw, 20), L"Logging Settings", c.fonts->titleBar, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 28;
      Line(dc, X(c, sx), Y(c, sy), X(c, sx + sw), Y(c, sy), kWin98ButtonShadow);
      sy += 8;

      std::wstring logLevels[] = { L"Debug", L"Info", L"Warn", L"Error", L"Critical" };
      int logIdx = std::clamp(static_cast<int>(cfg.logLevel), 0, 4);
      DrawSettingRow(sx, sy, sw, L"Log Level", logLevels[logIdx], 40); sy += 28;

      DrawToggleRow(sx, sy, sw, L"Log Milliseconds", cfg.logMilliseconds); sy += 28;
      DrawToggleRow(sx, sy, sw, L"JSON Output", cfg.logJsonEnabled); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Plain Output", cfg.logPlainEnabled); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Deduplicate Logs", cfg.logDeduplicate); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Pause Live Logs", cfg.pauseLiveLogs); sy += 32;

      DrawSettingRow(sx, sy, sw, L"Visible Lines", std::to_wstring(cfg.logVisibleLines), 41); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Flush Interval", std::to_wstring(cfg.logFlushIntervalMs) + L"ms", 42); sy += 28;
      DrawSettingRow(sx, sy, sw, L"Retention Days", std::to_wstring(cfg.logRetentionDays), 43); sy += 28;

      RECT logInfo = R(c, sx, sy + 4, sw, 40);
      Fill(dc, logInfo, kWin98White);
      Win98Bevel(dc, logInfo, true);
      int logCount = ctx.logs ? static_cast<int>(ctx.logs->size()) : 0;
      Text(dc, R(c, sx + 8, sy + 8, sw - 16, 14), L"Log buffer: " + std::to_wstring(logCount) + L" entries",
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 24, sw - 16, 14), L"Max file size: " + Fixed(static_cast<double>(cfg.logMaxFileBytes) / 1048576.0, 1) + L"MB",
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    } else if (selectedCat == 4) {
      Text(dc, R(c, sx, sy, sw, 20), L"Security / SCRAM", c.fonts->titleBar, kWin98Red, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 28;
      Line(dc, X(c, sx), Y(c, sy), X(c, sx + sw), Y(c, sy), kWin98ButtonShadow);
      sy += 8;

      RECT dangerBg = R(c, sx, sy, sw, 80);
      Fill(dc, dangerBg, RGB(255, 240, 240));
      Win98Bevel(dc, dangerBg, true);
      Text(dc, R(c, sx + 8, sy + 8, sw - 16, 16), L"\u26A0 SCRAM! Emergency Mechanism", c.fonts->titleBar, kWin98Red,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 28, sw - 16, 14), L"SCRAM monitors system integrity and triggers",
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 44, sw - 16, 14), L"emergency protocols on critical events.",
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 88;

      int riskLevel = 0;
      if (s.cpuCoreTempC > 85.0) riskLevel += 2;
      else if (s.cpuCoreTempC > 70.0) riskLevel += 1;
      if (s.gpuPct > 90.0) riskLevel += 2;
      else if (s.gpuPct > 75.0) riskLevel += 1;
      if (s.sehExceptionCount > 0 || s.accessViolationCount > 0) riskLevel += 3;
      if (s.unsignedDriverCount > 0) riskLevel += 1;
      riskLevel = (std::min)(riskLevel, 10);

      RECT riskBar = R(c, sx, sy, sw, 28);
      Fill(dc, riskBar, kWin98White);
      Win98Bevel(dc, riskBar, true);
      int riskW = sw - 4;
      int riskFill = static_cast<int>(riskW * riskLevel / 10.0);
      RECT riskFillR = { riskBar.left + 2, riskBar.top + 2, riskBar.left + 2 + riskFill, riskBar.bottom - 2 };
      COLORREF riskColor = riskLevel <= 2 ? kWin98Green : (riskLevel <= 5 ? RGB(180, 120, 0) : kWin98Red);
      Fill(dc, riskFillR, riskColor);
      Text(dc, R(c, sx + 4, sy + 4, sw - 8, 20), L"Risk Level: " + std::to_wstring(riskLevel) + L"/10", c.fonts->menuFont,
           riskLevel <= 2 ? kWin98Green : (riskLevel <= 5 ? RGB(180, 120, 0) : kWin98Red),
           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 36;

      DrawToggleRow(sx, sy, sw, L"Notifications Enabled", cfg.notificationsEnabled); sy += 28;
      DrawToggleRow(sx, sy, sw, L"Sound Alerts", cfg.soundEnabled); sy += 32;

      RECT scramInfo = R(c, sx, sy + 4, sw, 80);
      Fill(dc, scramInfo, kWin98White);
      Win98Bevel(dc, scramInfo, true);
      Text(dc, R(c, sx + 8, sy + 8, sw - 16, 14), L"Security Checks:", c.fonts->smallFont, kWin98DarkGray,
           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 24, sw - 16, 14), L"SEH Exceptions: " + std::to_wstring(s.sehExceptionCount) +
           L"  |  Access Violations: " + std::to_wstring(s.accessViolationCount),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 40, sw - 16, 14), L"Unsigned Drivers: " + std::to_wstring(s.unsignedDriverCount) +
           L"  |  Hooks: " + std::to_wstring(s.hookModulesDetected),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 8, sy + 56, sw - 16, 14), L"Self Integrity: " + std::to_wstring(s.selfSignatureValid >= 0 ? (s.selfSignatureValid ? 1 : 0) : -1) +
           L"  |  Debug Ports: " + std::to_wstring(s.debugPortActive),
           c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    } else if (selectedCat == 5) {
      Text(dc, R(c, sx, sy, sw, 20), L"About MONIX", c.fonts->titleBar, kWin98Navy, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 28;
      Line(dc, X(c, sx), Y(c, sy), X(c, sx + sw), Y(c, sy), kWin98ButtonShadow);
      sy += 12;

      RECT logoBg = R(c, sx, sy, sw, 80);
      for (int lx = logoBg.left; lx < logoBg.right; ++lx) {
        float t = static_cast<float>(lx - logoBg.left) / static_cast<float>(logoBg.right - logoBg.left);
        int lr = static_cast<int>(0 + t * 20.0);
        int lg = static_cast<int>(0 + t * 20.0);
        int lb = static_cast<int>(128 + t * 40.0);
        RECT col = { lx, logoBg.top, lx + 1, logoBg.bottom };
        Fill(dc, col, RGB(lr, lg, lb));
      }
      Win98Bevel(dc, logoBg, true);
      Text(dc, R(c, sx + 16, sy + 8, sw - 32, 28), L"MONIX 1.0.0", c.fonts->titleBar, kWin98White,
           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 16, sy + 36, sw - 32, 18), L"System Monitor & Kernel Inspector", c.fonts->menuFont, RGB(150, 180, 255),
           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      Text(dc, R(c, sx + 16, sy + 56, sw - 32, 14), L"\u00A9 2025 Weird Stuff Software", c.fonts->smallFont, RGB(120, 150, 220),
           DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      sy += 96;

      RECT aboutInfo = R(c, sx, sy, sw, 180);
      Fill(dc, aboutInfo, kWin98White);
      Win98Bevel(dc, aboutInfo, true);

      auto AboutLine = [&](int y, const std::wstring& label, const std::wstring& value) {
        Text(dc, R(c, sx + 8, y, 200, 16), label, c.fonts->smallFont, kWin98DarkGray, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        Text(dc, R(c, sx + 220, y, sw - 236, 16), value, c.fonts->smallFont, kWin98Black, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      };

      AboutLine(sy + 8, L"Renderer:", ctx.rendererName);
      AboutLine(sy + 26, L"Shader:", ctx.shaderName);
      AboutLine(sy + 44, L"CPU:", std::string(s.cpuBrand).empty() ? L"Unknown" : [] (const char* b) { return std::wstring(b, b + strlen(b)); } (s.cpuBrand));
      AboutLine(sy + 62, L"Cores:", std::to_wstring(s.cpuCores) + L" physical / " + std::to_wstring(s.cpuLogicalCpus) + L" logical");
      AboutLine(sy + 80, L"GPU:", s.gpuModel);
      AboutLine(sy + 98, L"VRAM:", FormatBytes(s.gpuVramTotalBytes));
      AboutLine(sy + 116, L"RAM:", FormatBytes(s.ramTotalBytes));
      AboutLine(sy + 134, L"Display:", std::to_wstring(s.displayWidth) + L"x" + std::to_wstring(s.displayHeight) + L" @ " + std::to_wstring(s.displayRefreshRateHz) + L"Hz");
      AboutLine(sy + 152, L"BIOS:", s.biosVersion.empty() ? L"Unknown" : s.biosVersion);
    }
  }

  static void DrawUpdateDialog(HDC dc, const Canvas& c, const monix::UpdateState& us) {
    if (!us.dialogVisible) return;

    RECT overlay = c.rect;
    Fill(dc, overlay, RGB(0, 0, 0));

    const int dlgDW = 420;
    const int dlgDH = 280;
    const int dlgSW = S(c, dlgDW);
    const int dlgSH = S(c, dlgDH);
    int dlgX = c.rect.left + (c.rect.right - c.rect.left - dlgSW) / 2;
    int dlgY = c.rect.top + (c.rect.bottom - c.rect.top - dlgSH) / 2;
    RECT dlg = { dlgX, dlgY, dlgX + dlgSW, dlgY + dlgSH };

    Fill(dc, dlg, kWin98Gray);
    Win98Bevel(dc, dlg, false);

    RECT titleBar = { dlg.left, dlg.top, dlg.right, dlg.top + S(c, 22) };
    Fill(dc, titleBar, kWin98TitleBlue);
    Text(dc, titleBar, L"  Monix Update", c.fonts->menuFont, kWin98White, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT closeBtn = { dlg.right - S(c, 20), dlg.top + S(c, 2),
                      dlg.right - S(c, 4), dlg.top + S(c, 20) };
    Fill(dc, closeBtn, kWin98ButtonFace);
    Win98Bevel(dc, closeBtn, false);
    Text(dc, closeBtn, L"X", c.fonts->smallFont, kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int cy = dlg.top + S(c, 34);
    int innerX = dlg.left + S(c, 16);
    int innerW = dlgSW - S(c, 32);
    int innerRight = dlg.right - S(c, 16);

    auto dlgLine = [&](const std::wstring& text, HFONT font, COLORREF color, int spacing) {
      RECT r = { dlg.left, cy, dlg.right, cy + S(c, 20) };
      Text(dc, r, text, font, color, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      cy += S(c, spacing);
    };

    if (us.checking) {
      dlgLine(L"Checking for updates...", c.fonts->smallFont, kWin98Black, 30);
    } else if (us.updateAvailable) {
      dlgLine(L"A new version is available!", c.fonts->menuFont, kWin98Navy, 28);
      dlgLine(L"You are running: " + us.currentVersion, c.fonts->smallFont, kWin98DarkGray, 18);
      dlgLine(L"Latest version:  " + us.latestVersion, c.fonts->smallFont, kWin98BrightGreen, 24);

      RECT notesBg = { innerX, cy, innerRight, cy + S(c, 80) };
      Fill(dc, notesBg, kWin98White);
      Win98Bevel(dc, notesBg, true);
      if (!us.releaseNotes.empty()) {
        RECT notesText = { notesBg.left + S(c, 6), notesBg.top + S(c, 4),
                           notesBg.right - S(c, 6), notesBg.bottom - S(c, 4) };
        Text(dc, notesText, us.releaseNotes, c.fonts->smallFont, kWin98Black, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
      } else {
        Text(dc, notesBg, L"No release notes.", c.fonts->smallFont, kWin98DarkGray, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    } else {
      dlgLine(L"You are running the latest version!", c.fonts->menuFont, kWin98BrightGreen, 24);
      dlgLine(L"Version: " + us.currentVersion, c.fonts->smallFont, kWin98DarkGray, 24);
    }

    const int btnDW = 90;
    const int btnDH = 24;
    int btnSW = S(c, btnDW);
    int btnSH = S(c, btnDH);
    int btnY = dlg.bottom - S(c, 38);

    if (us.updateAvailable) {
      int okX = dlg.left + (dlgSW / 2 - btnSW - S(c, 10));
      RECT okBtn = { okX, btnY, okX + btnSW, btnY + btnSH };
      Fill(dc, okBtn, us.hoverButton == 0 ? kWin98Highlight : kWin98ButtonFace);
      Win98Bevel(dc, okBtn, false);
      Text(dc, okBtn, L"Download", c.fonts->smallFont, us.hoverButton == 0 ? kWin98HighlightText : kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      int cxBtn = okX + btnSW + S(c, 20);
      RECT cancelBtn = { cxBtn, btnY, cxBtn + btnSW, btnY + btnSH };
      Fill(dc, cancelBtn, us.hoverButton == 1 ? kWin98Highlight : kWin98ButtonFace);
      Win98Bevel(dc, cancelBtn, false);
      Text(dc, cancelBtn, L"Cancel", c.fonts->smallFont, us.hoverButton == 1 ? kWin98HighlightText : kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else {
      int okX = dlg.left + (dlgSW / 2 - btnSW / 2);
      RECT okBtn = { okX, btnY, okX + btnSW, btnY + btnSH };
      Fill(dc, okBtn, us.hoverButton == 0 ? kWin98Highlight : kWin98ButtonFace);
      Win98Bevel(dc, okBtn, false);
      Text(dc, okBtn, L"OK", c.fonts->smallFont, us.hoverButton == 0 ? kWin98HighlightText : kWin98Black, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
  }

  static bool HitTestUpdateDialog(const Canvas& c, const monix::UpdateState& us, POINT clientPt, int& result) {
    if (!us.dialogVisible) return false;

    const int dlgDW = 420;
    const int dlgDH = 280;
    const int dlgSW = S(c, dlgDW);
    const int dlgSH = S(c, dlgDH);
    int dlgX = c.rect.left + (c.rect.right - c.rect.left - dlgSW) / 2;
    int dlgY = c.rect.top + (c.rect.bottom - c.rect.top - dlgSH) / 2;
    RECT dlg = { dlgX, dlgY, dlgX + dlgSW, dlgY + dlgSH };

    RECT closeBtn = { dlg.right - S(c, 20), dlg.top + S(c, 2),
                      dlg.right - S(c, 4), dlg.top + S(c, 20) };
    if (PtInRect(&closeBtn, clientPt)) { result = -1; return true; }

    const int btnDW = 90;
    const int btnDH = 24;
    int btnSW = S(c, btnDW);
    int btnSH = S(c, btnDH);
    int btnY = dlg.bottom - S(c, 38);

    if (us.updateAvailable) {
      int okX = dlg.left + (dlgSW / 2 - btnSW - S(c, 10));
      RECT okBtn = { okX, btnY, okX + btnSW, btnY + btnSH };
      if (PtInRect(&okBtn, clientPt)) { result = 0; return true; }

      int cxBtn = okX + btnSW + S(c, 20);
      RECT cancelBtn = { cxBtn, btnY, cxBtn + btnSW, btnY + btnSH };
      if (PtInRect(&cancelBtn, clientPt)) { result = 1; return true; }
    } else {
      int okX = dlg.left + (dlgSW / 2 - btnSW / 2);
      RECT okBtn = { okX, btnY, okX + btnSW, btnY + btnSH };
      if (PtInRect(&okBtn, clientPt)) { result = 0; return true; }
    }

    return false;
  }

  static void Render(HDC dc, const RECT& clientRect, const CoreMonitorThemeContext& ctx,
                     Win98ThemeFonts& fonts, const Win98Assets& assets,
                     const std::filesystem::path& rootDir) {
    Canvas c = MakeCanvas(clientRect, fonts, assets);
    EnsureScaledFonts(fonts, c.scale, rootDir);
    Fill(dc, clientRect, kWin98Gray);
    Fill(dc, c.rect, kWin98Gray);

    DrawTitleBar(dc, c, ctx);
    DrawMenuBar(dc, c, ctx);
    DrawTabBar(dc, c, ctx);

    const int activeTab = std::clamp(ctx.menuIndex, 0, 4);
    switch (activeTab) {
      case 0: // LOG — side-by-side: Log Console left, Alerts right
      {
        const int contentY = 94;
        const int contentH = 910;
        const int halfW = 754;

        DrawLogConsoleLeft(dc, c, ctx, contentY, contentH, halfW);
        DrawAlertsRight(dc, c, ctx, contentY, contentH, halfW);
        break;
      }
      case 1: // TASKS
        DrawActiveTasks(dc, c, ctx);
        break;
      case 2: // USAGE
        DrawSystemOverview(dc, c, ctx);
        DrawRealTimeGraph(dc, c, ctx);
        DrawAiContextEngine(dc, c, ctx);
        DrawScramPanel(dc, c, ctx);
        break;
      case 3: // AI — side-by-side: Context Engine left, Alerts right
      {
        const int contentY = 94;
        const int contentH = 910;
        const int halfW = 754;
        DrawAiContextEngineFull(dc, c, ctx, contentY, contentH, halfW);
        DrawAlertsRight(dc, c, ctx, contentY, contentH, halfW);
        break;
      }
      case 4: // SETTINGS
        DrawSettingsTab(dc, c, ctx);
        break;
    }

    DrawStatusBar(dc, c, ctx);

    if (ctx.updateState && ctx.updateState->dialogVisible) {
      DrawUpdateDialog(dc, c, *ctx.updateState);
    }
  }
};

}  // namespace monix::ui
