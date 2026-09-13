#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "BootUp.hpp"

namespace Monix {
namespace Boot {

class BootUpDisplay {
public:
  BootUpDisplay() = default;

  void Draw(HDC dc, const RECT& clientRect, const BootUp& boot,
            HFONT bodyFont, HFONT smallFont, int dpiScale) const;

private:
  HFONT LoadBootFont(HDC dc, int dpiScale, int* outHeight, int* outWidth) const;
  static void DrawTextAt(HDC dc, int x, int y, const wchar_t* text, COLORREF color, HFONT font);
  static int Sc(int base, int dpiScale);
};

} // namespace Boot
} // namespace Monix