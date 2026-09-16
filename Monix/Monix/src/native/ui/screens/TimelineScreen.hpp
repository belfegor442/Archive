#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Screen.hpp"
#include "../Win98Theme.hpp"
#include "../../core/Timeline.hpp"

namespace monix::ui {

class TimelineScreen : public Screen {
public:
  std::wstring Name() const override { return L"Timeline"; }
  int Id() const override { return 1; }

  void SetTimeline(const Timeline* tl) { timeline_ = tl; }

  void Render(const ScreenContext& ctx) override {
    const auto& c = ctx.canvas;

    auto titleR = Win98Theme::R(c, 10, 10, 500, 28);
    Win98Theme::Text(ctx.dc, titleR, L"TIMELINE",
      ctx.canvas.fonts->fontTitle, Win98Theme::kWin98Black,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (!timeline_ || timeline_->Empty()) {
      auto noDataR = Win98Theme::R(c, 10, 50, 500, 30);
      Win98Theme::Text(ctx.dc, noDataR, L"No timeline data yet.",
        ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      return;
    }

    auto entries = timeline_->RecentEntries(maxVisible_);

    int y = 50;
    auto headerR = Win98Theme::R(c, 10, y, 960, 20);
    Win98Theme::Fill(ctx.dc, headerR, Win98Theme::kWin98Highlight);
    Win98Theme::Text(ctx.dc, headerR,
      L"  TIME          TYPE          CATEGORY    SEVERITY  DETAIL",
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98HighlightText,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += 22;

    for (const auto& e : entries) {
      if (y > c.designH - 40) break;

      RECT rowR = Win98Theme::R(c, 10, y, 960, 20);

      COLORREF bgColor = Win98Theme::kWin98White;
      if (e.severity == EventSeverity::Critical) bgColor = RGB(255, 200, 200);
      else if (e.severity == EventSeverity::High) bgColor = RGB(255, 230, 200);
      else if (e.severity == EventSeverity::Medium) bgColor = RGB(255, 255, 220);
      Win98Theme::Fill(ctx.dc, rowR, bgColor);

      wchar_t line[256];
      wchar_t timeBuf[32];
      uint64_t sec = e.timestampNs / 1000000000ULL;
      uint64_t ms = (e.timestampNs % 1000000000ULL) / 1000000ULL;
      swprintf(timeBuf, 32, L"%llu.%03llu", sec, ms);

      const wchar_t* typeStr = L"Unknown";
      switch (e.type) {
        case TimelineEntry::Type::Snapshot: typeStr = L"Snapshot"; break;
        case TimelineEntry::Type::Event: typeStr = L"Event"; break;
        case TimelineEntry::Type::Finding: typeStr = L"Finding"; break;
        case TimelineEntry::Type::Warning: typeStr = L"Warning"; break;
        case TimelineEntry::Type::Error: typeStr = L"Error"; break;
        case TimelineEntry::Type::UserAction: typeStr = L"UserAction"; break;
        case TimelineEntry::Type::SessionStart: typeStr = L"SessionStart"; break;
        case TimelineEntry::Type::SessionEnd: typeStr = L"SessionEnd"; break;
        case TimelineEntry::Type::Diagnostic: typeStr = L"Diagnostic"; break;
      }

      const wchar_t* catStr = L"System";
      switch (e.category) {
        case EventCategory::Hardware: catStr = L"Hardware"; break;
        case EventCategory::Network: catStr = L"Network"; break;
        case EventCategory::Thermal: catStr = L"Thermal"; break;
        case EventCategory::Process: catStr = L"Process"; break;
        case EventCategory::Storage: catStr = L"Storage"; break;
        case EventCategory::Security: catStr = L"Security"; break;
        case EventCategory::Power: catStr = L"Power"; break;
        case EventCategory::Scram: catStr = L"SCRAM"; break;
        case EventCategory::User: catStr = L"User"; break;
        case EventCategory::Diagnostic: catStr = L"Diagnostic"; break;
      }

      const wchar_t* sevStr = L"Info";
      COLORREF sevColor = Win98Theme::kWin98Black;
      switch (e.severity) {
        case EventSeverity::Info: sevStr = L"Info"; sevColor = Win98Theme::kWin98Black; break;
        case EventSeverity::Low: sevStr = L"Low"; sevColor = Win98Theme::kWin98DarkGray; break;
        case EventSeverity::Medium: sevStr = L"Med"; sevColor = RGB(180, 140, 0); break;
        case EventSeverity::High: sevStr = L"High"; sevColor = RGB(200, 100, 0); break;
        case EventSeverity::Critical: sevStr = L"CRIT"; sevColor = Win98Theme::kWin98Red; break;
      }

      swprintf(line, 256, L"  %-14s %-12s %-10s %-8s %s",
        timeBuf, typeStr, catStr, sevStr, e.label.c_str());

      Win98Theme::Text(ctx.dc, rowR, line,
        ctx.canvas.fonts->fontRegular, sevColor,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

      y += 20;
    }

    wchar_t countBuf[64];
    swprintf(countBuf, 64, L"Showing %d of %d entries",
      static_cast<int>(entries.size()), timeline_->TotalEntries());
    auto countR = Win98Theme::R(c, 10, c.designH - 30, 400, 20);
    Win98Theme::Text(ctx.dc, countR, countBuf,
      ctx.canvas.fonts->fontRegular, Win98Theme::kWin98DarkGray,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  void SetMaxVisible(int max) { maxVisible_ = max; }

private:
  const Timeline* timeline_ = nullptr;
  int maxVisible_ = 40;
};

}
