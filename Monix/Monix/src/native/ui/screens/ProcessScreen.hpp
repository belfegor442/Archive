#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/history/ProcessHistory.hpp"

namespace monix::ui {

class ProcessScreen : public Screen {
public:
  std::wstring Name() const override { return L"Processes"; }
  int Id() const override { return 2; }

  void SetHistory(const ProcessHistory* ph) { history_ = ph; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"PROCESS MONITOR",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!history_) {
      auto noDataR = Win98Theme::R(c, 10, 50, 500, 30);
      Win98Theme::Text(ctx.dc, noDataR, L"No process data.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    auto alive = history_->AliveProcesses();
    int y = 50;

    auto headerR = Win98Theme::R(c, 10, y, 960, 20);
    Win98Theme::Fill(ctx.dc, headerR, Win98Theme::kWin98Highlight);
    Win98Theme::Text(ctx.dc, headerR,
      L"  PID     NAME                    CPU%    RAM(MB)  THREADS  HANDLES  AVG_CPU",
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98HighlightText,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    std::sort(alive.begin(), alive.end(),
      [](const ProcessRecord& a, const ProcessRecord& b) {
        return a.avgCpu > b.avgCpu;
      });

    int shown = 0;
    for (const auto& proc : alive) {
      if (y > c.designH - 40 || shown >= maxVisible_) break;

      RECT rowR = Win98Theme::R(c, 10, y, 960, 20);
      COLORREF bgColor = (shown % 2 == 0) ? Win98Theme::kWin98White : Win98Theme::kWin98LightGray;
      Win98Theme::Fill(ctx.dc, rowR, bgColor);

      wchar_t line[256];
      double ramMB = static_cast<double>(proc.avgRam) / (1024.0 * 1024.0);
      swprintf(line, 256, L"  %-7d %-24s %5.1f   %8.0f   %5d    %5d    %5.1f",
        proc.pid, proc.name.c_str(), proc.avgCpu, ramMB,
        proc.history.empty() ? 0 : proc.history.back().threadCount,
        proc.history.empty() ? 0 : proc.history.back().handleCount,
        proc.avgCpu);

      COLORREF textColor = Win98Theme::kWin98Black;
      if (proc.avgCpu > 50.0) textColor = Win98Theme::kWin98Red;
      else if (proc.avgCpu > 20.0) textColor = RGB(180, 140, 0);

      Win98Theme::Text(ctx.dc, rowR, line,
        ctx.canvas.fonts->fontRegular, textColor,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

      y += 20;
      shown++;
    }

    auto trends = history_->DetectTrends();
    if (!trends.empty()) {
      y += 10;
      auto trendHeaderR = Win98Theme::R(c, 10, y, 960, 20);
      Win98Theme::Fill(ctx.dc, trendHeaderR, Win98Theme::kWin98Highlight);
      Win98Theme::Text(ctx.dc, trendHeaderR,
        L"  PROCESS TRENDS",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98HighlightText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 22;

      for (const auto& t : trends) {
        if (y > c.designH - 40 || shown >= maxVisible_ + 5) break;

        RECT rowR = Win98Theme::R(c, 10, y, 960, 20);
        Win98Theme::Fill(ctx.dc, rowR, RGB(255, 255, 220));

        wchar_t trendLine[256];
        swprintf(trendLine, 256, L"  %s: %s (slope: +%.1f%%/sample, pred: %.1f%%)",
          t.name.c_str(), t.trendType.c_str(), t.slope, t.predictedValue);
        Win98Theme::Text(ctx.dc, rowR, trendLine,
          ctx.canvas.fonts->fontRegular, RGB(180, 140, 0),
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += 20;
        shown++;
      }
    }

    wchar_t countBuf[64];
    swprintf(countBuf, 64, L"Alive: %d | Dead: %d",
      history_->AliveCount(), history_->DeadCount());
    auto countR = Win98Theme::R(c, 10, c.designH - 30, 400, 20);
    Win98Theme::Text(ctx.dc, countR, countBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  void SetMaxVisible(int max) { maxVisible_ = max; }

private:
  const ProcessHistory* history_ = nullptr;
  int maxVisible_ = 30;
};

}
