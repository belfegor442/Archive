#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "MonixKernel.hpp"

namespace Monix {
namespace Kernel {

class KernelDisplay {
public:
  KernelDisplay() = default;

  void Draw(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
            HFONT bodyFont, HFONT smallFont, int dpiScale) const;

private:
  void DrawDiagnostics(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                       HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;
  void DrawReady(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                 HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;
  void DrawAuth(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;
  void DrawAuthSuccess(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                       HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;
  void DrawLoading(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                   HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;
  void DrawCrash(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                 HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;
  void DrawShutdown(HDC dc, const RECT& clientRect, const MonixKernel& kernel,
                    HFONT mono, int lh, int cw, int cx, int W, int H, int dpiScale) const;

  HFONT LoadKernelFont(HDC dc, int dpiScale, int* outHeight) const;
  static void DrawTextAt(HDC dc, int x, int y, const wchar_t* text, COLORREF color, HFONT font);
  static void DrawTextCentered(HDC dc, int cx, int y, const wchar_t* text, COLORREF color, HFONT font);
  static int Sc(int base, int dpiScale);

  mutable int scrollOffset_ = 0;
};

} // namespace Kernel
} // namespace Monix
