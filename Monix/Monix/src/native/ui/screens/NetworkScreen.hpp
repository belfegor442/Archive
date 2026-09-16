#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/history/NetworkHistory.hpp"

namespace monix::ui {

class NetworkScreen : public Screen {
public:
  std::wstring Name() const override { return L"Network"; }
  int Id() const override { return 3; }

  void SetHistory(const NetworkHistory* nh) { history_ = nh; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"NETWORK MONITOR",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!history_) {
      auto noDataR = Win98Theme::R(c, 10, 50, 500, 30);
      Win98Theme::Text(ctx.dc, noDataR, L"No network data.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    int y = 50;
    auto sampleR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, sampleR, L"BANDWIDTH & LATENCY",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto latest = history_->LatestSample();
    auto statsR = Win98Theme::R(c, 10, y, 960, 18);
    wchar_t statsBuf[256];
    swprintf(statsBuf, 256,
      L"Up: %.0f KB/s | Down: %.0f KB/s | RTT: %.0f ms (avg: %.0f, min: %.0f, max: %.0f) | σ: %.1f",
      latest.upKbps, latest.downKbps, latest.rttMs,
      history_->AvgRtt(), history_->MinRtt(), history_->MaxRtt(),
      history_->StddevRtt());
    Win98Theme::Text(ctx.dc, statsR, statsBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    auto qualR = Win98Theme::R(c, 10, y, 960, 18);
    COLORREF qualColor = Win98Theme::kWin98BrightGreen;
    std::wstring quality = L"Excellent";
    if (latest.rttMs > 100.0) { quality = L"Poor"; qualColor = Win98Theme::kWin98Red; }
    else if (latest.rttMs > 50.0) { quality = L"Fair"; qualColor = Win98Theme::kWin98Yellow; }
    else if (latest.rttMs > 25.0) { quality = L"Good"; qualColor = RGB(0, 168, 0); }
    wchar_t qualBuf[128];
    swprintf(qualBuf, 128, L"Quality: %s | Drops: %.1f%% | Retransmit: %.1f%% | Conns: %d/%d",
      quality.c_str(), latest.droppedPktPct, latest.tcpRetransmitPct,
      latest.activeConns, latest.totalConns);
    Win98Theme::Text(ctx.dc, qualR, qualBuf,
      ctx.canvas.fonts->fontRegular, qualColor,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto trendHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, trendHeaderR, L"TREND",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto trend = history_->RttTrend();
    auto trendR = Win98Theme::R(c, 10, y, 960, 18);
    if (trend.direction == L"stable") {
      Win98Theme::Text(ctx.dc, trendR, L"RTT: Stable",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else {
      COLORREF trendColor = (trend.direction == L"rising") ?
        Win98Theme::kWin98Red : Win98Theme::kWin98BrightGreen;
      Win98Theme::Text(ctx.dc, trendR, trend.description,
        ctx.canvas.fonts->fontRegular, trendColor,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    y += 30;

    auto alerts = history_->RecentAlerts(5);
    if (!alerts.empty()) {
      auto alertHeaderR = Win98Theme::R(c, 10, y, 480, 24);
      Win98Theme::Text(ctx.dc, alertHeaderR, L"RECENT ALERTS",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;

      for (const auto& a : alerts) {
        if (y > c.designH - 40) break;
        RECT rowR = Win98Theme::R(c, 10, y, 960, 18);
        COLORREF bgColor = (a.severity >= EventSeverity::High) ?
          RGB(255, 230, 200) : RGB(255, 255, 220);
        Win98Theme::Fill(ctx.dc, rowR, bgColor);
        Win98Theme::Text(ctx.dc, rowR, L"  " + a.type + L": " + a.description,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += 20;
      }
    }

    y += 10;
    auto connHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, connHeaderR, L"CONNECTIONS",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto conns = history_->Connections();
    for (const auto& conn : conns) {
      if (y > c.designH - 40) break;
      RECT rowR = Win98Theme::R(c, 10, y, 960, 18);
      wchar_t connBuf[256];
      swprintf(connBuf, 256, L"  %s:%d -> %s:%d [%s] %s (PID %d)",
        conn.localAddress.c_str(), conn.localPort,
        conn.remoteAddress.c_str(), conn.remotePort,
        conn.state.c_str(), conn.processName.c_str(), conn.processId);
      Win98Theme::Text(ctx.dc, rowR, connBuf,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += 18;
    }

    auto ifaces = history_->Interfaces();
    if (!ifaces.empty()) {
      y += 10;
      auto ifaceHeaderR = Win98Theme::R(c, 10, y, 480, 24);
      Win98Theme::Text(ctx.dc, ifaceHeaderR, L"INTERFACES",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;

      for (const auto& iface : ifaces) {
        if (y > c.designH - 40) break;
        RECT rowR = Win98Theme::R(c, 10, y, 960, 18);
        wchar_t ifaceBuf[256];
        swprintf(ifaceBuf, 256, L"  %s: %s | %s | %s | %.0f Mbps",
          iface.name.c_str(), iface.description.c_str(),
          iface.ipv4Address.c_str(),
          iface.isWifi ? L"WiFi" : L"Wired",
          iface.speedMbps);
        Win98Theme::Text(ctx.dc, rowR, ifaceBuf,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += 18;
      }
    }
  }

private:
  const NetworkHistory* history_ = nullptr;
};

}
