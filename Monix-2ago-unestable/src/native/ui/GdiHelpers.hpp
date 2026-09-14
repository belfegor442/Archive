#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <string>

namespace monix::ui::gdi {

inline void Fill(HDC dc, const RECT& rect, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

inline void Outline(HDC dc, const RECT& rect, COLORREF color, int width = 1, int style = PS_SOLID) {
  HPEN pen = CreatePen(style, width, color);
  HGDIOBJ old = SelectObject(dc, pen);
  HBRUSH nullBrush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
  HGDIOBJ oldB = SelectObject(dc, nullBrush);
  Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
  SelectObject(dc, oldB);
  SelectObject(dc, old);
  DeleteObject(pen);
}

inline void Line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1, int style = PS_SOLID) {
  HPEN pen = CreatePen(style, width, color);
  HGDIOBJ old = SelectObject(dc, pen);
  MoveToEx(dc, x1, y1, nullptr);
  LineTo(dc, x2, y2);
  SelectObject(dc, old);
  DeleteObject(pen);
}

inline void Text(HDC dc, const RECT& r, const wchar_t* text, HFONT font, COLORREF color, UINT flags) {
  HGDIOBJ old = SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, color);
  DrawTextW(dc, text, -1, const_cast<RECT*>(&r), flags);
  SelectObject(dc, old);
}

inline void Text(HDC dc, const RECT& r, const std::wstring& text, HFONT font, COLORREF color, UINT flags) {
  HGDIOBJ old = SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, color);
  DrawTextW(dc, text.c_str(), -1, const_cast<RECT*>(&r), flags);
  SelectObject(dc, old);
}

inline RECT Pad(const RECT& r, int left, int top, int right, int bottom) {
  return RECT { r.left + left, r.top + top, r.right - right, r.bottom - bottom };
}

}  // namespace monix::ui::gdi
