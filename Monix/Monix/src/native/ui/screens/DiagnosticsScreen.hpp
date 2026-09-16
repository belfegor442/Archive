#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/diagnostics/DiagnosticsEngine.hpp"
#include "../../core/correlation/CorrelationEngine.hpp"
#include "../../core/anomaly/AnomalyDetector.hpp"
#include "../../core/snapshot/SystemSnapshot.hpp"

namespace monix::ui {

class DiagnosticsScreen : public Screen {
public:
  std::wstring Name() const override { return L"Diagnostics"; }
  int Id() const override { return 5; }

  void SetReport(const DiagnosticReport* r) { report_ = r; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"DIAGNOSTICS",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!report_) {
      auto noDataR = Win98Theme::R(c, 10, 50, 500, 30);
      Win98Theme::Text(ctx.dc, noDataR, L"No diagnostic report.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    int y = 50;

    auto scoreR = Win98Theme::R(c, 10, y, 300, 60);
    Win98Theme::Fill(ctx.dc, scoreR, Win98Theme::kWin98Black);
    Win98Theme::Win98Bevel(ctx.dc, scoreR, true);
    auto scoreInner = Win98Theme::Pad(scoreR, 8, 8, 8, 8);
    wchar_t scoreBuf[64];
    swprintf(scoreBuf, 64, L"Score: %d/100", report_->overallScore);
    COLORREF scoreColor = (report_->overallScore > 70) ? Win98Theme::kWin98BrightGreen :
                          (report_->overallScore > 40) ? Win98Theme::kWin98Yellow :
                                                         Win98Theme::kWin98Red;
    Win98Theme::Text(ctx.dc, scoreInner, scoreBuf,
      ctx.canvas.fonts->fontLarge, scoreColor,
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    auto summaryR = Win98Theme::R(c, 320, y, 400, 60);
    wchar_t summaryBuf[256];
    swprintf(summaryBuf, 256, L"Passed: %d | Failed: %d | Warnings: %d",
      report_->passedChecks, report_->failedChecks, report_->warningChecks);
    Win98Theme::Text(ctx.dc, summaryR, summaryBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 70;

    auto catHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, catHeaderR, L"CATEGORY SCORES",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    for (const auto& [cat, score] : report_->categoryScores) {
      if (y > c.designH - 40) break;
      RECT barR = Win98Theme::R(c, 10, y, 200, 18);
      Win98Theme::Text(ctx.dc, barR, L"  " + cat,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      RECT barBg = Win98Theme::R(c, 220, y, 150, 18);
      Win98Theme::Fill(ctx.dc, barBg, Win98Theme::kWin98DarkGray);
      int fillW = static_cast<int>(150.0 * score / 100.0);
      if (fillW > 0) {
        RECT barFill = Win98Theme::R(c, 220, y, fillW, 18);
        COLORREF barColor = (score > 70) ? Win98Theme::kWin98BrightGreen :
                            (score > 40) ? Win98Theme::kWin98Yellow :
                                           Win98Theme::kWin98Red;
        Win98Theme::Fill(ctx.dc, barFill, barColor);
      }

      RECT scoreTextR = Win98Theme::R(c, 380, y, 60, 18);
      wchar_t catScoreBuf[32];
      swprintf(catScoreBuf, 32, L"%d%%", score);
      Win98Theme::Text(ctx.dc, scoreTextR, catScoreBuf,
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 22;
    }
    y += 10;

    auto checkHeaderR = Win98Theme::R(c, 10, y, 480, 24);
    Win98Theme::Text(ctx.dc, checkHeaderR, L"CHECK DETAILS",
      ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 30;

    for (const auto& check : report_->checks) {
      if (y > c.designH - 40) break;

      RECT rowR = Win98Theme::R(c, 10, y, 960, 20);
      COLORREF bgColor = check.passed ? RGB(220, 255, 220) : RGB(255, 220, 220);
      Win98Theme::Fill(ctx.dc, rowR, bgColor);

      const wchar_t* status = check.passed ? L"PASS" : L"FAIL";
      COLORREF statusColor = check.passed ? Win98Theme::kWin98BrightGreen : Win98Theme::kWin98Red;

      wchar_t checkBuf[256];
      swprintf(checkBuf, 256, L"  [%s] %s: %s",
        status, check.name.c_str(), check.detail.c_str());

      Win98Theme::Text(ctx.dc, rowR, checkBuf,
        ctx.canvas.fonts->fontRegular, statusColor,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      y += 22;

      if (!check.passed && !check.recommendations.empty()) {
        for (const auto& rec : check.recommendations) {
          if (y > c.designH - 40) break;
          RECT recR = Win98Theme::R(c, 30, y, 940, 18);
          Win98Theme::Text(ctx.dc, recR, L"  -> " + rec,
            ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
          y += 18;
        }
      }
    }
  }

private:
  const DiagnosticReport* report_ = nullptr;
};

}
