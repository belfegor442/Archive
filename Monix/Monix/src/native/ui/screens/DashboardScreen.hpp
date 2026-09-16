#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/snapshot/SystemSnapshot.hpp"
#include "../../core/scram/ScramEngine2.hpp"
#include "../../core/diagnostics/DiagnosticsEngine.hpp"
#include "../../core/history/ProcessHistory.hpp"
#include "../../core/history/NetworkHistory.hpp"
#include "../../core/history/HardwareHistory.hpp"

namespace monix::ui {

class DashboardScreen : public Screen {
public:
  std::wstring Name() const override { return L"Dashboard"; }
  int Id() const override { return 0; }

  void SetSnapshot(const SystemSnapshot& snap) {
    snap_ = snap;
    hasData_ = true;
  }

  void SetScore(int score) { score_ = score; }
  void SetScramScore(int score) { scramScore_ = score; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"MONIX DASHBOARD",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    auto scoreR = Win98Theme::R(c, 10, 50, 200, 80);
    Win98Theme::Fill(ctx.dc, scoreR, Win98Theme::kWin98Black);
    Win98Theme::Win98Bevel(ctx.dc, scoreR, true);
    auto scoreInner = Win98Theme::Pad(scoreR, 8, 8, 8, 8);
    std::wstring scoreText = L"Score: " + std::to_wstring(score_);
    COLORREF scoreColor = (score_ > 70) ? Win98Theme::kWin98BrightGreen :
                          (score_ > 40) ? Win98Theme::kWin98Yellow :
                                          Win98Theme::kWin98Red;
    Win98Theme::Text(ctx.dc, scoreInner, scoreText,
      ctx.canvas.fonts->fontLarge, scoreColor,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    auto scramR = Win98Theme::R(c, 220, 50, 200, 80);
    Win98Theme::Fill(ctx.dc, scramR, Win98Theme::kWin98Black);
    Win98Theme::Win98Bevel(ctx.dc, scramR, true);
    auto scramInner = Win98Theme::Pad(scramR, 8, 8, 8, 8);
    std::wstring scramText = L"SCRAM: " + std::to_wstring(scramScore_);
    COLORREF scramColor = (scramScore_ < 30) ? Win98Theme::kWin98BrightGreen :
                          (scramScore_ < 60) ? Win98Theme::kWin98Yellow :
                                               Win98Theme::kWin98Red;
    Win98Theme::Text(ctx.dc, scramInner, scramText,
      ctx.canvas.fonts->fontLarge, scramColor,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!hasData_) {
      auto noDataR = Win98Theme::R(c, 440, 50, 300, 80);
      Win98Theme::Text(ctx.dc, noDataR, L"Waiting for data...",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    int y = 150;
    auto sectionR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, sectionR, L"SYSTEM OVERVIEW",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto cpuR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t cpuBuf[128];
    swprintf(cpuBuf, 128, L"CPU: %.1f%% | Cores: %d | Temp: %.0fC | Power: %.1fW",
      snap_.cpu.pct, snap_.cpu.coresActive,
      snap_.thermal.cpuCoreTempC, snap_.cpu.packagePowerW);
    Win98Theme::Text(ctx.dc, cpuR, cpuBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    auto ramR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t ramBuf[128];
    double ramPct = snap_.memory.UsedPct();
    swprintf(ramBuf, 128, L"RAM: %.1f%% | Used: %.1f GB / %.1f GB | Compressed: %.1f GB",
      ramPct,
      static_cast<double>(snap_.memory.usedBytes) / (1024.0 * 1024.0 * 1024.0),
      static_cast<double>(snap_.memory.totalBytes) / (1024.0 * 1024.0 * 1024.0),
      static_cast<double>(snap_.memory.compressedBytes) / (1024.0 * 1024.0 * 1024.0));
    Win98Theme::Text(ctx.dc, ramR, ramBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    if (snap_.gpu.pctValid) {
      auto gpuR = Win98Theme::R(c, 10, y, 480, 18);
      wchar_t gpuBuf[128];
      swprintf(gpuBuf, 128, L"GPU: %.1f%% | Temp: %.0fC | Power: %.1fW | VRAM: %.1f/%.1f GB",
        snap_.gpu.pct, snap_.gpu.tempC, snap_.gpu.powerW,
        static_cast<double>(snap_.gpu.usedBytes) / (1024.0 * 1024.0 * 1024.0),
        static_cast<double>(snap_.gpu.totalBytes) / (1024.0 * 1024.0 * 1024.0));
      Win98Theme::Text(ctx.dc, gpuR, gpuBuf,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 22;
    }

    y += 10;
    auto netSectionR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, netSectionR, L"NETWORK",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto netR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t netBuf[128];
    swprintf(netBuf, 128, L"Up: %.0f KB/s | Down: %.0f KB/s | RTT: %.0f ms | Conns: %d",
      snap_.network.upKbps, snap_.network.downKbps,
      snap_.network.rttMs, snap_.network.activeConns);
    Win98Theme::Text(ctx.dc, netR, netBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    auto dropR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t dropBuf[128];
    swprintf(dropBuf, 128, L"Drops: %.1f%% | Retransmit: %.1f%% | Total: %d",
      snap_.network.droppedPktPct, snap_.network.tcpRetransmitPct,
      snap_.network.totalConns);
    Win98Theme::Text(ctx.dc, dropR, dropBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto storSectionR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, storSectionR, L"STORAGE",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto storR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t storBuf[128];
    swprintf(storBuf, 128, L"Read: %.0f MB/s | Write: %.0f MB/s | IOPS: %.0f | Queue: %.1f",
      snap_.storage.diskReadMBs, snap_.storage.diskWriteMBs,
      snap_.storage.iops, snap_.storage.queueDepth);
    Win98Theme::Text(ctx.dc, storR, storBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    if (snap_.storage.totalBytes > 0) {
      auto freeR = Win98Theme::R(c, 10, y, 480, 18);
      double freePct = (static_cast<double>(snap_.storage.freeBytes) /
        static_cast<double>(snap_.storage.totalBytes)) * 100.0;
      wchar_t freeBuf[128];
      swprintf(freeBuf, 128, L"Free: %.1f%% | Wear: %.1f%% | Temp: %.0fC",
        freePct, snap_.storage.wearPct, snap_.storage.tempC);
      Win98Theme::Text(ctx.dc, freeR, freeBuf,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 22;
    }

    y += 10;
    auto procSectionR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, procSectionR, L"PROCESSES",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    auto procR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t procBuf[128];
    swprintf(procBuf, 128, L"Count: %d | Threads: %d | Handles: %d",
      snap_.processes.count, snap_.processes.threadCount,
      snap_.processes.handleCount);
    Win98Theme::Text(ctx.dc, procR, procBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    auto topR = Win98Theme::R(c, 10, y, 480, 18);
    wchar_t topBuf[128];
    swprintf(topBuf, 128, L"Top CPU: %s (%.1f%%) | Top RAM: %s (%.1f%%)",
      snap_.processes.topCpuName.c_str(), snap_.processes.topCpuPct,
      snap_.processes.topRamName.c_str(), snap_.processes.topRamPct);
    Win98Theme::Text(ctx.dc, topR, topBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

private:
  SystemSnapshot snap_ = {};
  bool hasData_ = false;
  int score_ = 100;
  int scramScore_ = 0;
};

}
