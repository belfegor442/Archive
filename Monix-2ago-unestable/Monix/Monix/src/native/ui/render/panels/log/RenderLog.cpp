#include "ui/render/panels/log/RenderLog.hpp"

LogToolbarRect MonixApp::LogToolbarRects(const RECT& clientRect) const {
  LogToolbarRect r;
  const RECT outer = ContentRect(clientRect);
  const int btnW = 90;
  const int btnH = 28;
  const int gap = 6;
  const int startY = outer.top + 44;
  int x = outer.left + 14;
  r.clear = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.pause = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.search = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.json = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.csv = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.copy = { x, startY, x + btnW, startY + btnH };
  return r;
}

std::array<RECT, 6> MonixApp::LogFilterRects(const RECT& clientRect) const {
  std::array<RECT, 6> rects{};
  const RECT outer = ContentRect(clientRect);
  const int btnH = 26;
  const int gap = 8;
  const int sidebarW = 300;
  const int availableW = (outer.right - sidebarW - 14) - outer.left - 28;
  const int btnW = (availableW - gap * 5) / 6;
  int x = outer.left + 14;
  for (std::size_t i = 0; i < 6; ++i) {
    rects[i] = { x, outer.top + 80, x + btnW, outer.top + 80 + btnH };
    x += btnW + gap;
  }
  return rects;
}

int MonixApp::CountFilteredLogs() const {
  if (state_.logState.activeFilter == LogFilter::All) {
    return static_cast<int>(state_.logState.entries.size());
  }
  int count = 0;
  for (const auto& e : state_.logState.entries) {
    switch (state_.logState.activeFilter) {
      case LogFilter::Warn:
        if (e.level == LogLevel::Warn) ++count;
        break;
      case LogFilter::Err:
        if (e.level == LogLevel::Error) ++count;
        break;
      case LogFilter::Crit:
        if (e.level == LogLevel::Critical) ++count;
        break;
      case LogFilter::Net:
        if (e.domain == L"NETWORK") ++count;
        break;
      case LogFilter::Kernel:
        if (e.domain == L"KERNEL") ++count;
        break;
      default:
        break;
    }
  }
  return count;
}

void MonixApp::DrawLogToolbar(HDC dc, const RECT& clientRect) {
  auto r = LogToolbarRects(clientRect);
  const int btnH = 30;

  auto drawBtn = [&](RECT btn, const std::wstring& label, ColorRole accent) {
    FillSolid(dc, btn, RGB(8, 12, 8));
    DrawRectOutline(dc, btn, ResolveColor(accent));
    DrawTextRect(dc, btn, label, accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  };

  drawBtn(r.clear, L"CLEAR", ColorRole::White);
  drawBtn(r.pause, state_.logState.livePaused ? L"RESUME" : L"PAUSE", state_.logState.livePaused ? ColorRole::Success : ColorRole::Warning);
  drawBtn(r.search, L"SEARCH", ColorRole::Primary);
  drawBtn(r.json, L"JSON", ColorRole::Dim);
  drawBtn(r.csv, L"CSV", ColorRole::Dim);
  drawBtn(r.copy, L"COPY", ColorRole::Dim);
}

void MonixApp::DrawLogFilterBadges(HDC dc, const RECT& clientRect) {
  auto rects = LogFilterRects(clientRect);
  const std::array<std::wstring, 6> labels = { L"ALL", L"WARN", L"NET", L"CRIT", L"ERR", L"KERNEL" };
  const std::array<ColorRole, 6> colors = {
    ColorRole::White, ColorRole::Warning, ColorRole::Network,
    ColorRole::Fatal, ColorRole::Error, ColorRole::Kernel
  };
  const std::array<int, 6> counts = {
    static_cast<int>(state_.logState.entries.size()),
    state_.logState.counters.warnings,
    state_.logState.counters.network,
    state_.logState.counters.critical,
    state_.logState.counters.errors,
    state_.logState.counters.kernel
  };
  const std::array<LogFilter, 6> filters = {
    LogFilter::All, LogFilter::Warn, LogFilter::Net,
    LogFilter::Crit, LogFilter::Err, LogFilter::Kernel
  };

  for (std::size_t i = 0; i < 6; ++i) {
    const bool active = (state_.logState.activeFilter == filters[i]);
    COLORREF bg = active ? RGB(18, 50, 20) : RGB(8, 12, 8);
    FillSolid(dc, rects[i], bg);
    DrawRectOutline(dc, rects[i], active ? ResolveColor(colors[i]) : ResolveColor(ColorRole::Accent));
    std::wstring text = labels[i] + L": " + std::to_wstring(counts[i]);
    RECT textRect = { rects[i].left + 4, rects[i].top, rects[i].right - 4, rects[i].bottom };
    DrawTextRect(dc, textRect, text, active ? colors[i] : ColorRole::Dim, smallFont_,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }
}

void MonixApp::DrawLogColumnHeaders(HDC dc, const RECT& logArea) {
  const int headerH = 26;
  RECT header = { logArea.left, logArea.top, logArea.right, logArea.top + headerH };
  FillSolid(dc, header, RGB(10, 16, 10));
  DrawRectOutline(dc, header, ResolveColor(ColorRole::Accent));

  const int totalW = header.right - header.left;
  const int timeW = static_cast<int>(totalW * 0.12);
  const int catW = static_cast<int>(totalW * 0.18);
  const int subcatW = static_cast<int>(totalW * 0.12);
  const int contentW = totalW - timeW - catW - subcatW;

  int x = header.left + 10;
  DrawTextRect(dc, { x, header.top, x + timeW, header.bottom }, L"TIME", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  x += timeW;
  DrawTextRect(dc, { x, header.top, x + catW, header.bottom }, L"CATEGORY", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  x += catW;
  DrawTextRect(dc, { x, header.top, x + subcatW, header.bottom }, L"SUB-CATEGORY", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  x += subcatW;
  DrawTextRect(dc, { x, header.top, header.right - 10, header.bottom }, L"CONTENT", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawLogSidebar(HDC dc, const RECT& sidebarRect) {
  DrawPanel(dc, sidebarRect, L"STATUS", ColorRole::Primary, smallFont_);

  const int chartH = 150;
  RECT chartRect = { sidebarRect.left + 14, sidebarRect.top + 46, sidebarRect.right - 14, sidebarRect.top + 46 + chartH };
  FillSolid(dc, chartRect, RGB(4, 4, 5));
  DrawRectOutline(dc, chartRect, ResolveColor(ColorRole::Accent));

  auto drawOverlay = [&](HDC dc, const RECT& rect, const std::vector<double>& values, ColorRole color) {
    if (values.size() < 2) return;
    HPEN pen = CreatePen(PS_SOLID, 2, ResolveColor(color));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const int width = std::max(1, static_cast<int>(rect.right - rect.left - 10));
    const int height = std::max(1, static_cast<int>(rect.bottom - rect.top - 10));
    const double step = static_cast<double>(width) / static_cast<double>(std::max<std::size_t>(1, values.size() - 1));
    for (std::size_t i = 0; i < values.size(); ++i) {
      const double normalized = std::clamp(values[i], 0.0, 1.0);
      const int x = rect.left + 5 + static_cast<int>(i * step);
      const int y = rect.bottom - 5 - static_cast<int>(normalized * height);
      if (i == 0) MoveToEx(dc, x, y, nullptr);
      else LineTo(dc, x, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  };

  drawOverlay(dc, chartRect, state_.logState.warnHistory,   ColorRole::Warning);
  drawOverlay(dc, chartRect, state_.logState.errHistory,    ColorRole::Error);
  drawOverlay(dc, chartRect, state_.logState.critHistory,   ColorRole::Fatal);
  drawOverlay(dc, chartRect, state_.logState.kernelHistory, ColorRole::Kernel);
  drawOverlay(dc, chartRect, state_.logState.netHistory,    ColorRole::Network);

  RECT legendRect = { chartRect.left + 4, chartRect.bottom + 2, chartRect.right - 4, chartRect.bottom + 18 };
  const int dotSpacing = (legendRect.right - legendRect.left) / 5;
  auto drawDot = [&](int x, const std::wstring& label, ColorRole c) {
    HBRUSH br = CreateSolidBrush(ResolveColor(c));
    RECT dot = { x, legendRect.top + 4, x + 8, legendRect.bottom - 4 };
    FillRect(dc, &dot, br);
    DeleteObject(br);
    RECT lr = { x + 10, legendRect.top, x + 50, legendRect.bottom };
    DrawTextRect(dc, lr, label, ColorRole::Dim, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  };
  int lx = legendRect.left;
  drawDot(lx, L"WARN", ColorRole::Warning);   lx += dotSpacing;
  drawDot(lx, L"ERR", ColorRole::Error);      lx += dotSpacing;
  drawDot(lx, L"CRIT", ColorRole::Fatal);     lx += dotSpacing;
  drawDot(lx, L"KERN", ColorRole::Kernel);    lx += dotSpacing;
  drawDot(lx, L"NET", ColorRole::Network);

  RECT counterPanel = { sidebarRect.left + 14, chartRect.bottom + 22, sidebarRect.right - 14, sidebarRect.bottom - 14 };
  FillSolid(dc, counterPanel, RGB(8, 10, 8));
  DrawRectOutline(dc, counterPanel, ResolveColor(ColorRole::Accent));

  RECT cTitle = counterPanel;
  cTitle.bottom = cTitle.top + 30;
  FillSolid(dc, cTitle, RGB(10, 14, 10));
  DrawTextRect(dc, cTitle, L"COUNTER", ColorRole::Engine, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  const int rowH = 28;
  int y = cTitle.bottom + 6;
  auto drawCounter = [&](const std::wstring& label, int value, ColorRole color) {
    if (y + rowH > counterPanel.bottom) return;
    RECT row = { counterPanel.left + 8, y, counterPanel.right - 8, y + rowH };
    DrawTextRect(dc, row, label, ColorRole::Dim, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT valRect = { counterPanel.right - 70, y, counterPanel.right - 8, y + rowH };
    DrawTextRect(dc, valRect, std::to_wstring(value), color, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += rowH;
  };

  drawCounter(L"Warnings:", state_.logState.counters.warnings, ColorRole::Warning);
  drawCounter(L"Error:", state_.logState.counters.errors, ColorRole::Error);
  drawCounter(L"Network:", state_.logState.counters.network, ColorRole::Network);
  drawCounter(L"Critical:", state_.logState.counters.critical, ColorRole::Fatal);
  drawCounter(L"Kernel:", state_.logState.counters.kernel, ColorRole::Kernel);
}

void MonixApp::DrawLogView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  DrawPanel(dc, outer, L"LOG CORE", ColorRole::Engine, smallFont_);

  const std::wstring counterText =
    L"TOTAL " + std::to_wstring(CountFilteredLogs()) +
    L" | INFO " + std::to_wstring(state_.logState.counters.info) +
    L" | WARN " + std::to_wstring(state_.logState.counters.warnings) +
    L" | ERROR " + std::to_wstring(state_.logState.counters.errors) +
    L" | CRIT " + std::to_wstring(state_.logState.counters.critical);
  DrawTextRect(dc, RECT { outer.left + 220, outer.top + 10, outer.right - 18, outer.top + 38 }, counterText, ColorRole::Dim, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  DrawLogToolbar(dc, clientRect);
  DrawLogFilterBadges(dc, clientRect);

  const int sidebarW = 300;
  const int sidebarGap = 14;
  const int colHeaderH = 26;
  const int contentStartY = outer.top + 114;
  RECT sidebarRect = { outer.right - sidebarW - 14, contentStartY, outer.right - 14, outer.bottom - 14 };
  DrawLogSidebar(dc, sidebarRect);

  RECT logArea = { outer.left + 14, contentStartY, sidebarRect.left - sidebarGap, outer.bottom - 14 };
  DrawLogColumnHeaders(dc, logArea);

  RECT listRect = { logArea.left, logArea.top + colHeaderH, logArea.right, logArea.bottom };
  const int visibleLines = VisibleLogLines(listRect);

  std::vector<int> filteredIndices;
  filteredIndices.reserve(state_.logState.entries.size());
  for (int i = 0; i < static_cast<int>(state_.logState.entries.size()); ++i) {
    const auto& e = state_.logState.entries[i];
    switch (state_.logState.activeFilter) {
      case LogFilter::All:
        filteredIndices.push_back(i);
        break;
      case LogFilter::Warn:
        if (e.level == LogLevel::Warn) filteredIndices.push_back(i);
        break;
      case LogFilter::Err:
        if (e.level == LogLevel::Error) filteredIndices.push_back(i);
        break;
      case LogFilter::Crit:
        if (e.level == LogLevel::Critical) filteredIndices.push_back(i);
        break;
      case LogFilter::Net:
        if (e.domain == L"NETWORK") filteredIndices.push_back(i);
        break;
      case LogFilter::Kernel:
        if (e.domain == L"KERNEL") filteredIndices.push_back(i);
        break;
    }
  }

  const int totalFiltered = static_cast<int>(filteredIndices.size());
  const int maxScroll = std::max(0, totalFiltered - visibleLines);
  // DEFENSIVE CLAMP: Primary scroll mutation happens in WM_MOUSEWHEEL handler (AppWindowProc.cpp:992).
  // This clamp is a safety net to ensure scroll stays in bounds if the log list shrinks between frames.
  state_.logState.scroll = std::clamp(state_.logState.scroll, 0, maxScroll);

  int y = listRect.top;
  if (filteredIndices.empty()) {
    DrawTextRect(dc, listRect, state_.logState.entries.empty() ? L"Waiting for live telemetry logs..." : L"No logs match the current filter.",
      ColorRole::Dim, bodyFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  const int totalW = listRect.right - listRect.left;
  const int timeW = static_cast<int>(totalW * 0.12);
  const int catW = static_cast<int>(totalW * 0.18);
  const int subcatW = static_cast<int>(totalW * 0.12);

  for (int i = 0; i < visibleLines && state_.logState.scroll + i < totalFiltered; ++i) {
    const auto& entry = state_.logState.entries[filteredIndices[state_.logState.scroll + i]];

    RECT rowRect = { listRect.left, y, listRect.right, y + logLineHeight_ + 2 };
    if ((state_.logState.scroll + i) % 2 == 0) {
      FillSolid(dc, rowRect, RGB(6, 8, 6));
    }

    int x = listRect.left + 6;
    std::wstring timeLabel = entry.time;
    if (entry.sessionId.size() > 4) {
      timeLabel += L" " + entry.sessionId.substr(entry.sessionId.size() - 4);
    }
    DrawTextRect(dc, { x, y, x + timeW - 4, y + logLineHeight_ }, timeLabel, ColorRole::Dim, logFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    x += timeW;
    DrawTextRect(dc, { x, y, x + catW - 4, y + logLineHeight_ }, L"[" + entry.domain + L"]", entry.color, logFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    x += catW;
    DrawTextRect(dc, { x, y, x + subcatW - 4, y + logLineHeight_ }, entry.severity, entry.color, logFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    x += subcatW;
    DrawTextRect(dc, { x, y, listRect.right - 4, y + logLineHeight_ }, entry.message, entry.color, logFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    y += logLineHeight_ + 2;
  }
}
