#include "ui/render/panels/settings/RenderSettings.hpp"

void MonixApp::DrawSettingsView(HDC dc, const RECT& clientRect) {
  using namespace monix::renderer_vk;
  const RECT outer = ContentRect(clientRect);

  const int subTabW = 86;
  const int subTabH = 32;
  const int subTabGap = 4;
  const int subTabY = outer.top;

  RECT generalTab { outer.left, subTabY, outer.left + subTabW, subTabY + subTabH };
  RECT displayTab { outer.left + subTabW + subTabGap, subTabY, outer.left + subTabW * 2 + subTabGap, subTabY + subTabH };
  RECT loggingTab { outer.left + subTabW * 2 + subTabGap * 2, subTabY, outer.left + subTabW * 3 + subTabGap * 2, subTabY + subTabH };
  RECT shadersTab { outer.left + subTabW * 3 + subTabGap * 3, subTabY, outer.left + subTabW * 4 + subTabGap * 3, subTabY + subTabH };
  RECT systemTab { outer.left + subTabW * 4 + subTabGap * 4, subTabY, outer.left + subTabW * 5 + subTabGap * 4, subTabY + subTabH };
  RECT perfTab { outer.left + subTabW * 5 + subTabGap * 5, subTabY, outer.left + subTabW * 6 + subTabGap * 5, subTabY + subTabH };

  const bool showGeneral = state_.shaderUi.panel.subTab == SettingsSubTab::General;
  const bool showDisplay = state_.shaderUi.panel.subTab == SettingsSubTab::Display;
  const bool showLogging = state_.shaderUi.panel.subTab == SettingsSubTab::Logging;
  const bool showShaders = state_.shaderUi.panel.subTab == SettingsSubTab::Shaders;
  const bool showSystem = state_.shaderUi.panel.subTab == SettingsSubTab::System;
  const bool showPerf = state_.shaderUi.panel.subTab == SettingsSubTab::Performance;

  FillSolid(dc, generalTab, showGeneral ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, generalTab, ResolveColor(showGeneral ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, generalTab, L"GENERAL", showGeneral ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, displayTab, showDisplay ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, displayTab, ResolveColor(showDisplay ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, displayTab, L"DISPLAY", showDisplay ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, loggingTab, showLogging ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, loggingTab, ResolveColor(showLogging ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, loggingTab, L"LOGGING", showLogging ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, shadersTab, showShaders ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, shadersTab, ResolveColor(showShaders ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, shadersTab, L"SHADERS", showShaders ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, systemTab, showSystem ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, systemTab, ResolveColor(showSystem ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, systemTab, L"SYSTEM", showSystem ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, perfTab, showPerf ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, perfTab, ResolveColor(showPerf ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, perfTab, L"PERF", showPerf ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  const RECT contentArea { outer.left, outer.top + subTabH + 8, outer.right, outer.bottom };

  if (showGeneral) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    RECT rightPanel = contentArea;
    rightPanel.left = leftPanel.right + 14;

    DrawPanel(dc, leftPanel, L"RUNTIME", ColorRole::Security, smallFont_);
    DrawPanel(dc, rightPanel, L"UI PREFERENCES", ColorRole::Engine, smallFont_);

    const auto rows = BuildSettingActionRects(clientRect);
    // DEFENSIVE CLAMP: Primary index mutation happens in HandleSettingsClick/HandleSettingsKey (AppWindowProc.cpp:217,551-557).
    // This clamp is a safety net to ensure selectedIndex stays in bounds if the settings list changes between frames.
    if (!rows.empty()) {
      state_.shaderUi.selectedIndex = std::clamp(state_.shaderUi.selectedIndex, 0, static_cast<int>(rows.size()) - 1);
    }
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.shaderUi.selectedIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_);
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(RuntimeSettings(), ColorRole::Security, ColorRole::White);
    drawRows(IntroAnimSettings(), ColorRole::Security, ColorRole::White);
    drawRows(GeneralExtendedSettings(), ColorRole::Engine, ColorRole::White);

  } else if (showDisplay) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(220, static_cast<int>((contentArea.bottom - contentArea.top) * 0.30));
    RECT rightMid = contentArea;
    rightMid.left = leftPanel.right + 14;
    rightMid.top = rightTop.bottom + 14;
    rightMid.bottom = rightTop.bottom + 14 + std::max(120, static_cast<int>((contentArea.bottom - contentArea.top) * 0.16));
    RECT rightMid2 = contentArea;
    rightMid2.left = leftPanel.right + 14;
    rightMid2.top = rightMid.bottom + 14;
    rightMid2.bottom = rightMid.bottom + 14 + std::max(100, static_cast<int>((contentArea.bottom - contentArea.top) * 0.14));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightMid2.bottom + 14;
    RECT rightBottom2 = contentArea;
    rightBottom2.left = leftPanel.right + 14;
    rightBottom2.top = rightBottom.bottom + 14;

    DrawPanel(dc, rightBottom2, L"BORDER OVERLAY", ColorRole::Engine, smallFont_);

    const auto rows = BuildSettingActionRects(clientRect);
    // DEFENSIVE CLAMP: Primary index mutation happens in HandleSettingsClick/HandleSettingsKey (AppWindowProc.cpp).
    if (!rows.empty()) {
      state_.shaderUi.selectedIndex = std::clamp(state_.shaderUi.selectedIndex, 0, static_cast<int>(rows.size()) - 1);
    }
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.shaderUi.selectedIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_);
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(BorderSettings(), ColorRole::Engine, ColorRole::Engine);

  } else if (showLogging) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    RECT rightPanel = contentArea;
    rightPanel.left = leftPanel.right + 14;

    DrawPanel(dc, leftPanel, L"LOGGING CONFIGURATION", ColorRole::Engine, smallFont_);
    DrawPanel(dc, rightPanel, L"LOG OUTPUT INFO", ColorRole::Accent, smallFont_);

    const auto rows = BuildSettingActionRects(clientRect);
    // DEFENSIVE CLAMP: Primary index mutation happens in HandleSettingsClick/HandleSettingsKey (AppWindowProc.cpp).
    if (!rows.empty()) {
      state_.shaderUi.selectedIndex = std::clamp(state_.shaderUi.selectedIndex, 0, static_cast<int>(rows.size()) - 1);
    }
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.shaderUi.selectedIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_);
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(LoggingSettings(), ColorRole::Engine, ColorRole::Engine);

    const std::wstring logInfo =
      L"Log buffer: " + std::to_wstring(config_.logBufferSize) + L" entries\n"
      L"Visible lines: " + std::to_wstring(config_.logVisibleLines) + L"\n"
      L"Flush interval: " + std::to_wstring(config_.logFlushIntervalMs) + L" ms\n"
      L"Retention: " + std::to_wstring(config_.logRetentionDays) + L" days\n"
      L"Deduplication: " + std::wstring(config_.logDeduplicate ? L"ON" : L"OFF") + L"\n"
      L"Format: " + std::wstring(config_.logJsonEnabled ? L"JSON Lines" : L"Plain text") + L"\n"
      L"Live logs: " + std::wstring(config_.pauseLiveLogs ? L"PAUSED" : L"STREAMING") + L"\n\n"
      L"Log files rotate daily and are compressed\n"
      L"after the configured retention period.";
    DrawTextRect(dc, RECT { rightPanel.left + 18, rightPanel.top + 56, rightPanel.right - 18, rightPanel.bottom - 18 }, logInfo, ColorRole::Primary, smallFont_, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
  } else if (showShaders) {
    const int listW = static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    const int detailW = contentArea.right - contentArea.left - listW - 14;

    RECT filterBar = contentArea;
    filterBar.bottom = filterBar.top + 34;

    RECT listPanel = contentArea;
    listPanel.top = filterBar.bottom + 6;
    listPanel.right = contentArea.left + listW;

    RECT detailPanel = contentArea;
    detailPanel.left = listPanel.right + 14;
    detailPanel.top = filterBar.bottom + 6;

    const int filterBtnW = 80;
    const int filterBtnH = 26;
    const int filterGap = 8;
    int fx = contentArea.left + 14;

    auto drawFilterTab = [&](const wchar_t* label, ShaderLanguageFilter tabFilter) {
      RECT btn { fx, filterBar.top + 4, fx + filterBtnW, filterBar.top + 4 + filterBtnH };
      const bool active = state_.shaderUi.panel.filter == tabFilter;
      FillSolid(dc, btn, active ? RGB(22, 86, 38) : RGB(8, 12, 8));
      DrawRectOutline(dc, btn, ResolveColor(active ? ColorRole::Primary : ColorRole::Accent));
      DrawTextRect(dc, btn, label, active ? ColorRole::White : ColorRole::Primary, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      fx += filterBtnW + filterGap;
    };

    drawFilterTab(L"GLSL", ShaderLanguageFilter::GLSL);
    drawFilterTab(L"SLANG", ShaderLanguageFilter::Slang);
    drawFilterTab(L"PRESET", ShaderLanguageFilter::Preset);

    RECT favBtn { fx, filterBar.top + 4, fx + filterBtnW + 20, filterBar.top + 4 + filterBtnH };
    FillSolid(dc, favBtn, state_.shaderUi.showFavoritesOnly ? RGB(22, 86, 38) : RGB(8, 12, 8));
    DrawRectOutline(dc, favBtn, ResolveColor(state_.shaderUi.showFavoritesOnly ? ColorRole::Warning : ColorRole::Accent));
    DrawTextRect(dc, favBtn, L"\x2605 Favorites", state_.shaderUi.showFavoritesOnly ? ColorRole::Warning : ColorRole::Dim, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    fx += filterBtnW + 20 + filterGap;

    RECT searchBox { fx, filterBar.top + 4, contentArea.left + listW - 14, filterBar.top + 4 + filterBtnH };
    FillSolid(dc, searchBox, state_.shaderUi.searchFocused ? RGB(12, 18, 12) : RGB(7, 12, 8));
    DrawRectOutline(dc, searchBox, ResolveColor(state_.shaderUi.searchFocused ? ColorRole::Primary : ColorRole::Accent));
    std::wstring searchText = state_.shaderUi.searchText.empty() && !state_.shaderUi.searchFocused
      ? L"Search shaders..." : state_.shaderUi.searchText;
    if (state_.shaderUi.searchFocused) searchText += L"_";
    DrawTextRect(dc, searchBox, searchText, state_.shaderUi.searchText.empty() ? ColorRole::Dim : ColorRole::White, smallFont_, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_EDITCONTROL);

    DrawPanel(dc, listPanel, L"SHADER LIBRARY", ColorRole::Security, smallFont_);
    DrawPanel(dc, detailPanel, L"SHADER DETAIL", ColorRole::Primary, smallFont_);

    const int rowH = std::max(26, smallLineHeight_ + 4);
    const int innerPad = 18;

    const auto categories = shaderBrowserPanel_ ? shaderBrowserPanel_->categoryGroups() : std::vector<monix::renderer_vk::ShaderCategoryGroup>{};

    int y = listPanel.top + 48;
    int globalRow = 0;

    for (const auto& cat : categories) {
      if (y + rowH > listPanel.bottom - 50) break;

      RECT catRect { listPanel.left + innerPad, y, listPanel.right - innerPad, y + rowH };
      FillSolid(dc, catRect, RGB(10, 18, 12));
      const std::wstring catLabel = L"\x25BC " + Utf8ToWide(cat.name);
      DrawTextLine(dc, catRect.left + 6, catRect.top + 2, catRect.right - catRect.left - 12, catLabel, ColorRole::Dim, smallFont_);
      y += rowH + 2;

      for (const size_t libIdx : cat.entryIndices) {
        if (globalRow >= state_.shaderUi.panel.scrollOffset) {
          if (y + rowH > listPanel.bottom - 50) break;

          const bool selected = globalRow == state_.shaderUi.panel.selectedShaderIndex;
          RECT rowRect { listPanel.left + innerPad + 12, y, listPanel.right - innerPad, y + rowH };
          FillSolid(dc, rowRect, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
          DrawRectOutline(dc, rowRect, ResolveColor(selected ? ColorRole::Primary : ColorRole::Accent));

          const auto* entry = shaderLibrary_ ? shaderLibrary_->entry(libIdx) : nullptr;
          if (entry) {
            const std::wstring indicator = [s = entry->status]() -> std::wstring {
              switch (s) {
              case monix::renderer_vk::ShaderEntryStatus::Unknown:   return L"\x25CB";
              case monix::renderer_vk::ShaderEntryStatus::Pending:   return L"\x25CB";
              case monix::renderer_vk::ShaderEntryStatus::Compiling: return L"\x25CF";
              case monix::renderer_vk::ShaderEntryStatus::Compiled:  return L"\x25CF";
              case monix::renderer_vk::ShaderEntryStatus::Active:    return L"\x25D4";
              case monix::renderer_vk::ShaderEntryStatus::Error:     return L"\x25CF";
              }
              return L"\x25CB";
            }();
            const ColorRole indicatorColor = [s = entry->status]() -> ColorRole {
              switch (s) {
              case monix::renderer_vk::ShaderEntryStatus::Unknown:   return ColorRole::Dim;
              case monix::renderer_vk::ShaderEntryStatus::Pending:   return ColorRole::Dim;
              case monix::renderer_vk::ShaderEntryStatus::Compiling: return ColorRole::Warning;
              case monix::renderer_vk::ShaderEntryStatus::Compiled:  return ColorRole::Success;
              case monix::renderer_vk::ShaderEntryStatus::Active:    return ColorRole::Primary;
              case monix::renderer_vk::ShaderEntryStatus::Error:     return ColorRole::Error;
              }
              return ColorRole::Dim;
            }();

            DrawTextLine(dc, rowRect.left + 6, rowRect.top + 2, 20, indicator, indicatorColor, smallFont_);
            DrawTextLine(dc, rowRect.left + 28, rowRect.top + 2, rowRect.right - rowRect.left - 56, Utf8ToWide(entry->name), selected ? ColorRole::White : ColorRole::Dim, smallFont_);
            if (shaderBrowserPanel_ && shaderBrowserPanel_->isFavorite(libIdx)) {
              DrawTextLine(dc, rowRect.right - 24, rowRect.top + 2, 18, L"\x2605", ColorRole::Warning, smallFont_);
            }
          }
          y += rowH + 2;
        }
        ++globalRow;
      }
    }

    RECT actionBar { listPanel.left, listPanel.bottom - 42, listPanel.right, listPanel.bottom - 8 };
    FillSolid(dc, actionBar, RGB(10, 14, 10));

    const int actionBtnW = 76;
    const int actionBtnH = 28;
    const int actionGap = 6;
    int abx = actionBar.left + innerPad;

    auto drawActionBtn = [&](const wchar_t* label) {
      RECT btn { abx, actionBar.top + 2, abx + actionBtnW, actionBar.top + 2 + actionBtnH };
      FillSolid(dc, btn, RGB(22, 86, 38));
      DrawRectOutline(dc, btn, ResolveColor(ColorRole::Primary));
      DrawTextRect(dc, btn, label, ColorRole::White, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      abx += actionBtnW + actionGap;
    };

    drawActionBtn(L"IMPORT");
    drawActionBtn(L"DELETE");
    drawActionBtn(L"REFRESH");
    drawActionBtn(L"OPEN");

    const int totalFiltered = shaderBrowserPanel_ ? shaderBrowserPanel_->filteredShaderCount() : 0;
    const auto summary = shaderBrowserPanel_ ? shaderBrowserPanel_->statusSummary() : std::string();

    RECT summaryBar { listPanel.left + innerPad, actionBar.top - 20, listPanel.right - innerPad, actionBar.top - 2 };
    DrawTextLine(dc, summaryBar.left, summaryBar.top, summaryBar.right - summaryBar.left, Utf8ToWide(summary), ColorRole::Dim, smallFont_);

    if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
      const auto detail = shaderBrowserPanel_->detailInfo();
      int dy = detailPanel.top + 56;
      const int labelW = 130;
      const int lineH = smallLineHeight_ + 2;

      auto drawDetailLine = [&](const wchar_t* label, const std::wstring& value) {
        if (dy + lineH > detailPanel.bottom - 180) return;
        DrawTextLine(dc, detailPanel.left + 18, dy, labelW, label, ColorRole::Dim, smallFont_);
        DrawTextLine(dc, detailPanel.left + 18 + labelW, dy, detailPanel.right - detailPanel.left - 36 - labelW, value, ColorRole::White, smallFont_);
        dy += lineH;
      };

      auto drawPerfLine = [&](const wchar_t* label, double ms) {
        if (dy + lineH > detailPanel.bottom - 180) return;
        DrawTextLine(dc, detailPanel.left + 18, dy, labelW, label, ColorRole::Dim, smallFont_);
        std::wstring val = (ms > 0.0) ? (std::to_wstring(ms).substr(0, 6) + L" ms") : L"N/A";
        DrawTextLine(dc, detailPanel.left + 18 + labelW, dy, detailPanel.right - detailPanel.left - 36 - labelW, val, ColorRole::White, smallFont_);
        dy += lineH;
      };

      drawDetailLine(L"Name:", Utf8ToWide(detail.name));
      drawDetailLine(L"Path:", Utf8ToWide(detail.path));
      drawDetailLine(L"Language:", Utf8ToWide(detail.language));
      drawDetailLine(L"Category:", Utf8ToWide(detail.category));
      drawDetailLine(L"Extension:", Utf8ToWide(detail.extension));
      drawDetailLine(L"Status:", Utf8ToWide(shaderEntryStatusName(detail.status)));
      drawDetailLine(L"Source:", detail.isActive ? L"Active" : L"Inactive");
      drawDetailLine(L"Hash:", std::to_wstring(detail.contentHash));
      drawDetailLine(L"File Size:", std::to_wstring(detail.fileSize) + L" bytes");
      drawDetailLine(L"Active:", detail.isActive ? L"Yes" : L"No");
      drawDetailLine(L"Compiled:", detail.hasCompiledModule ? L"Yes" : L"No");
      drawDetailLine(L"SPIR-V:", detail.spirvSizeBytes > 0 ? (std::to_wstring(detail.spirvSizeBytes) + L" bytes") : L"N/A");
      drawDetailLine(L"Cache:", detail.lastCacheStatus.empty() ? L"N/A" : Utf8ToWide(detail.lastCacheStatus));

      dy += 6;
      DrawTextLine(dc, detailPanel.left + 18, dy, detailPanel.right - detailPanel.left - 36, L"PERFORMANCE", ColorRole::Security, smallFont_);
      dy += lineH;
      drawPerfLine(L"Compile:", detail.compileDurationMs);
      drawPerfLine(L"Validation:", detail.validationDurationMs);
      drawPerfLine(L"Pipeline:", detail.pipelineDurationMs);
      drawPerfLine(L"Total:", detail.totalActivationMs);

      RECT loadBtn { detailPanel.left + 18, detailPanel.bottom - 52, detailPanel.left + 120, detailPanel.bottom - 22 };
      RECT reloadBtn { detailPanel.left + 130, detailPanel.bottom - 52, detailPanel.left + 240, detailPanel.bottom - 22 };
      FillSolid(dc, loadBtn, RGB(22, 86, 38));
      DrawRectOutline(dc, loadBtn, ResolveColor(ColorRole::Primary));
      DrawTextRect(dc, loadBtn, L"LOAD", ColorRole::White, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      FillSolid(dc, reloadBtn, RGB(22, 86, 38));
      DrawRectOutline(dc, reloadBtn, ResolveColor(ColorRole::Primary));
      DrawTextRect(dc, reloadBtn, L"RELOAD", ColorRole::White, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      if (!detail.firstError.empty()) {
        RECT errPanel { detailPanel.left, detailPanel.bottom - 280, detailPanel.right, detailPanel.bottom - 180 };
        DrawPanel(dc, errPanel, L"ERROR", ColorRole::Error, smallFont_);
        int ey = errPanel.top + 56;
        const int errPad = 18;
        DrawTextLine(dc, errPanel.left + errPad, ey, errPanel.right - errPanel.left - errPad * 2, Utf8ToWide(detail.firstError), ColorRole::Error, smallFont_);
      }
    } else {
      DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 56, detailPanel.right - detailPanel.left - 36, L"Select a shader to view details.", ColorRole::Dim, smallFont_);

      RECT loadBtn { detailPanel.left + 18, detailPanel.bottom - 52, detailPanel.left + 120, detailPanel.bottom - 22 };
      RECT reloadBtn { detailPanel.left + 130, detailPanel.bottom - 52, detailPanel.left + 240, detailPanel.bottom - 22 };
      FillSolid(dc, loadBtn, RGB(12, 20, 12));
      DrawRectOutline(dc, loadBtn, ResolveColor(ColorRole::Accent));
      DrawTextRect(dc, loadBtn, L"LOAD", ColorRole::Dim, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      FillSolid(dc, reloadBtn, RGB(12, 20, 12));
      DrawRectOutline(dc, reloadBtn, ResolveColor(ColorRole::Accent));
      DrawTextRect(dc, reloadBtn, L"RELOAD", ColorRole::Dim, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    RECT cachePanel { detailPanel.left, detailPanel.bottom - 170, detailPanel.right, detailPanel.bottom - 58 };
    {
      DrawPanel(dc, cachePanel, L"CACHE / DIAGNOSTICS", ColorRole::Engine, smallFont_);
      int cy = cachePanel.top + 56;
      const int cLabelW = 110;
      const int cLineH = smallLineHeight_ + 2;

      if (shaderBrowserPanel_) {
        auto cs = shaderBrowserPanel_->cacheStats();
        auto drawCacheLine = [&](const wchar_t* label, const std::wstring& value) {
          if (cy + cLineH > cachePanel.bottom - 8) return;
          DrawTextLine(dc, cachePanel.left + 18, cy, cLabelW, label, ColorRole::Dim, smallFont_);
          DrawTextLine(dc, cachePanel.left + 18 + cLabelW, cy, cachePanel.right - cachePanel.left - 36 - cLabelW, value, ColorRole::White, smallFont_);
          cy += cLineH;
        };

        drawCacheLine(L"Hits:", std::to_wstring(cs.hits));
        drawCacheLine(L"Misses:", std::to_wstring(cs.misses));
        double rate = (cs.hits + cs.misses) > 0 ? (100.0 * cs.hits / (cs.hits + cs.misses)) : 0.0;
        drawCacheLine(L"Hit Rate:", (std::to_wstring(rate).substr(0, 5) + L"%"));
        drawCacheLine(L"Corrupted:", std::to_wstring(cs.corrupted));
        drawCacheLine(L"Entries:", std::to_wstring(cs.totalEntries));
        double mb = cs.totalSizeBytes / (1024.0 * 1024.0);
        drawCacheLine(L"Size:", (std::to_wstring(mb).substr(0, 6) + L" MB"));
        double limMB = cs.memoryLimitBytes / (1024.0 * 1024.0);
        drawCacheLine(L"Limit:", (std::to_wstring(limMB).substr(0, 6) + L" MB"));

        cy += 4;
        auto diag = shaderBrowserPanel_->rendererDiagnostics();
        drawCacheLine(L"GPU:", diag.gpuVendor.empty() ? L"N/A" : Utf8ToWide(diag.gpuVendor));
        drawCacheLine(L"Renderer:", diag.gpuRenderer.empty() ? L"N/A" : Utf8ToWide(diag.gpuRenderer));
        drawCacheLine(L"API:", diag.apiVersion.empty() ? L"N/A" : Utf8ToWide(diag.apiVersion));
        drawCacheLine(L"Validation:", diag.validationEnabled ? L"Enabled" : L"Disabled");
      }
    }

    auto hre = shaderBrowserPanel_ ? shaderBrowserPanel_->lastHotReloadEvent() : monix::renderer_vk::ShaderBrowserPanel::HotReloadEvent{};
    if (hre.active) {
      RECT hotRect { contentArea.left + 14, contentArea.bottom - 24, contentArea.left + 250, contentArea.bottom - 4 };
      const bool ok = hre.status == "Active" || hre.status == "Compiled";
      DrawTextLine(dc, hotRect.left, hotRect.top, hotRect.right - hotRect.left,
        ok ? L"RELOADED" : L"RELOAD FAILED", ok ? ColorRole::Success : ColorRole::Error, smallFont_);
    }

    const auto& errHist = shaderBrowserPanel_ ? shaderBrowserPanel_->errorHistory() : std::deque<monix::renderer_vk::ErrorHistoryEntry>{};
    if (!errHist.empty()) {
      RECT histPanel { listPanel.left, listPanel.bottom - 180, listPanel.right, listPanel.bottom - 46 };
      DrawPanel(dc, histPanel, L"ERROR HISTORY", ColorRole::Error, smallFont_);
      int hy = histPanel.top + 56;
      const int hLineH = smallLineHeight_ + 2;
      int showCount = std::min(static_cast<int>(errHist.size()), 4);
      for (int i = showCount - 1; i >= 0; --i) {
        if (hy + hLineH > histPanel.bottom - 8) break;
        const auto& e = errHist[i];
        std::wstring line = Utf8ToWide(e.shader) + L" [" + Utf8ToWide(e.stage) + L"] " + Utf8ToWide(e.error);
        DrawTextLine(dc, histPanel.left + 18, hy, histPanel.right - histPanel.left - 36, line, ColorRole::Error, smallFont_);
        hy += hLineH;
      }
    }
  } else if (showSystem) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(320, static_cast<int>((contentArea.bottom - contentArea.top) * 0.45));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightTop.bottom + 14;

    DrawPanel(dc, leftPanel, L"THEME & APPEARANCE", ColorRole::Primary, smallFont_);
    DrawPanel(dc, rightTop, L"WINDOW & SYSTEM", ColorRole::Security, smallFont_);
    DrawPanel(dc, rightBottom, L"HOTKEYS", ColorRole::Warning, smallFont_);

    const auto rows = BuildSettingActionRects(clientRect);
    // DEFENSIVE CLAMP: Primary index mutation happens in HandleSettingsClick/HandleSettingsKey (AppWindowProc.cpp).
    if (!rows.empty()) {
      state_.shaderUi.selectedIndex = std::clamp(state_.shaderUi.selectedIndex, 0, static_cast<int>(rows.size()) - 1);
    }
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.shaderUi.selectedIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_);
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(ThemeSettings(), ColorRole::Primary, ColorRole::White);
    drawRows(WindowStartupSettings(), ColorRole::Security, ColorRole::White);
    drawRows(SystemExtendedSettings(), ColorRole::Security, ColorRole::White);
    drawRows(HotkeySettings(), ColorRole::Warning, ColorRole::Warning);
  } else if (showPerf) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);

    DrawPanel(dc, leftPanel, L"PERFORMANCE", ColorRole::Thermal, smallFont_);

    const auto rows = BuildSettingActionRects(clientRect);
    // DEFENSIVE CLAMP: Primary index mutation happens in HandleSettingsClick/HandleSettingsKey (AppWindowProc.cpp).
    if (!rows.empty()) {
      state_.shaderUi.selectedIndex = std::clamp(state_.shaderUi.selectedIndex, 0, static_cast<int>(rows.size()) - 1);
    }
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.shaderUi.selectedIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_);
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(PerformanceSettings(), ColorRole::Thermal, ColorRole::Thermal);
  }
}
