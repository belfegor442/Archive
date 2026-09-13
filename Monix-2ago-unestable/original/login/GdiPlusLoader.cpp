#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static Gdiplus::GdiplusStartupInput gdiplusStartupInput;
static ULONG_PTR gdiplusToken = 0;

extern "C" __declspec(dllexport) void InitGdiPlus() {
  if (!gdiplusToken) Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
}

extern "C" __declspec(dllexport) void ShutdownGdiPlus() {
  if (gdiplusToken) { Gdiplus::GdiplusShutdown(gdiplusToken); gdiplusToken = 0; }
}

extern "C" __declspec(dllexport) int DrawPngToDc(HDC dc, const wchar_t* path, int x, int y, int maxW) {
  if (!gdiplusToken) return 0;
  Gdiplus::Image img(path);
  if (img.GetLastStatus() != Gdiplus::Ok) return 0;
  int w = static_cast<int>(img.GetWidth());
  int h = static_cast<int>(img.GetHeight());
  int drawW = w;
  int drawH = h;
  if (maxW > 0 && w > maxW) {
    drawW = maxW;
    drawH = static_cast<int>((double)h / w * maxW);
  }
  Gdiplus::Graphics g(dc);
  g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
  g.DrawImage(&img, x, y, drawW, drawH);
  return drawH;
}