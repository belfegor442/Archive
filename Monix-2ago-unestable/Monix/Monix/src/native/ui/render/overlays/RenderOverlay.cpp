#include "ui/render/overlays/RenderOverlay.hpp"
#include "core/StringUtils.hpp"

void MonixApp::DrawBackground(HDC dc, const RECT& clientRect) {
  FillSolid(dc, clientRect, RGB(5, 5, 5));

  const int height = std::max(1L, clientRect.bottom - clientRect.top);
  const int bands = 42;
  for (int index = 0; index < bands; ++index) {
    const int bandTop = clientRect.top + (height * index) / bands;
    const int bandBottom = clientRect.top + (height * (index + 1)) / bands;
    const double position = static_cast<double>(bandTop - clientRect.top) / static_cast<double>(height);
    const double centerBias = 1.0 - std::fabs((position * 2.0) - 1.0);
    const int green = static_cast<int>(10 + centerBias * 12.0);
    RECT bandRect { clientRect.left, bandTop, clientRect.right, std::max(bandTop + 1, bandBottom) };
    FillSolid(dc, bandRect, RGB(4, green, 6));
  }

  RECT topFalloff { clientRect.left, clientRect.top, clientRect.right, clientRect.top + 90 };
  FillSolid(dc, topFalloff, RGB(8, 12, 8));
}

void MonixApp::DrawTabs(HDC dc, const RECT& clientRect) {
  const auto rects = TabRects(clientRect);
  const std::array<std::wstring, 6> labels { L"LOG", L"TASKS", L"HARDWARE", L"NETWORK", L"SCRAM", L"SETTINGS" };

  for (std::size_t i = 0; i < rects.size(); ++i) {
    RECT rect = rects[i];
    const Tab tab = static_cast<Tab>(i);
    const bool active = state_.activeTab == tab;
    FillSolid(dc, rect, active ? RGB(22, 86, 38) : RGB(8, 12, 8));
    DrawRectOutline(dc, rect, active ? ResolveColor(ColorRole::Primary) : ResolveColor(ColorRole::Accent));
    DrawTextRect(dc, rect, labels[i], active ? ColorRole::White : ColorRole::Primary, tabFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  RECT badge {
    rects.back().right + 12,
    rects.back().top,
    clientRect.right - 72,
    rects.back().bottom
  };
  FillSolid(dc, badge, RGB(8, 12, 8));
  DrawRectOutline(dc, badge, ResolveColor(ColorRole::Accent));
  const std::wstring badgeText = L"MONIX CORE 3.0 :: " + state_.snapshot.host;
  DrawTextRect(dc, badge, badgeText, ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawFooter(HDC dc, const RECT& clientRect) {
  RECT footer { 72, clientRect.bottom - 50, clientRect.right - 72, clientRect.bottom - 18 };
  FillSolid(dc, footer, RGB(8, 12, 8));
  DrawRectOutline(dc, footer, ResolveColor(ColorRole::Accent));

  std::wstring shaderName = L"base";
  std::wstring fontDisplayName = L"Terminal";
  if (!paths_.fontList.empty() && state_.currentFontIndex >= 0 && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
    fontDisplayName = paths_.fontList[state_.currentFontIndex].displayName;
  }
  const std::wstring footerText =
    L"Delay " + std::to_wstring(config_.telemetryIntervalMs) + L"ms | "
    + fontDisplayName + L" | "
    + shaderName + L" | "
    L"View " + LogViewModeText(config_.logViewMode) + L" | "
    L"F1 debug | F2 cache | F3 feedback | F4 textures | F5 reload | F6 JSON | F7 CSV | F8 triage | F9 font | F10 shot | F11 demo";
  DrawTextRect(dc, RECT { footer.left + 12, footer.top + 2, footer.right - 12, footer.bottom }, footerText, ColorRole::Dim, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawToast(HDC dc, const RECT& clientRect) {
  if (state_.notifState.toastMessage.empty()) {
    return;
  }

  RECT toast {
    clientRect.right - 390,
    clientRect.bottom - 108,
    clientRect.right - 72,
    clientRect.bottom - 68
  };
  FillSolid(dc, toast, RGB(10, 16, 10));
  DrawRectOutline(dc, toast, ResolveColor(ColorRole::UserInput));
  DrawTextRect(dc, RECT { toast.left + 10, toast.top + 2, toast.right - 10, toast.bottom }, state_.notifState.toastMessage, ColorRole::UserInput, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawClickDebug(HDC dc, const RECT& clientRect) {
  if (!state_.clickDebug.mode) return;

  const int cw = clientRect.right - clientRect.left;
  const int ch = clientRect.bottom - clientRect.top;

  double scale = 4.0 * ch / (3.0 * cw);
  int visW = static_cast<int>(cw * scale + 0.5);
  int visX = (cw - visW) / 2;

  DrawRectOutline(dc, RECT { clientRect.left + visX, clientRect.top, clientRect.left + visX + visW, clientRect.top + ch }, RGB(120, 120, 120));

  int cx = clientRect.left + cw / 2;
  int cy = clientRect.top + ch / 2;
  HPEN crossPen = CreatePen(PS_DOT, 1, RGB(100, 100, 100));
  HGDIOBJ oldCrossPen = SelectObject(dc, crossPen);
  MoveToEx(dc, cx - 20, cy, nullptr);
  LineTo(dc, cx + 20, cy);
  MoveToEx(dc, cx, cy - 20, nullptr);
  LineTo(dc, cx, cy + 20);
  SelectObject(dc, oldCrossPen);
  DeleteObject(crossPen);

  POINT cursorPt {};
  GetCursorPos(&cursorPt);
  ScreenToClient(hwnd_, &cursorPt);
  POINT cursorBmp = MapToViewport(cursorPt);

  HPEN curPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
  HGDIOBJ oldCurPen = SelectObject(dc, curPen);
  int cr = 8;
  Ellipse(dc, cursorBmp.x - cr, cursorBmp.y - cr, cursorBmp.x + cr, cursorBmp.y + cr);
  SelectObject(dc, oldCurPen);
  DeleteObject(curPen);

  if (state_.clickDebug.ms > 0 && GetTickCount64() - state_.clickDebug.ms < 5000) {
    HPEN hitPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HGDIOBJ oldHitPen = SelectObject(dc, hitPen);
    HBRUSH hitBrush = CreateSolidBrush(RGB(255, 255, 255));
    HGDIOBJ oldBrush = SelectObject(dc, hitBrush);
    int r = 6;
    Ellipse(dc, state_.clickDebug.bmp.x - r, state_.clickDebug.bmp.y - r, state_.clickDebug.bmp.x + r, state_.clickDebug.bmp.y + r);
    SelectObject(dc, oldHitPen);
    DeleteObject(hitPen);
    SelectObject(dc, oldBrush);
    DeleteObject(hitBrush);
  }

  HFONT oldFont = static_cast<HFONT>(SelectObject(dc, smallFont_));
  SetBkMode(dc, OPAQUE);
  SetBkColor(dc, RGB(0, 0, 0));
  SetTextColor(dc, RGB(255, 255, 255));

  wchar_t line1[160];
  swprintf_s(line1, L"CURSOR WND(%ld,%ld) -> BMP(%ld,%ld) | visX=%d visW=%d cw=%d ch=%d",
    cursorPt.x, cursorPt.y, cursorBmp.x, cursorBmp.y, visX, visW, cw, ch);
  RECT textR1 { clientRect.left + 4, clientRect.bottom - 50, clientRect.right - 4, clientRect.bottom - 30 };
  DrawText(dc, line1, -1, &textR1, DT_LEFT | DT_SINGLELINE);

  wchar_t line2[160];
  if (state_.clickDebug.ms > 0 && GetTickCount64() - state_.clickDebug.ms < 5000) {
    swprintf_s(line2, L"CLICK WND(%ld,%ld) -> BMP(%ld,%ld)",
      state_.clickDebug.wnd.x, state_.clickDebug.wnd.y, state_.clickDebug.bmp.x, state_.clickDebug.bmp.y);
  } else {
    wcscpy_s(line2, L"Click anywhere to debug mapping");
  }
  RECT textR2 { clientRect.left + 4, clientRect.bottom - 28, clientRect.right - 4, clientRect.bottom - 8 };
  DrawText(dc, line2, -1, &textR2, DT_LEFT | DT_SINGLELINE);

  SelectObject(dc, oldFont);
}

void MonixApp::DrawNotifications(HDC dc, const RECT& clientRect) {
  int y = 84;
  for (std::size_t i = 0; i < state_.notifState.items.size(); ++i) {
    const auto& item = state_.notifState.items[i];
    RECT card {
      clientRect.right - kNotificationWidth - 18,
      y,
      clientRect.right - 18,
      y + kNotificationHeight
    };
    FillSolid(dc, card, RGB(10, 14, 10));
    DrawRectOutline(dc, card, ResolveColor(item.color));
    DrawTextRect(dc, RECT { card.left + 12, card.top + 8, card.right - 12, card.top + 28 }, item.title, item.color, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    DrawTextRect(dc, RECT { card.left + 12, card.top + 28, card.right - 12, card.bottom - 8 }, item.message, ColorRole::White, smallFont_, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    y += kNotificationHeight + 10;
  }
}

void MonixApp::DrawIntro(HDC dc, const RECT& clientRect) {
  const auto logo = monix::IntroLogo();
  int cellW = 0;
  int cellH = 0;
  int logoWidth = 0;
  int logoHeight = 0;
  int maxColumns = 0;
  ComputeIntroLogoMetrics(clientRect, cellW, cellH, logoWidth, logoHeight, maxColumns);

  const int logoLeft = clientRect.left + ((clientRect.right - clientRect.left) - logoWidth) / 2;
  int y = state_.intro.logoY;
  for (const auto& line : logo) {
    int x = logoLeft;
    for (wchar_t ch : line) {
      if (ch == L'█' || ch == L'░') {
        RECT cell { x, y, x + cellW - 1, y + cellH - 1 };
        FillSolid(dc, cell, ch == L'█' ? ResolveColor(ColorRole::Primary) : ResolveColor(ColorRole::Dim));
      }
      x += cellW;
    }
    y += cellH;
  }

  if (state_.intro.settledAtMs != 0 && GetTickCount64() - state_.intro.settledAtMs >= config_.introCreditDelayMs) {
    const int creditY = state_.intro.logoY + logoHeight + 20 - state_.intro.creditOffset;
    DrawTextRect(dc, RECT { clientRect.left, creditY, clientRect.right, creditY + smallLineHeight_ + 10 }, L"Created by: belfegor442", ColorRole::Dim, bodyFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  DrawTextRect(dc, RECT { clientRect.left, clientRect.bottom - 140, clientRect.right, clientRect.bottom - 88 }, L"Booting native telemetry, S.C.R.A.M watcher and kernel probes...", ColorRole::Dim, bodyFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawTaskContextMenu(HDC dc) const {
  if (!state_.taskMenu.visible) {
    return;
  }

  static const std::array<std::wstring, 9> items {
    L"Inspect process",
    L"Track in S.C.R.A.M",
    L"Copy name and PID",
    L"Set priority HIGH",
    L"Set priority LOW",
    L"Terminate process",
    L"Export logs JSON",
    L"Refresh snapshot",
    L"Open in Explorer"
  };

  FillSolid(dc, state_.taskMenu.rect, RGB(8, 24, 10));
  DrawRectOutline(dc, state_.taskMenu.rect, ResolveColor(ColorRole::Scram));

  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    RECT itemRect {
      state_.taskMenu.rect.left,
      state_.taskMenu.rect.top + i * kTaskMenuItemHeight,
      state_.taskMenu.rect.right,
      state_.taskMenu.rect.top + (i + 1) * kTaskMenuItemHeight
    };
    if (i == state_.taskMenu.hoverIndex) {
      FillSolid(dc, itemRect, RGB(24, 52, 28));
    }
    DrawRectOutline(dc, itemRect, ResolveColor(ColorRole::Accent));
    DrawTextRect(dc, RECT { itemRect.left + 12, itemRect.top + 2, itemRect.right - 12, itemRect.bottom }, items[i], i == state_.taskMenu.hoverIndex ? ColorRole::White : ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }
}

void MonixApp::DrawDevShell(HDC dc, const RECT& clientRect) {
  if (!state_.devShell.active) return;

  const int W = clientRect.right - clientRect.left;
  const int H = clientRect.bottom - clientRect.top;
  const int shellH = H / 3;
  const int shellTop = H - shellH;

  RECT shellRect { clientRect.left, shellTop, clientRect.right, clientRect.bottom };
  FillSolid(dc, shellRect, RGB(0, 0, 0));

  RECT borderRect { clientRect.left, shellTop, clientRect.right, shellTop + 1 };
  FillSolid(dc, borderRect, RGB(60, 180, 80));

  const int margin = 12;
  const int lineH = smallLineHeight_ + 2;
  const int maxLines = (shellH - 30) / lineH;

  int startIdx = std::max(0, static_cast<int>(state_.devShell.history.size()) - maxLines + state_.devShell.scrollOffset);
  int y = shellTop + 8;

  for (int i = startIdx; i < static_cast<int>(state_.devShell.history.size()) && y < shellTop + shellH - 24; ++i) {
    const auto& line = state_.devShell.history[i];
    RECT lineRect { clientRect.left + margin, y, clientRect.right - margin, y + lineH };
    ColorRole color = ColorRole::Success;
    if (!line.empty() && line[0] == L'>') color = ColorRole::White;
    else if (line.find(L"FAIL") != std::wstring::npos || line.find(L"ERROR") != std::wstring::npos) color = ColorRole::Error;
    else if (line.find(L"WARN") != std::wstring::npos) color = ColorRole::Warning;
    DrawTextRect(dc, lineRect, line.c_str(), color, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    y += lineH;
  }

  RECT inputRect { clientRect.left + margin, shellTop + shellH - 20, clientRect.right - margin, shellTop + shellH - 2 };
  wchar_t prompt[256];
  ULONGLONG now = GetTickCount64();
  bool cursorOn = ((now / 500) % 2) == 0;
  _snwprintf_s(prompt, 256, L"monix> %s%s", state_.devShell.input.c_str(), cursorOn ? L"_" : L"");
  DrawTextRect(dc, inputRect, prompt, ColorRole::Success, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}
