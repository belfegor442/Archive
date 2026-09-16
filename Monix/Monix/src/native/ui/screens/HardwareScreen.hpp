#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/history/HardwareHistory.hpp"

namespace monix::ui {

class HardwareScreen : public Screen {
public:
  std::wstring Name() const override { return L"Hardware"; }
  int Id() const override { return 4; }

  void SetHistory(const HardwareHistory* hh) { history_ = hh; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"HARDWARE MONITOR",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!history_) {
      auto noDataR = Win98Theme::R(c, 10, 50, 500, 30);
      Win98Theme::Text(ctx.dc, noDataR, L"No hardware data.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    int y = 50;

    auto cpuHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, cpuHeaderR, L"CPU",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto cpuR = Win98Theme::R(c, 10, y, 960, 18);
    wchar_t cpuBuf[256];
    swprintf(cpuBuf, 256, L"Avg: %.1f%% | Max: %.1f%%",
      history_->AvgCpu(), history_->MaxCpu());
    Win98Theme::Text(ctx.dc, cpuR, cpuBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    auto cpuHist = history_->CpuHistory();
    if (!cpuHist.empty()) {
      auto latest = cpuHist.back();
      auto latestR = Win98Theme::R(c, 10, y, 960, 18);
      wchar_t latestBuf[256];
      swprintf(latestBuf, 256, L"Current: %.1f%% | Cores active: %d | Package power: %.1fW",
        latest.overallPct, latest.coresActive, latest.packagePowerW);
      Win98Theme::Text(ctx.dc, latestR, latestBuf,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;
    }

    auto thermalHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, thermalHeaderR, L"THERMAL",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto thermalHist = history_->ThermalHistory();
    if (!thermalHist.empty()) {
      auto latest = thermalHist.back();
      auto thermalR = Win98Theme::R(c, 10, y, 960, 18);
      COLORREF tempColor = Win98Theme::kWin98Black;
      if (latest.cpuCoreTempC > 85.0) tempColor = Win98Theme::kWin98Red;
      else if (latest.cpuCoreTempC > 75.0) tempColor = RGB(200, 100, 0);
      else if (latest.cpuCoreTempC > 65.0) tempColor = RGB(180, 140, 0);

      wchar_t thermalBuf[256];
      swprintf(thermalBuf, 256,
        L"CPU: %.0fC | Package: %.0fC | GPU: %.0fC | SSD: %.0fC | Avg: %.0fC | Max: %.0fC",
        latest.cpuCoreTempC, latest.cpuPackageTempC,
        latest.gpuTempC, latest.ssdTempC,
        history_->AvgTemp(), history_->MaxTemp());
      Win98Theme::Text(ctx.dc, thermalR, thermalBuf,
        ctx.canvas.fonts->fontRegular, tempColor,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 22;

      if (latest.cpuThrottling) {
        auto throttleR = Win98Theme::R(c, 10, y, 960, 18);
        Win98Theme::Text(ctx.dc, throttleR, L"*** CPU THROTTLING ACTIVE ***",
          ctx.canvas.fonts->fontBold, Win98Theme::kWin98Red,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += 22;
      }

      if (latest.fanCount > 0) {
        auto fanR = Win98Theme::R(c, 10, y, 960, 18);
        wchar_t fanBuf[128];
        swprintf(fanBuf, 128, L"Fans: %d | Speeds: %.0f / %.0f / %.0f RPM",
          latest.fanCount,
          latest.fanSpeeds[0], latest.fanSpeeds[1], latest.fanSpeeds[2]);
        Win98Theme::Text(ctx.dc, fanR, fanBuf,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += 22;
      }
      y += 8;
    }

    auto gpuHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, gpuHeaderR, L"GPU",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto gpuHist = history_->GpuHistory();
    if (!gpuHist.empty()) {
      auto latest = gpuHist.back();
      auto gpuR = Win98Theme::R(c, 10, y, 960, 18);
      wchar_t gpuBuf[256];
      swprintf(gpuBuf, 256, L"Usage: %.1f%% | Temp: %.0fC | Power: %.1fW | VRAM: %.1f/%.1f GB",
        latest.pct, latest.tempC, latest.powerW,
        static_cast<double>(latest.usedBytes) / (1024.0 * 1024.0 * 1024.0),
        static_cast<double>(latest.totalBytes) / (1024.0 * 1024.0 * 1024.0));
      Win98Theme::Text(ctx.dc, gpuR, gpuBuf,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;
    }

    auto storageHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, storageHeaderR, L"STORAGE",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto storageHist = history_->StorageHistory();
    if (!storageHist.empty()) {
      auto latest = storageHist.back();
      auto storageR = Win98Theme::R(c, 10, y, 960, 18);
      COLORREF storColor = Win98Theme::kWin98Black;
      if (latest.wearPct > 80.0) storColor = Win98Theme::kWin98Red;
      else if (latest.wearPct > 50.0) storColor = RGB(180, 140, 0);

      wchar_t storBuf[256];
      swprintf(storBuf, 256,
        L"Read: %.0f MB/s | Write: %.0f MB/s | IOPS: %.0f | Queue: %.1f | Active: %.1f%% | Wear: %.1f%% | Temp: %.0fC",
        latest.diskReadMBs, latest.diskWriteMBs,
        latest.iops, latest.queueDepth,
        latest.activeTimePct, latest.wearPct, latest.tempC);
      Win98Theme::Text(ctx.dc, storageR, storBuf,
        ctx.canvas.fonts->fontRegular, storColor,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;
    }

    auto alerts = history_->RecentAlerts(5);
    if (!alerts.empty()) {
      auto alertHeaderR = Win98Theme::R(c, 10, y, 480, 24);
      Win98Theme::Text(ctx.dc, alertHeaderR, L"HARDWARE ALERTS",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;

      for (const auto& a : alerts) {
        if (y > c.designH - 40) break;
        RECT rowR = Win98Theme::R(c, 10, y, 960, 18);
        COLORREF bgColor = (a.severity >= EventSeverity::High) ?
          RGB(255, 230, 200) : RGB(255, 255, 220);
        Win98Theme::Fill(ctx.dc, rowR, bgColor);
        Win98Theme::Text(ctx.dc, rowR,
          L"  " + a.component + L" " + a.type + L": " + a.description,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += 20;
      }
    }

    auto trends = history_->DetectTrends();
    if (!trends.empty()) {
      y += 10;
      auto trendHeaderR = Win98Theme::R(c, 10, y, 480, 24);
      Win98Theme::Text(ctx.dc, trendHeaderR, L"TRENDS",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;

      for (const auto& t : trends) {
        if (y > c.designH - 40) break;
        RECT rowR = Win98Theme::R(c, 10, y, 960, 18);
        COLORREF trendColor = (t.direction == L"rising") ?
          RGB(200, 100, 0) : Win98Theme::kWin98BrightGreen;
        Win98Theme::Text(ctx.dc, rowR, L"  " + t.description,
          ctx.canvas.fonts->fontRegular, trendColor,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += 18;
      }
    }
  }

private:
  const HardwareHistory* history_ = nullptr;
};

}
