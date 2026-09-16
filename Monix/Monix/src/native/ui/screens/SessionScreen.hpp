#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/SessionManager.hpp"
#include "../../storage/SessionPersistence.hpp"

namespace monix::ui {

class SessionScreen : public Screen {
public:
  std::wstring Name() const override { return L"Sessions"; }
  int Id() const override { return 6; }

  void SetSessionManager(const SessionManager* sm) { sessions_ = sm; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"SESSIONS",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!sessions_) {
      auto noDataR = Win98Theme::R(c, 10, 50, 500, 30);
      Win98Theme::Text(ctx.dc, noDataR, L"No session manager.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    int y = 50;

    if (sessions_->HasActiveSession()) {
      auto activeR = Win98Theme::R(c, 10, y, 480, 24);
      Win98Theme::Text(ctx.dc, activeR, L"ACTIVE SESSION",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98BrightGreen,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;

      const auto* session = sessions_->CurrentSession();
      if (session) {
        auto infoR = Win98Theme::R(c, 10, y, 960, 18);
        wchar_t infoBuf[256];
        swprintf(infoBuf, 256, L"ID: %s | Name: %s",
          session->metadata.id.c_str(), session->metadata.name.c_str());
        Win98Theme::Text(ctx.dc, infoR, infoBuf,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += 22;

        auto statsR = Win98Theme::R(c, 10, y, 960, 18);
        wchar_t statsBuf[256];
        swprintf(statsBuf, 256,
          L"Snapshots: %d | Events: %d | Findings: %d | Peak Risk: %d",
          session->metadata.totalSnapshots, session->metadata.totalEvents,
          session->metadata.totalFindings, session->metadata.peakRiskScore);
        Win98Theme::Text(ctx.dc, statsR, statsBuf,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += 22;

        auto errR = Win98Theme::R(c, 10, y, 960, 18);
        wchar_t errBuf[256];
        swprintf(errBuf, 256, L"Errors: %d | Warnings: %d | Critical: %d | Avg CPU: %.1f%% | Avg RAM: %.1f%%",
          session->errorCount, session->warningCount, session->criticalCount,
          session->avgCpu, session->avgRamPct);
        Win98Theme::Text(ctx.dc, errR, errBuf,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        y += 22;

        if (!session->metadata.notes.empty()) {
          auto notesR = Win98Theme::R(c, 10, y, 960, 18);
          Win98Theme::Text(ctx.dc, notesR, L"Notes: " + session->metadata.notes,
            ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
          y += 22;
        }
      }
    } else {
      auto noActiveR = Win98Theme::R(c, 10, y, 500, 20);
      Win98Theme::Text(ctx.dc, noActiveR, L"No active session.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;
    }

    auto history = sessions_->History();
    if (!history.empty()) {
      y += 10;
      auto histHeaderR = Win98Theme::R(c, 10, y, 480, 24);
      Win98Theme::Text(ctx.dc, histHeaderR, L"SESSION HISTORY (" + std::to_wstring(history.size()) + L")",
        ctx.canvas.fonts->fontBold, Win98Theme::kWin98Black,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 30;

      auto headerR = Win98Theme::R(c, 10, y, 960, 20);
      Win98Theme::Fill(ctx.dc, headerR, Win98Theme::kWin98Highlight);
      Win98Theme::Text(ctx.dc, headerR,
        L"  ID                           NAME              SNAP  EVT  FIND  RISK",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98HighlightText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      y += 22;

      for (const auto& h : history) {
        if (y > c.designH - 40) break;
        RECT rowR = Win98Theme::R(c, 10, y, 960, 20);
        COLORREF bgColor = (h.peakRiskScore > 50) ? RGB(255, 230, 200) : Win98Theme::kWin98White;
        Win98Theme::Fill(ctx.dc, rowR, bgColor);

        wchar_t line[256];
        swprintf(line, 256, L"  %-28s %-18s %4d %4d %4d  %4d",
          h.id.c_str(), h.name.c_str(),
          h.totalSnapshots, h.totalEvents, h.totalFindings, h.peakRiskScore);
        Win98Theme::Text(ctx.dc, rowR, line,
          ctx.canvas.fonts->fontRegular, Win98Theme::kWin98Black,
          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        y += 20;
      }
    }
  }

private:
  const SessionManager* sessions_ = nullptr;
};

}
