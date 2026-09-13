#include "ui/render/panels/network/RenderNetwork.hpp"

void MonixApp::DrawNetworkView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  RECT statsPanel = outer;
  statsPanel.bottom = outer.top + 188;
  RECT flowPanel = outer;
  flowPanel.top = statsPanel.bottom + 14;
  flowPanel.right = outer.left + static_cast<int>((outer.right - outer.left) * 0.62);
  flowPanel.bottom = outer.bottom - 226;
  RECT detailPanel = outer;
  detailPanel.left = flowPanel.right + 14;
  detailPanel.top = statsPanel.bottom + 14;
  detailPanel.bottom = flowPanel.bottom;
  RECT logPanel = outer;
  logPanel.top = flowPanel.bottom + 14;

  DrawPanel(dc, statsPanel, L"NETWORK OVERVIEW", ColorRole::Engine, smallFont_);
  DrawPanel(dc, flowPanel, L"LIVE SOCKET FLOWS", ColorRole::Network, smallFont_);
  DrawPanel(dc, detailPanel, L"NETWORK DIAGNOSTICS", ColorRole::Primary, smallFont_);
  DrawPanel(dc, logPanel, L"NETWORK EVENT LOG", ColorRole::Engine, smallFont_);

  const auto historyMax = [](const std::vector<double>& values, double minimum) {
    double peak = minimum;
    for (double value : values) {
      peak = std::max(peak, value);
    }
    return peak;
  };

  const auto topFlow = std::max_element(state_.snapshot.flows.begin(), state_.snapshot.flows.end(), [](const NetworkFlow& left, const NetworkFlow& right) {
    return left.activeConnections < right.activeConnections;
  });
  const std::wstring busiestFlow = topFlow == state_.snapshot.flows.end() ?
    L"Awaiting socket inventory" :
    topFlow->name + L" (" + std::to_wstring(topFlow->activeConnections) + L")";

  const int columnWidth = (statsPanel.right - statsPanel.left - 42) / 3;
  const int cardHeight = 52;
  const std::array<std::wstring, 6> captions {
    L"Download: " + FormatRate(state_.snapshot.netDownBytesPerSec),
    L"Upload: " + FormatRate(state_.snapshot.netUpBytesPerSec),
    L"Latency: " + std::to_wstring(state_.snapshot.latencyMs) + L"ms",
    L"Established: " + std::to_wstring(state_.snapshot.outboundConnections),
    L"Listening: " + std::to_wstring(state_.snapshot.inboundConnections),
    L"Top talker: " + busiestFlow
  };

  for (int i = 0; i < static_cast<int>(captions.size()); ++i) {
    const int row = i / 3;
    const int column = i % 3;
    RECT card {
      statsPanel.left + 16 + column * columnWidth,
      statsPanel.top + 54 + row * (cardHeight + 12),
      statsPanel.left + 16 + (column + 1) * columnWidth - 12,
      statsPanel.top + 54 + row * (cardHeight + 12) + cardHeight
    };
    FillSolid(dc, card, RGB(8, 18, 10));
    DrawRectOutline(dc, card, ResolveColor(ColorRole::Accent));
    DrawTextRect(dc, card, captions[i], i < 2 ? ColorRole::Network : (i == 2 ? ColorRole::Primary : ColorRole::White), smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  RECT downSpark { detailPanel.left + 18, detailPanel.top + 56, detailPanel.right - 18, detailPanel.top + 106 };
  RECT upSpark { detailPanel.left + 18, detailPanel.top + 126, detailPanel.right - 18, detailPanel.top + 176 };
  RECT latencySpark { detailPanel.left + 18, detailPanel.top + 196, detailPanel.right - 18, detailPanel.top + 246 };
  DrawSparkline(dc, downSpark, state_.history.net, historyMax(state_.history.net, 8.0), ColorRole::Network);
  DrawSparkline(dc, upSpark, state_.history.netUpload, historyMax(state_.history.netUpload, 4.0), ColorRole::Primary);
  DrawSparkline(dc, latencySpark, state_.history.latency, historyMax(state_.history.latency, 25.0), ColorRole::UserInput);

  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 252, detailPanel.right - detailPanel.left - 36, L"DNS estimator: " + std::to_wstring(state_.snapshot.dnsPseudo) + L"/s", ColorRole::Engine, smallFont_);
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 282, detailPanel.right - detailPanel.left - 36, L"Tracked process count: " + std::to_wstring(static_cast<int>(state_.snapshot.flows.size())), ColorRole::Dim, smallFont_);
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 312, detailPanel.right - detailPanel.left - 36, L"Collector: TCP inventory grouped by owning process", ColorRole::Dim, smallFont_);
  DrawTextRect(dc, RECT { detailPanel.left + 18, detailPanel.top + 346, detailPanel.right - 18, detailPanel.bottom - 18 }, L"Charts are stable and non-reactive: traffic, latency and noise do not alter the CRT treatment. Only the data changes, never the display personality.", ColorRole::Dim, smallFont_, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

  RECT header { flowPanel.left + 14, flowPanel.top + 46, flowPanel.right - 14, flowPanel.top + 88 };
  FillSolid(dc, header, RGB(8, 18, 10));
  DrawRectOutline(dc, header, ResolveColor(ColorRole::Accent));
  DrawTextRect(dc, RECT { header.left + 10, header.top, header.left + 220, header.bottom }, L"PROCESS", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { header.left + 230, header.top, header.left + 350, header.bottom }, L"EST.", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { header.left + 360, header.top, header.right - 110, header.bottom }, L"REMOTE", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { header.right - 104, header.top, header.right - 12, header.bottom }, L"STATE", ColorRole::Primary, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  int y = header.bottom;
  const int rowHeight = std::max(36, bodyLineHeight_ + 12);
  for (std::size_t i = 0; i < state_.snapshot.flows.size() && i < 10; ++i) {
    if (y + rowHeight > flowPanel.bottom) break;
    RECT row { header.left, y, header.right, y + rowHeight };
    FillSolid(dc, row, RGB(4, 12, 5));
    DrawRectOutline(dc, row, ResolveColor(ColorRole::Accent));
    const auto& flow = state_.snapshot.flows[i];
    DrawTextLine(dc, row.left + 10, row.top + 6, 200, flow.name, ColorRole::White, smallFont_);
    DrawTextLine(dc, row.left + 230, row.top + 6, 110, std::to_wstring(flow.activeConnections), ColorRole::Network, smallFont_);
    const int remoteW = (std::max)(60, static_cast<int>(row.right - row.left - 490));
    DrawTextLine(dc, row.left + 360, row.top + 6, remoteW, flow.remote, ColorRole::Dim, smallFont_);
    DrawTextLine(dc, row.right - 104, row.top + 6, 90, flow.state, flow.state == L"ACTIVE" ? ColorRole::Success : ColorRole::Dim, smallFont_);
    y += rowHeight;
  }

  std::vector<LogEntry> networkLogs;
  for (const auto& entry : state_.logState.entries) {
    if (entry.domain == L"NETWORK") {
      networkLogs.push_back(entry);
    }
  }
  y = logPanel.top + 52;
  if (networkLogs.empty()) {
    DrawTextRect(dc, RECT { logPanel.left + 16, logPanel.top + 50, logPanel.right - 16, logPanel.bottom - 18 }, L"No network events emitted yet.", ColorRole::Dim, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  for (std::size_t i = 0; i < networkLogs.size() && i < 6; ++i) {
    DrawTextLine(dc, logPanel.left + 16, y, logPanel.right - logPanel.left - 32, ComposeLogLine(networkLogs[i]), networkLogs[i].color, smallFont_);
    y += logLineHeight_ + 6;
  }
}
