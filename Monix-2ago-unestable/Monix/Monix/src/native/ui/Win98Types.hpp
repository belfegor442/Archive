#pragma once

#include <windows.h>

namespace Gdiplus { class Bitmap; }

namespace monix::ui {

struct Win98Assets {
  Gdiplus::Bitmap* windowBase = nullptr;
  Gdiplus::Bitmap* windowHeader = nullptr;
  Gdiplus::Bitmap* windowHeaderInactive = nullptr;
  Gdiplus::Bitmap* windowHeaderResizable = nullptr;
  Gdiplus::Bitmap* windowHeaderResizableInactive = nullptr;
  Gdiplus::Bitmap* buttonNormal = nullptr;
  Gdiplus::Bitmap* buttonFocus = nullptr;
  Gdiplus::Bitmap* buttonFocusOutlined = nullptr;
  Gdiplus::Bitmap* buttonInactive = nullptr;
  Gdiplus::Bitmap* buttonPressed = nullptr;
  Gdiplus::Bitmap* buttonPressedOutlined = nullptr;
  Gdiplus::Bitmap* dividerLine = nullptr;
  Gdiplus::Bitmap* icons = nullptr;
  Gdiplus::Bitmap* innerFrame = nullptr;
  Gdiplus::Bitmap* innerFrameInverted = nullptr;
  Gdiplus::Bitmap* progressFill = nullptr;
  Gdiplus::Bitmap* ratio = nullptr;
  Gdiplus::Bitmap* ratioInactive = nullptr;
  Gdiplus::Bitmap* ratioSelected = nullptr;
  Gdiplus::Bitmap* sidebarUnderside = nullptr;
  Gdiplus::Bitmap* sliderBackground = nullptr;
  Gdiplus::Bitmap* sliderHandle = nullptr;
  Gdiplus::Bitmap* toggleActive = nullptr;
  Gdiplus::Bitmap* toggleInactive = nullptr;
  Gdiplus::Bitmap* toggleSelected = nullptr;

  void Clear() {
    auto del = [](Gdiplus::Bitmap*& p) { delete p; p = nullptr; };
    del(windowBase); del(windowHeader); del(windowHeaderInactive);
    del(windowHeaderResizable); del(windowHeaderResizableInactive);
    del(buttonNormal); del(buttonFocus); del(buttonFocusOutlined);
    del(buttonInactive); del(buttonPressed); del(buttonPressedOutlined);
    del(dividerLine); del(icons); del(innerFrame); del(innerFrameInverted);
    del(progressFill); del(ratio); del(ratioInactive); del(ratioSelected);
    del(sidebarUnderside); del(sliderBackground); del(sliderHandle);
    del(toggleActive); del(toggleInactive); del(toggleSelected);
  }
};

struct Win98ThemeFonts {
  HFONT msSansSerif = nullptr;
  HFONT titleBar = nullptr;
  HFONT menuFont = nullptr;
  HFONT smallFont = nullptr;
  HFONT logFont = nullptr;
  int lineHeight = 18;
  int smallLineHeight = 14;
  int logLineHeight = 14;
  float lastFontScale = 0.0f;
  bool fontsLoaded = false;

  void Destroy() {
    auto del = [](HFONT& f) { if (f) { DeleteObject(f); f = nullptr; } };
    del(msSansSerif); del(titleBar); del(menuFont); del(smallFont); del(logFont);
  }
};

}
