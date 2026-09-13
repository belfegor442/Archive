#include "ui/render/primitives/RenderPrimitives.hpp"

void MonixApp::DrawTextRect(HDC dc, const RECT& rect, const std::wstring& text, ColorRole color, HFONT font, UINT format) const {
  SetBkMode(dc, TRANSPARENT);
  HGDIOBJ oldFont = SelectObject(dc, font);

  RECT mainRect = rect;
  SetTextColor(dc, ResolveColor(color));
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &mainRect, format);

  SelectObject(dc, oldFont);
}

void MonixApp::DrawTextLine(HDC dc, int x, int y, int width, const std::wstring& text, ColorRole color, HFONT font, UINT format) const {
  RECT rect { x, y, x + width, y + std::max(bodyLineHeight_ + 8, 28) };
  DrawTextRect(dc, rect, text, color, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS | format);
}

void MonixApp::DrawPanel(HDC dc, const RECT& rect, const std::wstring& title, ColorRole accent, HFONT titleFont) const {
  FillSolid(dc, rect, RGB(8, 10, 8));
  DrawRectOutline(dc, rect, ResolveColor(ColorRole::Accent));
  RECT header = rect;
  header.bottom = header.top + 38;
  FillSolid(dc, header, RGB(10, 14, 10));
  DrawRectOutline(dc, header, ResolveColor(accent));
  DrawTextRect(dc, RECT { header.left + 12, header.top + 4, header.right - 12, header.bottom }, title, accent, titleFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawProgressBar(HDC dc, const RECT& rect, double pct, ColorRole accent) const {
  FillSolid(dc, rect, RGB(7, 18, 9));
  DrawRectOutline(dc, rect, ResolveColor(ColorRole::Accent));
  RECT fill = ShrinkRect(rect, 2);
  fill.right = fill.left + static_cast<int>((fill.right - fill.left) * std::clamp(pct, 0.0, 100.0) / 100.0);
  if (fill.right > fill.left) {
    FillSolid(dc, fill, ResolveColor(accent));
  }
}

void MonixApp::DrawSparkline(HDC dc, const RECT& rect, const std::vector<double>& values, double maxValue, ColorRole color) const {
  FillSolid(dc, rect, RGB(4, 12, 5));
  DrawRectOutline(dc, rect, ResolveColor(ColorRole::Accent));
  if (values.size() < 2 || maxValue <= 0.0) {
    return;
  }

  HPEN pen = CreatePen(PS_SOLID, 2, ResolveColor(color));
  HGDIOBJ oldPen = SelectObject(dc, pen);
  const int width = std::max(1L, rect.right - rect.left - 10);
  const int height = std::max(1L, rect.bottom - rect.top - 10);
  const double step = static_cast<double>(width) / static_cast<double>(std::max<std::size_t>(1, values.size() - 1));

  for (std::size_t i = 0; i < values.size(); ++i) {
    const double normalized = std::clamp(values[i] / maxValue, 0.0, 1.0);
    const int x = rect.left + 5 + static_cast<int>(i * step);
    const int y = rect.bottom - 5 - static_cast<int>(normalized * height);
    if (i == 0) {
      MoveToEx(dc, x, y, nullptr);
    } else {
      LineTo(dc, x, y);
    }
  }

  SelectObject(dc, oldPen);
  DeleteObject(pen);
}

RECT MonixApp::ContentRect(const RECT& clientRect) const {
  return RECT { clientRect.left + 72, clientRect.top + 118, clientRect.right - 72, clientRect.bottom - 66 };
}

std::array<RECT, 6> MonixApp::TabRects(const RECT& clientRect) const {
  const int top = 26;
  const int height = 54;
  const int startX = 72;
  const std::array<int, 6> baseWidths { 170, 220, 220, 260, 170, 250 };
  const int badgeWidth = 290;
  const int available = std::max(900, static_cast<int>(clientRect.right - startX - 72 - badgeWidth));
  const double scale = std::min(1.0, static_cast<double>(available) / 1290.0);
  std::array<RECT, 6> rects {};
  int cursor = startX;
  for (std::size_t i = 0; i < rects.size(); ++i) {
    const int width = static_cast<int>(baseWidths[i] * scale);
    rects[i] = RECT { cursor, top, cursor + width, top + height };
    cursor += width;
  }
  return rects;
}

std::wstring MonixApp::ComposeLogLine(const LogEntry& entry) const {
  std::wstring line;
  switch (config_.logViewMode) {
    case LogViewMode::Timeline:
      line = L">> " + entry.time + L" [" + entry.domain + L"] " + entry.severity + L" " + entry.message;
      break;
    case LogViewMode::Compact:
      line = L"[" + entry.severity + L"] " + entry.message;
      break;
    case LogViewMode::Structured:
    default:
      line = L"#" + std::to_wstring(entry.eventId) +
        L" " + entry.time +
        L" " + entry.severity +
        L" " + entry.domain +
        L" mod=" + entry.module +
        L" pid=" + std::to_wstring(entry.processId) +
        L" tid=" + std::to_wstring(entry.threadId) +
        L" :: " + entry.message;
      if (!entry.metadata.empty()) {
        line += L" {" + entry.metadata + L"}";
      }
      break;
  }
  if (entry.repeatCount > 1) {
    line += L" x" + std::to_wstring(entry.repeatCount);
  }
  return line;
}
