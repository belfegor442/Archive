#include "BootUpDisplay.hpp"

#include <algorithm>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

extern "C" void InitGdiPlus();
extern "C" void ShutdownGdiPlus();
extern "C" int DrawPngToDc(HDC dc, const wchar_t* path, int x, int y, int maxW);

namespace Monix {
namespace Boot {

static constexpr COLORREF COL_BG   = RGB(0, 0, 0);
static constexpr COLORREF COL_TEXT = RGB(192, 192, 192);
static constexpr COLORREF COL_DIM  = RGB(128, 128, 128);
static constexpr COLORREF COL_WHT  = RGB(255, 255, 255);

int BootUpDisplay::Sc(int base, int dpiScale) {
  return (std::max<int>)(1, static_cast<int>(base * (dpiScale / 96.0)));
}

void BootUpDisplay::DrawTextAt(HDC dc, int x, int y, const wchar_t* text, COLORREF color, HFONT font) {
  SetTextColor(dc, color);
  SetBkColor(dc, COL_BG);
  SetBkMode(dc, OPAQUE);
  SelectObject(dc, font);
  SetTextAlign(dc, TA_LEFT | TA_TOP);
  ExtTextOutW(dc, x, y, ETO_OPAQUE, nullptr, text, static_cast<int>(wcslen(text)), nullptr);
}

static int DrawLogo(HDC dc, int x, int y, int maxW) {
  static bool resolved = false;
  static bool available = false;
  static wchar_t logoPath[MAX_PATH] = {};

  if (!resolved) {
    resolved = true;
    InitGdiPlus();
    GetModuleFileNameW(nullptr, logoPath, MAX_PATH);
    wchar_t* sl = wcsrchr(logoPath, L'\\');
    if (sl) {
      const wchar_t* candidates[] = {
        L"..\\login\\ami.png",
        L"login\\ami.png",
        L"ami.png"
      };
      for (const auto* rel : candidates) {
        wcscpy_s(sl + 1, MAX_PATH - (sl - logoPath + 1), rel);
        if (GetFileAttributesW(logoPath) != INVALID_FILE_ATTRIBUTES) {
          available = true;
          break;
        }
      }
    }
  }

  if (!available) return 0;
  return DrawPngToDc(dc, logoPath, x, y, maxW);
}

HFONT BootUpDisplay::LoadBootFont(HDC dc, int dpiScale, int* outHeight, int* outWidth) const {
  static HFONT cachedFont = nullptr;
  static int cachedDpi = 0;
  static int cachedHeight = 0;
  static int cachedWidth = 0;

  if (cachedFont && cachedDpi == dpiScale) {
    if (outHeight) *outHeight = cachedHeight;
    if (outWidth) *outWidth = cachedWidth;
    return cachedFont;
  }
  if (cachedFont) { DeleteObject(cachedFont); cachedFont = nullptr; }

  wchar_t fontPath[MAX_PATH];
  GetModuleFileNameW(nullptr, fontPath, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(fontPath, L'\\');
  if (lastSlash) {
    wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - fontPath + 1), L"fonts\\Px437_AMI_EGA_8x8.ttf");
    if (GetFileAttributesW(fontPath) != INVALID_FILE_ATTRIBUTES) {
      AddFontResourceExW(fontPath, FR_PRIVATE, 0);
      int fontSize = Sc(8, dpiScale);
      HFONT font = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Px437 AMI EGA 8x8");
      if (font) {
        TEXTMETRICW tm{};
        HFONT old = static_cast<HFONT>(SelectObject(dc, font));
        GetTextMetricsW(dc, &tm);
        SelectObject(dc, old);
        cachedFont = font;
        cachedDpi = dpiScale;
        cachedHeight = tm.tmHeight + tm.tmExternalLeading;
        cachedWidth = tm.tmAveCharWidth;
        if (outHeight) *outHeight = cachedHeight;
        if (outWidth) *outWidth = cachedWidth;
        return font;
      }
    }
  }

  int fontSize = Sc(8, dpiScale);
  HFONT font = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS,
    NONANTIALIASED_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Terminal");
  if (font && outHeight) {
    TEXTMETRICW tm{};
    HFONT old = static_cast<HFONT>(SelectObject(dc, font));
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, old);
    cachedHeight = tm.tmHeight + tm.tmExternalLeading;
    cachedWidth = tm.tmAveCharWidth;
    *outHeight = cachedHeight;
    if (outWidth) *outWidth = cachedWidth;
  }
  cachedFont = font;
  cachedDpi = dpiScale;
  cachedHeight = cachedHeight ? cachedHeight : (fontSize + 4);
  return font;
}
void BootUpDisplay::Draw(HDC dc, const RECT& clientRect, const BootUp& boot,
                         HFONT bodyFont, HFONT smallFont, int dpiScale) const {
  HBRUSH br = CreateSolidBrush(COL_BG);
  FillRect(dc, &clientRect, br);
  DeleteObject(br);

  int fontHeight = 0;
  int fontWidth = 0;
  HFONT mono = LoadBootFont(dc, dpiScale, &fontHeight, &fontWidth);
  if (!mono) mono = bodyFont;
  const int lh = fontHeight > 0 ? fontHeight : Sc(8, dpiScale);
  const int cw = fontWidth > 0 ? fontWidth : Sc(8, dpiScale);
  const int margin = Sc(16, dpiScale);
  const int startY = Sc(16, dpiScale);
  const int bx = margin;
  int phase = boot.GetPhase();
  int memKb = boot.GetMemoryKb();

  auto text = [&](int col, int row, const wchar_t* t, COLORREF c) {
    DrawTextAt(dc, bx + col * cw, startY + row * lh, t, c, mono);
  };

  int logoH = DrawLogo(dc, bx, startY, Sc(400, dpiScale));
  int row = 2;
  if (logoH > lh) {
    row = 2 + (logoH / lh);
  }

  using P = BootUp::Phase;
  auto p = static_cast<P>(phase);

  if (p >= P::Header) {
    text(0, row, L"AMIBIOS (C)2007 American Megatrends, Inc.", COL_TEXT);
    row++;
    text(0, row, L"ASUS P5KPL ACPI BIOS Revision 0603", COL_TEXT);
    row++;
    text(0, row, L"CPU : Intel(R) Pentium(R) Dual CPU E2180 @ 2.00GHz", COL_TEXT);
    row++;
    text(0, row, L"Speed : 2.51 GHz    Count : 2", COL_DIM);
    row++;
  }

  if (p >= P::PostInfo) {
    text(0, row, L"Press DEL to run Setup", COL_TEXT);
    row++;
    text(0, row, L"Press F8 for BBS POPUP", COL_TEXT);
    row++;
    text(0, row, L"DDR2-667 in Dual-Channel Interleaved Mode", COL_DIM);
    row++;
    text(0, row, L"Initializing USB Controllers .. Done.", COL_DIM);
    row++;
  }

  if (p >= P::MemoryCount && p <= P::MemoryDone) {
    int mb = memKb / 1024;
    wchar_t memLine[64];
    if (p == P::MemoryDone) {
      _snwprintf_s(memLine, _countof(memLine), _TRUNCATE, L"%dMB OK", mb);
    } else {
      _snwprintf_s(memLine, _countof(memLine), _TRUNCATE, L"%dMB", mb);
    }
    text(0, row, memLine, COL_TEXT);
    row++;
  }

  if (p >= P::Devices) {
    text(0, row, L"USB Device(s) : 1 Keyboard, 1 Mouse", COL_DIM);
    row++;
    text(0, row, L"Auto-Detecting SATA1 DEVICE.. IDE Hard Disk:ST3500641AS", COL_DIM);
    row++;
    text(0, row, L"Auto-detecting USB Mass Storage Devices ..", COL_DIM);
    row++;
    text(0, row, L"00 USB mass storage devices found and configured.", COL_DIM);
    row++;
  }

  if (p >= P::Copyright) {
    row++;
    text(0, row, L"(C) American Megatrends, Inc.", COL_DIM);
    row++;
    text(0, row, L"64-0603-000001-00101111-022908-Bearlake-A0820000-Y2KC", COL_DIM);
  }
}

} // namespace Boot
} // namespace Monix