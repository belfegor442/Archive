#include "MonixApp.hpp"
#include "settings/SettingGroups.hpp"
#include "settings/AdjustSetting.hpp"
#include "ui/AppStateGroups.hpp"
#include "ui/CoreMonitorTheme.hpp"
#include "ui/Win98Theme.hpp"
#include "app/bootstrap/AppConstants.hpp"
#include "updater/AutoUpdater.hpp"
#include "updater/Version.hpp"

#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>

using namespace monix;

static std::mutex s_updateInfoMutex;
static monix::updater::UpdateInfo s_pendingUpdateInfo;
static bool s_updateInfoReady = false;

bool MonixApp::HitTestTabs(const RECT& clientRect, POINT point, Tab& tab) const {
  const auto rects = TabRects(clientRect);
  for (std::size_t index = 0; index < rects.size(); ++index) {
    if (PtInRect(&rects[index], point)) {
      tab = static_cast<Tab>(index);
      return true;
    }
  }
  return false;
}

int MonixApp::VisibleLogLines(const RECT& logRect) const {
  const int available = std::max(0, static_cast<int>(logRect.bottom - logRect.top));
  return std::max(1, available / std::max(18, logLineHeight_ + 2));
}

int MonixApp::VisibleTaskRows(const RECT& tableRect) const {
  const int rowHeight = std::max(38, bodyLineHeight_ + 14);
  return std::max(4, static_cast<int>(tableRect.bottom - tableRect.top - 56) / rowHeight);
}

bool MonixApp::HitTestLogToolbar(const RECT& clientRect, POINT point) {
  auto r = LogToolbarRects(clientRect);
  if (PtInRect(&r.clear, point)) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    state_.logState.entries.clear();
    state_.logState.counters = SessionCounters{};
    state_.logState.scroll = 0;
    SetToast(L"Logs cleared");
    return true;
  }
  if (PtInRect(&r.pause, point)) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    state_.logState.livePaused = !state_.logState.livePaused;
    SetToast(state_.logState.livePaused ? L"Live logs paused" : L"Live logs resumed");
    return true;
  }
  if (PtInRect(&r.search, point)) {
    SetToast(L"Search: use Ctrl+F to search logs");
    return true;
  }
  if (PtInRect(&r.json, point)) {
    ExportLogsJson();
    SetToast(L"Exported logs as JSON");
    return true;
  }
  if (PtInRect(&r.csv, point)) {
    ExportLogsCsv();
    SetToast(L"Exported logs as CSV");
    return true;
  }
  if (PtInRect(&r.copy, point)) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (!state_.logState.entries.empty()) {
      std::wstring all;
      for (const auto& e : state_.logState.entries) {
        all += ComposeLogLine(e) + L"\r\n";
      }
      CopyToClipboard(all);
      SetToast(L"Copied all logs to clipboard");
    }
    return true;
  }
  return false;
}

bool MonixApp::HitTestLogFilters(const RECT& clientRect, POINT point) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex_);
  auto rects = LogFilterRects(clientRect);
  const std::array<LogFilter, 6> filters = {
    LogFilter::All, LogFilter::Warn, LogFilter::Net,
    LogFilter::Crit, LogFilter::Err, LogFilter::Kernel
  };
  for (std::size_t i = 0; i < 6; ++i) {
    if (PtInRect(&rects[i], point)) {
      state_.logState.activeFilter = filters[i];
      state_.logState.scroll = 0;
      return true;
    }
  }
  return false;
}

std::vector<SettingActionRect> MonixApp::BuildSettingActionRects(const RECT& clientRect) const {
  using namespace monix::renderer_vk;
  const RECT outer = ContentRect(clientRect);
  const int subTabH = 32;
  const RECT contentArea { outer.left, outer.top + subTabH + 8, outer.right, outer.bottom };

  const int rowHeight = std::max(26, smallLineHeight_ + 4);
  std::vector<SettingActionRect> rows;

  const auto appendPanel = [&](const RECT& panel, const auto& settings, int firstRowY) {
    int y = firstRowY;
    for (const SettingId id : settings) {
      SettingActionRect action;
      action.id = id;
      action.row = RECT { panel.left + 18, y, panel.right - 18, y + rowHeight };
      action.decrease = RECT { action.row.right - 84, action.row.top + 4, action.row.right - 52, action.row.bottom - 4 };
      action.increase = RECT { action.row.right - 42, action.row.top + 4, action.row.right - 10, action.row.bottom - 4 };
      action.value = RECT { action.row.right - 246, action.row.top, action.row.right - 94, action.row.bottom };
      rows.push_back(action);
      y += rowHeight + 6;
    }
  };

  if (state_.shaderUi.panel.subTab == SettingsSubTab::General) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    RECT rightPanel = contentArea;
    rightPanel.left = leftPanel.right + 14;
    RECT leftBottom = contentArea;
    leftBottom.top = leftPanel.top + 58 + static_cast<int>(RuntimeSettings().size() * (rowHeight + 6)) + 14;
    appendPanel(leftPanel, RuntimeSettings(), leftPanel.top + 58);
    appendPanel(leftBottom, IntroAnimSettings(), leftBottom.top + 8);
    appendPanel(rightPanel, GeneralExtendedSettings(), rightPanel.top + 58);
  } else if (state_.shaderUi.panel.subTab == SettingsSubTab::Display) {
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
    appendPanel(leftPanel, FullCrtSettings(), leftPanel.top + 58);
    appendPanel(rightTop, AdvancedCrtSettings(), rightTop.top + 58);
    appendPanel(rightMid, ColorCorrectionSettings(), rightMid.top + 58);
    appendPanel(rightMid2, ScanlineStyleSettings(), rightMid2.top + 58);
    appendPanel(rightBottom, BurnInSettings(), rightBottom.top + 58);
    appendPanel(rightBottom2, BorderSettings(), rightBottom2.top + 58);
  } else if (state_.shaderUi.panel.subTab == SettingsSubTab::Logging) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    appendPanel(leftPanel, LoggingSettings(), leftPanel.top + 58);
  } else if (state_.shaderUi.panel.subTab == SettingsSubTab::System) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(320, static_cast<int>((contentArea.bottom - contentArea.top) * 0.45));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightTop.bottom + 14;
    appendPanel(leftPanel, ThemeSettings(), leftPanel.top + 58);
    appendPanel(rightTop, WindowStartupSettings(), rightTop.top + 58);
    appendPanel(rightTop, SystemExtendedSettings(), rightTop.top + 58 + static_cast<int>((rightTop.bottom - rightTop.top) * 0.50));
    appendPanel(rightBottom, HotkeySettings(), rightBottom.top + 58);
  } else if (state_.shaderUi.panel.subTab == SettingsSubTab::Performance) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    appendPanel(leftPanel, PerformanceSettings(), leftPanel.top + 58);
  } else {
    appendPanel(contentArea, RuntimeSettings(), contentArea.top + 58);
  }
  return rows;
}

bool MonixApp::HandleSettingsClick(const RECT& clientRect, POINT point) {
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

  if (PtInRect(&generalTab, point)) {
    state_.shaderUi.panel.subTab = SettingsSubTab::General;
    return true;
  }
  if (PtInRect(&displayTab, point)) {
    state_.shaderUi.panel.subTab = SettingsSubTab::Display;
    return true;
  }
  if (PtInRect(&loggingTab, point)) {
    state_.shaderUi.panel.subTab = SettingsSubTab::Logging;
    return true;
  }
  if (PtInRect(&shadersTab, point)) {
    state_.shaderUi.panel.subTab = SettingsSubTab::Shaders;
    return true;
  }
  if (PtInRect(&systemTab, point)) {
    state_.shaderUi.panel.subTab = SettingsSubTab::System;
    return true;
  }
  if (PtInRect(&perfTab, point)) {
    state_.shaderUi.panel.subTab = SettingsSubTab::Performance;
    return true;
  }

  if (state_.shaderUi.panel.subTab == SettingsSubTab::General) {
    const auto rows = BuildSettingActionRects(clientRect);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      if (!PtInRect(&row.row, point)) {
        continue;
      }
      state_.shaderUi.selectedIndex = static_cast<int>(index);
      int direction = 0;
      if (PtInRect(&row.decrease, point)) {
        direction = -1;
      } else if (PtInRect(&row.increase, point) || PtInRect(&row.value, point)) {
        direction = 1;
      }
      if (direction != 0) {
        CommitSettingMutation(AdjustSetting(row.id, direction));
      }
      return true;
    }
    return false;
  }

  if (state_.shaderUi.panel.subTab == SettingsSubTab::Display || state_.shaderUi.panel.subTab == SettingsSubTab::Logging || state_.shaderUi.panel.subTab == SettingsSubTab::Performance) {
    const auto rows = BuildSettingActionRects(clientRect);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      if (!PtInRect(&row.row, point)) {
        continue;
      }
      state_.shaderUi.selectedIndex = static_cast<int>(index);
      int direction = 0;
      if (PtInRect(&row.decrease, point)) {
        direction = -1;
      } else if (PtInRect(&row.increase, point) || PtInRect(&row.value, point)) {
        direction = 1;
      }
      if (direction != 0) {
        CommitSettingMutation(AdjustSetting(row.id, direction));
      }
      return true;
    }
    return false;
  }

  if (state_.shaderUi.panel.subTab == SettingsSubTab::System) {
    const auto rows = BuildSettingActionRects(clientRect);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      if (!PtInRect(&row.row, point)) {
        continue;
      }
      state_.shaderUi.selectedIndex = static_cast<int>(index);
      int direction = 0;
      if (PtInRect(&row.decrease, point)) {
        direction = -1;
      } else if (PtInRect(&row.increase, point) || PtInRect(&row.value, point)) {
        direction = 1;
      }
      if (direction != 0) {
        CommitSettingMutation(AdjustSetting(row.id, direction));
      }
      return true;
    }
    return false;
  }

  if (state_.shaderUi.panel.subTab == SettingsSubTab::Shaders) {
    const RECT contentArea { outer.left, outer.top + subTabH + 8, outer.right, outer.bottom };

    const int filterBtnW = 80;
    const int filterBtnH = 26;
    const int filterGap = 8;
    int fx = contentArea.left + 14;

    auto hitFilterTab = [&](ShaderLanguageFilter tabFilter) -> bool {
      RECT btn { fx, contentArea.top + 4, fx + filterBtnW, contentArea.top + 4 + filterBtnH };
      fx += filterBtnW + filterGap;
      if (PtInRect(&btn, point)) {
        state_.shaderUi.panel.filter = tabFilter;
        state_.shaderUi.panel.selectedShaderIndex = -1;
        state_.shaderUi.panel.scrollOffset = 0;
        return true;
      }
      return false;
    };

    if (hitFilterTab(ShaderLanguageFilter::GLSL)) return true;
    if (hitFilterTab(ShaderLanguageFilter::Slang)) return true;
    if (hitFilterTab(ShaderLanguageFilter::Preset)) return true;

    RECT favBtn { fx, contentArea.top + 4, fx + filterBtnW + 20, contentArea.top + 4 + filterBtnH };
    if (PtInRect(&favBtn, point)) {
      state_.shaderUi.showFavoritesOnly = !state_.shaderUi.showFavoritesOnly;
      state_.shaderUi.panel.scrollOffset = 0;
      return true;
    }
    fx += filterBtnW + 20 + filterGap;

    const int listW = static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT filterBar = contentArea;
    filterBar.bottom = filterBar.top + 34;

    RECT searchBox { fx, contentArea.top + 4, contentArea.left + listW - 14, contentArea.top + 4 + filterBtnH };
    if (PtInRect(&searchBox, point)) {
      state_.shaderUi.searchFocused = true;
      return true;
    } else {
      state_.shaderUi.searchFocused = false;
    }

    RECT listPanel = contentArea;
    listPanel.top = filterBar.bottom + 6;
    listPanel.right = contentArea.left + listW;

    RECT detailPanel = contentArea;
    detailPanel.left = listPanel.right + 14;
    detailPanel.top = filterBar.bottom + 6;

    const int rowH = std::max(26, smallLineHeight_ + 4);
    const int innerPad = 18;

    RECT actionBar { listPanel.left, listPanel.bottom - 42, listPanel.right, listPanel.bottom - 8 };

    const int actionBtnW = 80;
    const int actionBtnH = 28;
    const int actionGap = 6;
    int abx = actionBar.left + innerPad;

    auto hitActionBtn = [&](const wchar_t* label) -> bool {
      RECT btn { abx, actionBar.top, abx + actionBtnW, actionBar.top + actionBtnH };
      abx += actionBtnW + actionGap;
      if (PtInRect(&btn, point)) {
        if (wcscmp(label, L"IMPORT") == 0) {
          OPENFILENAMEA ofn;
          ZeroMemory(&ofn, sizeof(ofn));
          char szFile[MAX_PATH] = "";
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hwnd_;
          ofn.lpstrFilter = "GLSL\0*.glsl\0GLSLPreset\0*.glslp\0Slang\0*.slang\0SlangPreset\0*.slangp\0All\0*.*\0";
          ofn.lpstrFile = szFile;
          ofn.nMaxFile = MAX_PATH;
          ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
          if (GetOpenFileNameA(&ofn)) {
            std::string srcPath = szFile;
            std::filesystem::path src(srcPath);
            std::string ext = src.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            std::string destDir;
            if (ext == ".glsl") destDir = "GLSL";
            else if (ext == ".glslp") destDir = "GLSLP";
            else if (ext == ".slang") destDir = "SLANG";
            else if (ext == ".slangp") destDir = "SLANGP";
            else return true;
            std::filesystem::path destRoot = paths_.rootDir / "Shaders";
            std::filesystem::path dest = destRoot / destDir / src.filename();
            std::error_code ec;
            std::filesystem::create_directories(dest.parent_path(), ec);
            std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
            if (shaderLibrary_) {
              shaderLibrary_->rescan();
              if (shaderBrowserPanel_) {
                shaderBrowserPanel_->setSearchQuery("");
                state_.shaderUi.searchText.clear();
              }
            }
          }
          return true;
        }
        if (wcscmp(label, L"DELETE") == 0) {
          if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
            const auto* entry = shaderBrowserPanel_->selectedEntry();
            if (entry && entry->sourceKind == monix::renderer_vk::ShaderSourceKind::User) {
              int resp = MessageBoxA(hwnd_, ("Delete shader: " + entry->name + "?").c_str(),
                "Confirm Delete", MB_YESNO | MB_ICONQUESTION);
              if (resp == IDYES) {
                std::error_code ec;
                std::filesystem::remove(entry->path, ec);
                if (shaderLibrary_) shaderLibrary_->rescan();
                state_.shaderUi.panel.selectedShaderIndex = -1;
              }
            }
          }
          return true;
        }
        if (wcscmp(label, L"REFRESH") == 0) {
          if (shaderLibrary_) shaderLibrary_->rescan();
          return true;
        }
        if (wcscmp(label, L"OPEN") == 0) {
          std::wstring wsPath = L"\"" + paths_.rootDir.wstring() + L"\\Shaders\"";
          ShellExecuteW(nullptr, L"open", L"explorer.exe", wsPath.c_str(), nullptr, SW_SHOWDEFAULT);
          return true;
        }
        return true;
      }
      return false;
    };

    if (hitActionBtn(L"IMPORT")) return true;
    if (hitActionBtn(L"DELETE")) return true;
    if (hitActionBtn(L"REFRESH")) return true;
    if (hitActionBtn(L"OPEN")) return true;

    if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
      RECT loadBtn { detailPanel.left + 18, detailPanel.bottom - 52, detailPanel.left + 120, detailPanel.bottom - 22 };
      RECT reloadBtn { detailPanel.left + 130, detailPanel.bottom - 52, detailPanel.left + 240, detailPanel.bottom - 22 };

      if (PtInRect(&loadBtn, point)) {
        shaderBrowserPanel_->requestLoad(txState_);
        return true;
      }
      if (PtInRect(&reloadBtn, point)) {
        shaderBrowserPanel_->requestReload(txState_);
        return true;
      }
    }

    if (PtInRect(&listPanel, point)) {
      int y = listPanel.top + 48;
      int globalRow = 0;
      const auto categories = shaderBrowserPanel_ ? shaderBrowserPanel_->categoryGroups() : std::vector<ShaderCategoryGroup>{};

      for (const auto& cat : categories) {
        if (y + rowH > listPanel.bottom - 50) break;
        y += rowH + 2;

        for (const size_t libIdx : cat.entryIndices) {
          if (globalRow >= state_.shaderUi.panel.scrollOffset) {
            if (y + rowH > listPanel.bottom - 50) break;
            RECT rowRect { listPanel.left + innerPad + 12, y, listPanel.right - innerPad, y + rowH };
            if (PtInRect(&rowRect, point)) {
              state_.shaderUi.panel.selectedShaderIndex = globalRow;
              return true;
            }
            y += rowH + 2;
          }
          ++globalRow;
        }
      }
    }

    return false;
  }

  return false;
}

bool MonixApp::HandleSettingsKey(WPARAM key) {
  using namespace monix::renderer_vk;
  if (state_.shaderUi.panel.subTab == SettingsSubTab::Shaders) {
    if (state_.shaderUi.searchFocused) {
      if (key == VK_ESCAPE) {
        state_.shaderUi.searchFocused = false;
        state_.shaderUi.searchText.clear();
        if (shaderBrowserPanel_) shaderBrowserPanel_->setSearchQuery("");
        return true;
      }
      return true;
    }

    if (key == VK_UP) {
      state_.shaderUi.panel.selectedShaderIndex = std::max(0, state_.shaderUi.panel.selectedShaderIndex - 1);
      return true;
    }
    if (key == VK_DOWN) {
      int maxIdx = shaderBrowserPanel_ ? shaderBrowserPanel_->filteredShaderCount() - 1 : 0;
      state_.shaderUi.panel.selectedShaderIndex = std::min(maxIdx, state_.shaderUi.panel.selectedShaderIndex + 1);
      return true;
    }
    if (key == VK_RETURN) {
      if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
        shaderBrowserPanel_->requestLoad(txState_);
      }
      return true;
    }
    if (key == VK_DELETE) {
      if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
        const auto* entry = shaderBrowserPanel_->selectedEntry();
        if (entry && entry->sourceKind == monix::renderer_vk::ShaderSourceKind::User) {
          int resp = MessageBoxA(hwnd_, ("Delete shader: " + entry->name + "?").c_str(),
            "Confirm Delete", MB_YESNO | MB_ICONQUESTION);
          if (resp == IDYES) {
            std::error_code ec;
            std::filesystem::remove(entry->path, ec);
            if (shaderLibrary_) shaderLibrary_->rescan();
            state_.shaderUi.panel.selectedShaderIndex = -1;
          }
        }
      }
      return true;
    }
    return false;
  }

  const auto getTabSettings = [&]() -> std::vector<SettingId> {
    using namespace monix::renderer_vk;
    switch (state_.shaderUi.panel.subTab) {
      case SettingsSubTab::General: {
        std::vector<SettingId> v;
        for (auto id : RuntimeSettings()) v.push_back(id);
        for (auto id : IntroAnimSettings()) v.push_back(id);
        for (auto id : GeneralExtendedSettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::Display: {
        std::vector<SettingId> v;
        for (auto id : FullCrtSettings()) v.push_back(id);
        for (auto id : AdvancedCrtSettings()) v.push_back(id);
        for (auto id : ColorCorrectionSettings()) v.push_back(id);
        for (auto id : ScanlineStyleSettings()) v.push_back(id);
        for (auto id : BurnInSettings()) v.push_back(id);
        for (auto id : BorderSettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::Logging: {
        std::vector<SettingId> v;
        for (auto id : LoggingSettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::System: {
        std::vector<SettingId> v;
        for (auto id : ThemeSettings()) v.push_back(id);
        for (auto id : WindowStartupSettings()) v.push_back(id);
        for (auto id : SystemExtendedSettings()) v.push_back(id);
        for (auto id : HotkeySettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::Performance: {
        std::vector<SettingId> v;
        for (auto id : PerformanceSettings()) v.push_back(id);
        return v;
      }
      default: return {};
    }
  };

  const auto tabSettings = getTabSettings();
  if (tabSettings.empty()) {
    return false;
  }

  state_.shaderUi.selectedIndex = std::clamp(state_.shaderUi.selectedIndex, 0, static_cast<int>(tabSettings.size()) - 1);
  if (key == VK_UP) {
    state_.shaderUi.selectedIndex = std::max(0, state_.shaderUi.selectedIndex - 1);
    return true;
  }
  if (key == VK_DOWN) {
    state_.shaderUi.selectedIndex = std::min(static_cast<int>(tabSettings.size()) - 1, state_.shaderUi.selectedIndex + 1);
    return true;
  }
  if (key == VK_LEFT) {
    CommitSettingMutation(AdjustSetting(tabSettings[state_.shaderUi.selectedIndex], -1));
    return true;
  }
  if (key == VK_RIGHT || key == VK_RETURN || key == VK_SPACE) {
    CommitSettingMutation(AdjustSetting(tabSettings[state_.shaderUi.selectedIndex], 1));
    return true;
  }

  return false;
}

int MonixApp::HitTestTaskRow(const RECT& clientRect, POINT point) const {
  const RECT outer = ContentRect(clientRect);
  RECT tableRect = outer;
  tableRect.right = outer.right - 380;
  tableRect.top += 68;
  tableRect.bottom -= 18;

  if (!PtInRect(&tableRect, point)) {
    return -1;
  }

  const int headerHeight = 48;
  if (point.y < tableRect.top + headerHeight) {
    return -1;
  }

  const int rowHeight = std::max(38, bodyLineHeight_ + 14);
  const int rowIndex = (point.y - tableRect.top - headerHeight) / rowHeight;
  const int visibleRows = VisibleTaskRows(tableRect);
  if (rowIndex < 0 || rowIndex >= visibleRows) {
    return -1;
  }

  const int processIndex = state_.taskScroll + rowIndex;
  if (processIndex >= static_cast<int>(state_.snapshot.processes.size())) {
    return -1;
  }
  return processIndex;
}

void MonixApp::OpenTaskMenu(const RECT& clientRect, POINT point, int processIndex) {
  ContextMenuState menu;
  menu.visible = true;
  menu.processIndex = processIndex;
  menu.hoverIndex = -1;
  const int itemCount = 9;
  menu.rect = RECT {
    std::clamp(point.x, clientRect.left + 12, clientRect.right - kTaskMenuWidth - 12),
    std::clamp(point.y, clientRect.top + 12, clientRect.bottom - itemCount * kTaskMenuItemHeight - 18),
    0,
    0
  };
  menu.rect.right = menu.rect.left + kTaskMenuWidth;
  menu.rect.bottom = menu.rect.top + itemCount * kTaskMenuItemHeight;
  state_.taskMenu = menu;
}

bool MonixApp::HandleTaskMenuClick(POINT point) {
  if (!state_.taskMenu.visible || !PtInRect(&state_.taskMenu.rect, point)) {
    state_.taskMenu.visible = false;
    return false;
  }

  const int index = (point.y - state_.taskMenu.rect.top) / kTaskMenuItemHeight;
  if (index < 0 || index >= 9) {
    state_.taskMenu.visible = false;
    return false;
  }

  ExecuteTaskMenuAction(index);
  state_.taskMenu.visible = false;
  return true;
}

bool MonixApp::ApplyPriorityToProcess(int pid, DWORD priorityClass) {
  HANDLE handle = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!handle) {
    return false;
  }
  const bool success = SetPriorityClass(handle, priorityClass) != 0;
  CloseHandle(handle);
  return success;
}

bool MonixApp::TerminateProcessById(int pid) {
  HANDLE handle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!handle) {
    return false;
  }
  const bool success = TerminateProcess(handle, 1) != 0;
  CloseHandle(handle);
  return success;
}

void MonixApp::CopyToClipboard(const std::wstring& text) const {
  if (!OpenClipboard(hwnd_)) {
    return;
  }
  const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (memory) {
    EmptyClipboard();
    void* buffer = GlobalLock(memory);
    if (buffer) {
      std::memcpy(buffer, text.c_str(), bytes);
      GlobalUnlock(memory);
      SetClipboardData(CF_UNICODETEXT, memory);
    } else {
      GlobalFree(memory);
    }
  }
  CloseClipboard();
}

void MonixApp::ExecuteTaskMenuAction(int itemIndex) {
  if (state_.taskMenu.processIndex < 0 || state_.taskMenu.processIndex >= static_cast<int>(state_.snapshot.processes.size())) {
    return;
  }

  auto& process = state_.snapshot.processes[state_.taskMenu.processIndex];
  switch (itemIndex) {
    case 0:
      state_.selectedTaskIndex = state_.taskMenu.processIndex;
      SetToast(L"Task inspection pinned");
      PushLog(L"TASKS", L"INFO", L"Inspection focus moved to " + process.name + L".", ColorRole::Primary);
      break;
    case 1:
      state_.scramState.trackedPid = process.pid;
      state_.activeTab = Tab::Scram;
      SetToast(L"Task linked into S.C.R.A.M watch");
      PushLog(L"SCRAM", L"INFO", process.name + L" linked into the live interpretation watchlist.", ColorRole::Scram);
      break;
    case 2:
      CopyToClipboard(process.name + L" | PID " + std::to_wstring(process.pid));
      SetToast(L"Task name and PID copied");
      break;
    case 3:
      if (ApplyPriorityToProcess(process.pid, HIGH_PRIORITY_CLASS)) {
        process.priority = L"HIGH";
        PushLog(L"TASKS", L"SUCCESS", process.name + L" priority raised to HIGH.", ColorRole::Success);
        SetToast(L"Priority raised");
      } else {
        PushLog(L"TASKS", L"ERROR", L"Unable to raise priority for " + process.name + L".", ColorRole::Error);
        SetToast(L"Priority change failed");
      }
      break;
    case 4:
      if (ApplyPriorityToProcess(process.pid, IDLE_PRIORITY_CLASS)) {
        process.priority = L"LOW";
        PushLog(L"TASKS", L"SUCCESS", process.name + L" priority dropped to LOW.", ColorRole::Success);
        SetToast(L"Priority lowered");
      } else {
        PushLog(L"TASKS", L"ERROR", L"Unable to lower priority for " + process.name + L".", ColorRole::Error);
        SetToast(L"Priority change failed");
      }
      break;
    case 5:
      if (TerminateProcessById(process.pid)) {
        PushLog(L"TASKS", L"CRITICAL", process.name + L" was terminated from MONIX control center.", ColorRole::Fatal, L"tasks", L"process-control", L"action=terminate");
        SetToast(L"Process terminated");
      } else {
        PushLog(L"TASKS", L"ERROR", L"Unable to terminate " + process.name + L".", ColorRole::Error, L"tasks", L"process-control", L"action=terminate");
        SetToast(L"Terminate failed");
      }
      break;
    case 6:
      ExportLogsJson();
      SetToast(L"Logs exported");
      PushLog(L"LOG", L"INFO", L"JSON export triggered from process control menu.", ColorRole::UserInput, L"log", L"export", L"source=task-menu");
      break;
    case 7:
      RequestRefresh();
      SetToast(L"Immediate refresh requested");
      break;
    case 8: {
      std::wstring cmd = L"/select,\"" + process.name + L"\"";
      ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOW);
      SetToast(L"Opening in Explorer");
      break;
    }
  }
}

void MonixApp::ComputeViewport(const RECT& client) {
  state_.viewport_ = client;
}

POINT MonixApp::MapToViewport(POINT windowPt) const {
  const RECT& vp = state_.viewport_;
  const int vw = vp.right - vp.left;
  const int vh = vp.bottom - vp.top;
  if (vw <= 0 || vh <= 0) return windowPt;
  const double scale = 4.0 * vh / (3.0 * vw);
  const double centerX = vp.left + vw * 0.5;
  const double centerY = vp.top + vh * 0.5;
  POINT out;
  out.x = static_cast<long>(centerX + (windowPt.x - centerX) / scale);
  out.y = windowPt.y;
  out.x = std::clamp(out.x, vp.left, vp.right - 1);
  out.y = std::clamp(out.y, vp.top, vp.bottom - 1);
  return out;
}

void MonixApp::TickAnimations(const RECT& clientRect) {
  // NOTE: config_ fields (introStepPx, introCreditDelayMs, introHoldMs) are read below
  // without configMutex_. This is accepted risk: config values are POD scalars that are
  // only written on user-initiated settings changes (rare), and a torn read of a POD int
  // is benign (may skip or repeat one animation step).
  if (state_.notifState.toastUntilMs > 0 && GetTickCount64() > state_.notifState.toastUntilMs) {
    state_.notifState.toastMessage.clear();
    state_.notifState.toastUntilMs = 0;
  }

  state_.notifState.items.erase(
    std::remove_if(state_.notifState.items.begin(), state_.notifState.items.end(), [](const NotificationItem& item) {
      return GetTickCount64() > item.expiresAtMs;
    }),
    state_.notifState.items.end()
  );

  FlushLogQueues(false);

  if (!state_.intro.active) {
    return;
  }

  if (!state_.intro.soundPlayed && std::filesystem::exists(paths_.introWave)) {
    PlaySoundW(paths_.introWave.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    state_.intro.soundPlayed = true;
  }

  int cellW = 0;
  int cellH = 0;
  int logoWidth = 0;
  int logoHeight = 0;
  int maxColumns = 0;
  ComputeIntroLogoMetrics(clientRect, cellW, cellH, logoWidth, logoHeight, maxColumns);
  const int targetY = clientRect.top + ((clientRect.bottom - clientRect.top) - logoHeight) / 2 - 12;
  if (state_.intro.logoY == -9999) {
    state_.intro.logoY = -logoHeight;
  }

  if (state_.intro.logoY < targetY) {
    state_.intro.logoY = std::min(targetY, state_.intro.logoY + config_.introStepPx);
    return;
  }

  if (state_.intro.settledAtMs == 0) {
    state_.intro.settledAtMs = GetTickCount64();
  }

  const ULONGLONG elapsed = GetTickCount64() - state_.intro.settledAtMs;
  if (elapsed >= config_.introCreditDelayMs && state_.intro.creditOffset > 0) {
    state_.intro.creditOffset = std::max(0, state_.intro.creditOffset - std::max(3, config_.introStepPx / 3));
  }

  if (elapsed >= config_.introHoldMs) {
    state_.intro.active = false;
    SetToast(L"Telemetry viewport ready");
  }
}

void MonixApp::ComputeIntroLogoMetrics(const RECT& clientRect, int& cellW, int& cellH, int& logoWidth, int& logoHeight, int& maxColumns) const {
  const auto logo = IntroLogo();
  maxColumns = 0;
  for (const auto& line : logo) {
    maxColumns = std::max(maxColumns, static_cast<int>(line.size()));
  }

  const int maxWidth = std::max(540, static_cast<int>((clientRect.right - clientRect.left) * 0.78));
  cellW = std::clamp(maxWidth / std::max(1, maxColumns), 7, 16);
  cellH = cellW;
  logoWidth = maxColumns * cellW;
  logoHeight = static_cast<int>(logo.size()) * cellH;
}

LRESULT MonixApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
      info->ptMinTrackSize.x = kWindowMinWidth;
      info->ptMinTrackSize.y = kWindowMinHeight;
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_LBUTTONDOWN: {
      SetFocus(hwnd);
      RECT client {};
      GetClientRect(hwnd, &client);
      ComputeViewport(client);
      POINT wndPt { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      POINT point = MapToViewport(wndPt);
      if (state_.clickDebug.mode) {
        state_.clickDebug.wnd = wndPt;
        state_.clickDebug.bmp = point;
        state_.clickDebug.ms = GetTickCount64();
      }
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      if (!state_.loggedIn) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      PlayClickSound();

      state_.pressedButton = -1;

      if (state_.updateState.dialogVisible) {
        const int cw2 = client.right - client.left;
        const int ch2 = client.bottom - client.top;
        const float sc = monix::ui::Win98Theme::ComputeScale(cw2, ch2);
        const int dlgW = 420;
        const int dlgH = 280;
        int dlgX = client.left + static_cast<int>((cw2 - dlgW * sc) / 2);
        int dlgY = client.top + static_cast<int>((ch2 - dlgH * sc) / 2);
        RECT dlg = { dlgX, dlgY, dlgX + static_cast<int>(dlgW * sc), dlgY + static_cast<int>(dlgH * sc) };

        if (PtInRect(&dlg, wndPt)) {
          RECT closeBtn = { dlg.right - static_cast<int>(20 * sc), dlg.top + static_cast<int>(2 * sc),
                            dlg.right - static_cast<int>(4 * sc), dlg.top + static_cast<int>(20 * sc) };
          const int btnW = 90, btnH = 24;
          int btnY = dlg.bottom - static_cast<int>(38 * sc);

          if (PtInRect(&closeBtn, wndPt)) {
            state_.updateState.dialogVisible = false;
          } else if (state_.updateState.updateAvailable) {
            int okX = dlg.left + static_cast<int>((dlgW / 2 - btnW - 10) * sc);
            RECT okBtn = { okX, btnY, okX + static_cast<int>(btnW * sc), btnY + static_cast<int>(btnH * sc) };
            int cxBtn = okX + static_cast<int>((btnW + 20) * sc);
            RECT cancelBtn = { cxBtn, btnY, cxBtn + static_cast<int>(btnW * sc), btnY + static_cast<int>(btnH * sc) };
            if (PtInRect(&okBtn, wndPt)) {
              if (!state_.updateState.downloadUrl.empty()) {
                ShellExecuteW(hwnd, L"open", state_.updateState.downloadUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
              }
              state_.updateState.dialogVisible = false;
            } else if (PtInRect(&cancelBtn, wndPt)) {
              state_.updateState.dialogVisible = false;
            }
          } else {
            int okX = dlg.left + static_cast<int>((dlgW / 2 - btnW / 2) * sc);
            RECT okBtn = { okX, btnY, okX + static_cast<int>(btnW * sc), btnY + static_cast<int>(btnH * sc) };
            if (PtInRect(&okBtn, wndPt)) {
              state_.updateState.dialogVisible = false;
            }
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (state_.taskMenu.visible && HandleTaskMenuClick(point)) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      state_.taskMenu.visible = false;

      if (!state_.intro.active) {
        if (IsCoreMonitorThemeActive()) {
          if (config_.themeMode == monix::ui::kWin98ThemeMode) {
            int tabIndex = 0;
            if (monix::ui::Win98Theme::HitTestTabs(client, wndPt, tabIndex)) {
              state_.coreMonitorMenuIndex = tabIndex;
              InvalidateRect(hwnd, nullptr, FALSE);
              return 0;
            }
            {
              const int cw3 = client.right - client.left;
              const int ch3 = client.bottom - client.top;
              const float sc3 = monix::ui::Win98Theme::ComputeScale(cw3, ch3);
              const int dw3 = static_cast<int>(1536 * sc3);
              const int dh3 = static_cast<int>(1024 * sc3);
              const int ox3 = client.left + (cw3 - dw3) / 2;
              const int oy3 = client.top + (ch3 - dh3) / 2;
              double mx = (wndPt.x - ox3) / sc3;
              double my = (wndPt.y - oy3) / sc3;
              if (my >= 29.0 && my <= 48.0) {
                int mIdx = static_cast<int>((mx - 4.0) / 70.0);
                if (mIdx == 3) {
                  if (!state_.updateState.checking) {
                    state_.updateState.checking = true;
                    state_.updateState.dialogVisible = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    auto* updater = new monix::updater::AutoUpdater();
                    HWND hCopy = hwnd;
                    updater->CheckForUpdateAsync([hCopy, updater](const monix::updater::UpdateInfo& info) {
                      {
                        std::lock_guard<std::mutex> lock(s_updateInfoMutex);
                        s_pendingUpdateInfo = info;
                        s_updateInfoReady = true;
                      }
                      PostMessage(hCopy, WM_USER + 77, 0, 0);
                      delete updater;
                    });
                  }
                  InvalidateRect(hwnd, nullptr, FALSE);
                  return 0;
                }
              }
            }
            if (state_.coreMonitorMenuIndex == 1) {
              auto taskHit = monix::ui::Win98Theme::HitTestTaskRow(client, wndPt);
              if (taskHit.hit) {
                std::vector<ProcessInfo> procs = state_.snapshot.processes;
                if (procs.empty()) {
                  procs = {
                    { L"explorer.exe", 1840, 4, 1, 1.1, 34000000ull, 0.2, 0, L"Running", L"NORMAL", L"" },
                    { L"chrome.exe", 2672, 4, 1, 12.4, 420000000ull, 3.1, 0, L"Running", L"NORMAL", L"" },
                    { L"obs64.exe", 6216, 4, 1, 18.0, 820000000ull, 28.0, 0, L"Running", L"HIGH", L"" },
                    { L"Monix.exe", 3920, 4, 1, 2.3, 48000000ull, 2.0, 0, L"Running", L"NORMAL", L"" }
                  };
                }
                std::sort(procs.begin(), procs.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
                  if (a.cpuPct != b.cpuPct) return a.cpuPct > b.cpuPct;
                  return a.ramBytes > b.ramBytes;
                });
                int rowIdx = state_.taskScroll + taskHit.index;
                if (rowIdx >= 0 && rowIdx < static_cast<int>(procs.size())) {
                  state_.selectedTaskPid = procs[rowIdx].pid;
                  state_.selectedTaskIndex = rowIdx;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              int toolbarBtn = monix::ui::Win98Theme::HitTestTaskToolbar(client, wndPt);
              if (toolbarBtn >= 0) {
                state_.pressedButton = 100 + toolbarBtn;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                auto findSelected = [&]() -> const ProcessInfo* {
                  if (state_.selectedTaskPid != 0) {
                    for (const auto& p : state_.snapshot.processes) {
                      if (p.pid == state_.selectedTaskPid) return &p;
                    }
                  }
                  return nullptr;
                };
                if (toolbarBtn == 0 || toolbarBtn == 1) {
                  const ProcessInfo* proc = findSelected();
                  if (proc) {
                    TerminateProcessById(proc->pid);
                    SetToast(L"Ended: " + proc->name + L" (PID: " + std::to_wstring(proc->pid) + L")");
                  } else {
                    SetToast(L"No task selected");
                  }
                } else if (toolbarBtn == 2) {
                  SetToast(L"Priority: Use +/- keys on selected process");
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              int taskScrollDir = monix::ui::Win98Theme::HitTestTaskScrollbar(client, wndPt);
              if (taskScrollDir != 0) {
                const int maxScroll = (std::max)(0, static_cast<int>(state_.snapshot.processes.size()) - 28);
                state_.taskScroll = std::clamp(state_.taskScroll + taskScrollDir, 0, maxScroll);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
            }
            if (state_.coreMonitorMenuIndex == 2) {
              int scramBtn = monix::ui::Win98Theme::HitTestScramButton(client, wndPt);
              if (scramBtn >= 0) {
                state_.pressedButton = 200 + scramBtn;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
              }
              if (scramBtn == 0) {
                state_.scramState.headline = L"SCRAM TEST PASSED";
                state_.scramState.insight = L"Emergency mechanism tested successfully";
                PushLog(L"SCRAM", L"INFO", L"SCRAM TEST executed — all systems nominal.", ColorRole::Scram, L"scram", L"test", L"action=test");
                SetToast(L"SCRAM TEST: All systems nominal");
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              } else if (scramBtn == 1) {
                state_.scramState.headline = L"SCRAM REAL ACTIVATED";
                state_.scramState.insight = L"Emergency mechanism armed for real execution";
                PushLog(L"SCRAM", L"WARNING", L"SCRAM REAL armed — emergency triage active.", ColorRole::Scram, L"scram", L"real", L"action=real");
                SetToast(L"SCRAM REAL: Emergency triage activated");
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
            }
            if (state_.coreMonitorMenuIndex == 0) {
              int scrollDir = monix::ui::Win98Theme::HitTestLogScrollbar(client, wndPt);
              if (scrollDir != 0) {
                state_.logState.scroll = (std::max)(0, state_.logState.scroll + scrollDir);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
            }
            if (state_.coreMonitorMenuIndex == 4) {
              int settingsCat = 0;
              if (monix::ui::Win98Theme::HitTestSettingsCategories(client, wndPt, settingsCat)) {
                state_.settingsCategory = settingsCat;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              int toggleIdx = -1;
              if (monix::ui::Win98Theme::HitTestSettingsToggle(client, wndPt, toggleIdx) && toggleIdx >= 0) {
                SettingId sid = SettingId::TelemetryIntervalMs;
                bool found = false;
                int cat = state_.settingsCategory;
                if (cat == 0) {
                  const SettingId cat0Toggles[] = {
                    SettingId::AlwaysOnTop, SettingId::MinimizeToTray, SettingId::StartWithWindows,
                    SettingId::StartMaximized, SettingId::SaveWindowPosition, SettingId::ShowFps,
                    SettingId::VSyncEnabled
                  };
                  if (toggleIdx < 7) { sid = cat0Toggles[toggleIdx]; found = true; }
                } else if (cat == 1) {
                  const SettingId cat1Toggles[] = {
                    SettingId::CrtEnabled, SettingId::CrtBurnInEnabled, SettingId::BorderEnabled
                  };
                  if (toggleIdx < 3) { sid = cat1Toggles[toggleIdx]; found = true; }
                } else if (cat == 2) {
                  const SettingId cat2Toggles[] = { SettingId::AnalyticsHistoryEnabled };
                  if (toggleIdx < 1) { sid = cat2Toggles[toggleIdx]; found = true; }
                } else if (cat == 3) {
                  const SettingId cat3Toggles[] = {
                    SettingId::LogMilliseconds, SettingId::LogJsonEnabled, SettingId::LogPlainEnabled,
                    SettingId::LogDeduplicate, SettingId::PauseLiveLogs
                  };
                  if (toggleIdx < 5) { sid = cat3Toggles[toggleIdx]; found = true; }
                } else if (cat == 4) {
                  const SettingId cat4Toggles[] = {
                    SettingId::NotificationsEnabled, SettingId::SoundEnabled
                  };
                  if (toggleIdx < 2) { sid = cat4Toggles[toggleIdx]; found = true; }
                }
                if (found) {
                  CommitSettingMutation(AdjustSetting(sid, 1));
                  SetToast(L"Setting toggled");
                  InvalidateRect(hwnd, nullptr, FALSE);
                  return 0;
                }
              }
              int adjRow = -1, adjDir = 0;
              if (monix::ui::Win98Theme::HitTestSettingsAdjust(client, wndPt, adjRow, adjDir) && adjRow >= 0 && adjDir != 0) {
                SettingId sid = SettingId::TelemetryIntervalMs;
                bool found = false;
                int cat = state_.settingsCategory;
                if (cat == 0) {
                  if (adjRow >= 7 && adjRow <= 10) {
                    const SettingId cat0Adj[] = {
                      SettingId::ThemeMode, SettingId::ProcessPriority,
                      SettingId::FontScale, SettingId::WindowOpacity
                    };
                    sid = cat0Adj[adjRow - 7]; found = true;
                  }
                } else if (cat == 1) {
                  if (adjRow >= 1 && adjRow <= 12) {
                    const SettingId cat1Adj[] = {
                      SettingId::CrtBrightness, SettingId::CrtContrast, SettingId::CrtSaturation,
                      SettingId::CrtGamma, SettingId::CrtCurvatureStrength, SettingId::CrtScanlineIntensity,
                      SettingId::CrtChromaticAberration, SettingId::CrtPhosphorGlow, SettingId::CrtVignetteStrength,
                      SettingId::CrtSharpness, SettingId::CrtFlickerAmount, SettingId::CrtNoiseAmount,
                    };
                    if (adjRow - 1 < 12) { sid = cat1Adj[adjRow - 1]; found = true; }
                  } else if (adjRow == 14) {
                    sid = SettingId::CrtBurnInIntensity; found = true;
                  }
                } else if (cat == 2) {
                  if (adjRow >= 1 && adjRow <= 4) {
                    const SettingId cat2Adj[] = {
                      SettingId::HistoryCapacity, SettingId::TelemetryIntervalMs,
                      SettingId::FrameIntervalMs, SettingId::FrameTargetFps
                    };
                    sid = cat2Adj[adjRow - 1]; found = true;
                  }
                } else if (cat == 3) {
                  const SettingId cat3Adj[] = {
                    SettingId::LogVisibleLines, SettingId::LogFlushIntervalMs,
                    SettingId::LogRetentionDays
                  };
                  if (adjRow >= 5 && adjRow <= 7) { sid = cat3Adj[adjRow - 5]; found = true; }
                }
                if (found) {
                  CommitSettingMutation(AdjustSetting(sid, adjDir));
                  SetToast(L"Setting adjusted");
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
            }
          }

          if (state_.coreMonitorMenuIndex != 4) {
            int direction = 0;
            if (monix::ui::CoreMonitorTheme::HitTestThemeControl(client, point, direction)) {
              CommitSettingMutation(AdjustSetting(SettingId::ThemeMode, direction));
              InvalidateRect(hwnd, nullptr, FALSE);
              return 0;
            }

            int menuIndex = 0;
            if (monix::ui::CoreMonitorTheme::HitTestMenu(client, point, menuIndex)) {
              state_.coreMonitorMenuIndex = menuIndex;
              InvalidateRect(hwnd, nullptr, FALSE);
              return 0;
            }
          }

          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }

        Tab tab = Tab::Log;
        if (HitTestTabs(client, point, tab)) {
          if (tab != state_.activeTab) {
            state_.activeTab = tab;
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }

        if (state_.activeTab == Tab::Tasks) {
          const int processIndex = HitTestTaskRow(client, point);
          if (processIndex >= 0) {
            state_.selectedTaskIndex = processIndex;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        } else if (state_.activeTab == Tab::Log) {
          if (HitTestLogToolbar(client, point)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
          if (HitTestLogFilters(client, point)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        } else if (state_.activeTab == Tab::Settings) {
          if (HandleSettingsClick(client, point)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        }
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_RBUTTONDOWN: {
      PlayClickSound();
      RECT client {};
      GetClientRect(hwnd, &client);
      ComputeViewport(client);
      POINT wndPt { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      POINT point = MapToViewport(wndPt);
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      if (!state_.intro.active && IsCoreMonitorThemeActive()) {
        if (state_.coreMonitorMenuIndex == 1) {
          auto taskHit = monix::ui::Win98Theme::HitTestTaskRow(client, wndPt);
          if (taskHit.hit) {
            state_.selectedTaskIndex = taskHit.index;
            OpenTaskMenu(client, wndPt, taskHit.index);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        }
        state_.taskMenu.visible = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.intro.active && state_.activeTab == Tab::Tasks) {
        const int processIndex = HitTestTaskRow(client, point);
        if (processIndex >= 0) {
          state_.selectedTaskIndex = processIndex;
          OpenTaskMenu(client, point, processIndex);
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
      }
      state_.taskMenu.visible = false;
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_LBUTTONUP: {
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      if (state_.pressedButton >= 0) {
        state_.pressedButton = -1;
        ReleaseCapture();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_MOUSEMOVE: {
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      if (state_.taskMenu.visible) {
        RECT client {};
        GetClientRect(hwnd, &client);
        ComputeViewport(client);
        POINT point { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        point = MapToViewport(point);
        if (PtInRect(&state_.taskMenu.rect, point)) {
          state_.taskMenu.hoverIndex = std::clamp(static_cast<int>((point.y - state_.taskMenu.rect.top) / kTaskMenuItemHeight), 0, 8);
        } else {
          state_.taskMenu.hoverIndex = -1;
        }
      }
      if (IsCoreMonitorThemeActive()) {
        RECT client {};
        GetClientRect(hwnd, &client);
        POINT wndPt { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const int cw4 = client.right - client.left;
        const int ch4 = client.bottom - client.top;
        const float sc4 = monix::ui::Win98Theme::ComputeScale(cw4, ch4);
        const int dw4 = static_cast<int>(1536 * sc4);
        const int dh4 = static_cast<int>(1024 * sc4);
        const int ox4 = client.left + (cw4 - dw4) / 2;
        const int oy4 = client.top + (ch4 - dh4) / 2;
        double mx = (wndPt.x - ox4) / sc4;
        double my = (wndPt.y - oy4) / sc4;
        int newHover = -1;
        if (my >= 29.0 && my <= 48.0) {
          int mIdx = static_cast<int>((mx - 4.0) / 70.0);
          if (mIdx >= 0 && mIdx < 4) newHover = mIdx;
        }
        if (newHover != state_.hoveredMenuIndex) {
          state_.hoveredMenuIndex = newHover;
        }
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MOUSEWHEEL: {
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      if (state_.intro.active) {
        return 0;
      }
      if (IsCoreMonitorThemeActive()) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        const int step = delta > 0 ? -3 : 3;
        if (state_.coreMonitorMenuIndex == 0) {
          const int maxLogScroll = (std::max)(0, static_cast<int>(state_.logState.entries.size()) - 20);
          state_.logState.scroll = std::clamp(state_.logState.scroll + step, 0, maxLogScroll);
        } else if (state_.coreMonitorMenuIndex == 1) {
          const int maxScroll = (std::max)(0, static_cast<int>(state_.snapshot.processes.size()) - 49);
          state_.taskScroll = std::clamp(state_.taskScroll + step, 0, maxScroll);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
      const int step = delta > 0 ? -1 : 1;
      if (state_.activeTab == Tab::Log) {
        RECT client {};
        GetClientRect(hwnd, &client);
        const RECT contentRect = ContentRect(client);
        const RECT listRect {
          contentRect.left + 14,
          contentRect.top + 46,
          contentRect.right - 14,
          contentRect.bottom - 14
        };
        const int maxScroll = std::max(0, static_cast<int>(state_.logState.entries.size()) - VisibleLogLines(listRect));
        state_.logState.scroll = std::clamp(state_.logState.scroll + step, 0, maxScroll);
      } else if (state_.activeTab == Tab::Tasks) {
        const int maxScroll = (std::max)(0, static_cast<int>(state_.snapshot.processes.size()) - 49);
        state_.taskScroll = std::clamp(state_.taskScroll + step, 0, maxScroll);
      } else if (state_.activeTab == Tab::Settings && state_.shaderUi.panel.subTab == monix::renderer_vk::SettingsSubTab::Shaders) {
        int maxScroll = 0;
        if (shaderBrowserPanel_) {
          const auto cats = shaderBrowserPanel_->categoryGroups();
          int totalRows = 0;
          for (const auto& cat : cats) totalRows += static_cast<int>(cat.entryIndices.size());
          maxScroll = std::max(0, totalRows - 15);
        }
        state_.shaderUi.panel.scrollOffset = std::clamp(state_.shaderUi.panel.scrollOffset + step, 0, maxScroll);
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_CHAR: {
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      if (!state_.loggedIn) {
        kernel_.HandleChar(wParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (state_.devShell.active) {
        if (wParam >= 32 && wParam < 127 && state_.devShell.input.size() < 200) {
          state_.devShell.input += static_cast<wchar_t>(wParam);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.intro.active && IsCoreMonitorThemeActive()) {
        return 0;
      }
      if (state_.activeTab == Tab::Settings &&
          state_.shaderUi.panel.subTab == monix::renderer_vk::SettingsSubTab::Shaders &&
          state_.shaderUi.searchFocused) {
        if (wParam == VK_BACK) {
          if (!state_.shaderUi.searchText.empty()) {
            state_.shaderUi.searchText.pop_back();
          }
        } else if (wParam >= 32 && wParam < 127) {
          state_.shaderUi.searchText += static_cast<wchar_t>(wParam);
        }
        if (shaderBrowserPanel_) {
          shaderBrowserPanel_->setSearchQuery(WideToUtf8(state_.shaderUi.searchText));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_KEYDOWN: {
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      const bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      const bool altHeld = (GetKeyState(VK_MENU) & 0x8000) != 0;
      if (ctrlHeld && shiftHeld && altHeld && wParam == VK_F5) {
        state_.devShell.active = !state_.devShell.active;
        if (state_.devShell.active && state_.devShell.history.empty()) {
          state_.devShell.history.push_back(L"MONIX DevShell v3.1.0 — type 'help' for commands");
          state_.devShell.history.push_back(L"─────────────────────────────────────────────");
        }
        SetToast(state_.devShell.active ? L"DevShell: ON (Ctrl+Shift+Alt+F5)" : L"DevShell: OFF");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (state_.devShell.active) {
        if (wParam == VK_ESCAPE) {
          state_.devShell.active = false;
          SetToast(L"DevShell: OFF");
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_RETURN) {
          std::wstring cmd = state_.devShell.input;
          state_.devShell.history.push_back(L"> " + cmd);
          state_.devShell.input.clear();
          if (cmd == L"help") {
            state_.devShell.history.push_back(L"Commands: help, status, version, processes, scram, clear, config, log, uptime, memory");
          } else if (cmd == L"status") {
            wchar_t buf[256];
            double ramPct = (state_.snapshot.ramTotalBytes > 0) ?
              (static_cast<double>(state_.snapshot.ramUsedBytes) / state_.snapshot.ramTotalBytes * 100.0) : 0.0;
            _snwprintf_s(buf, 256, L"CPU: %.0f%%  RAM: %.0f%%  Risk: %.0f  Procs: %d",
              state_.snapshot.cpuPct, ramPct, state_.scramState.smoothedRisk,
              static_cast<int>(state_.snapshot.processes.size()));
            state_.devShell.history.push_back(buf);
          } else if (cmd == L"version") {
            state_.devShell.history.push_back(L"MONIX v" MONIX_VERSION_W " (C++20, Vulkan + OpenGL)");
          } else if (cmd == L"processes") {
            for (const auto& p : state_.snapshot.processes) {
              if (p.cpuPct >= 5.0 || p.ramBytes > 100000000) {
                wchar_t buf[256];
                _snwprintf_s(buf, 256, L"  %s [PID %d] CPU %.0f%% RAM %lluMB",
                  p.name.c_str(), p.pid, p.cpuPct, static_cast<unsigned long long>(p.ramBytes / 1048576));
                state_.devShell.history.push_back(buf);
              }
            }
          } else if (cmd == L"scram") {
            wchar_t buf[256];
            _snwprintf_s(buf, 256, L"SCRAM risk=%.0f severity=%d smoothed=%.1f",
              state_.scramState.riskScore, state_.scramState.currentSeverity, state_.scramState.smoothedRisk);
            state_.devShell.history.push_back(buf);
          } else if (cmd == L"clear") {
            state_.devShell.history.clear();
          } else if (cmd == L"config") {
            wchar_t buf[256];
            _snwprintf_s(buf, 256, L"interval=%dms theme=%d loglevel=%d notifications=%s",
              config_.telemetryIntervalMs, config_.themeMode, config_.logLevel,
              config_.notificationsEnabled ? L"on" : L"off");
            state_.devShell.history.push_back(buf);
          } else if (cmd == L"log") {
            for (int i = std::max(0, static_cast<int>(state_.logState.entries.size()) - 10); i < static_cast<int>(state_.logState.entries.size()); ++i) {
              state_.devShell.history.push_back(ComposeLogLine(state_.logState.entries[i]));
            }
          } else if (cmd == L"uptime") {
            ULONGLONG ms = GetTickCount64();
            wchar_t buf[128];
            _snwprintf_s(buf, 128, L"Uptime: %llum %llum %llus", ms / 3600000, (ms / 60000) % 60, (ms / 1000) % 60);
            state_.devShell.history.push_back(buf);
          } else if (cmd == L"memory") {
            MEMORYSTATUSEX mem{};
            mem.dwLength = sizeof(mem);
            GlobalMemoryStatusEx(&mem);
            wchar_t buf[256];
            _snwprintf_s(buf, 256, L"RAM: %lluMB / %lluMB (%.0f%%)  PageFile: %lluMB",
              (mem.ullTotalPhys - mem.ullAvailPhys) / 1048576, mem.ullTotalPhys / 1048576, static_cast<double>(mem.dwMemoryLoad),
              mem.ullTotalPageFile / 1048576);
            state_.devShell.history.push_back(buf);
          } else if (!cmd.empty()) {
            state_.devShell.history.push_back(L"Unknown command: " + cmd);
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_BACK) {
          if (!state_.devShell.input.empty()) state_.devShell.input.pop_back();
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_UP) {
          if (state_.devShell.scrollOffset > 0) state_.devShell.scrollOffset--;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_DOWN) {
          state_.devShell.scrollOffset++;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.loggedIn && ctrlHeld && shiftHeld && wParam == 'K') {
        kernel_.SkipTests();
        SetToast(L"TESTS SKIPPED (DEV MODE) — Ctrl+Shift+K");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrlHeld && shiftHeld && wParam == 'L') {
        state_.clickDebug.mode = !state_.clickDebug.mode;
        SetToast(state_.clickDebug.mode ? L"CLICK DEBUG: ON" : L"CLICK DEBUG: OFF");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrlHeld && wParam == 'T') {
        CommitSettingMutation(AdjustSetting(SettingId::CrtEnabled, 1));
        SetToast(config_.crtEnabled ? L"CRT SHADER: ON" : L"CRT SHADER: OFF");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.loggedIn) {
        bool loggedIn = false, shutdown = false;
        kernel_.HandleKeyDown(wParam, loggedIn, shutdown);
        if (shutdown) {
          PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (state_.intro.active) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (IsCoreMonitorThemeActive()) {
        if (wParam >= '1' && wParam <= '9') {
          state_.coreMonitorMenuIndex = static_cast<int>(wParam - '1');
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_UP) {
          state_.coreMonitorMenuIndex = (state_.coreMonitorMenuIndex + monix::ui::CoreMonitorTheme::kMenuCount - 1) % monix::ui::CoreMonitorTheme::kMenuCount;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_DOWN) {
          state_.coreMonitorMenuIndex = (state_.coreMonitorMenuIndex + 1) % monix::ui::CoreMonitorTheme::kMenuCount;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if ((wParam == VK_LEFT || wParam == VK_RIGHT) &&
            state_.coreMonitorMenuIndex == monix::ui::CoreMonitorTheme::kMenuCount - 1) {
          CommitSettingMutation(AdjustSetting(SettingId::ThemeMode, wParam == VK_LEFT ? -1 : 1));
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_F5 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
          state_.logState.entries.clear();
          state_.logState.counters = SessionCounters{};
          state_.logState.scroll = 0;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_ESCAPE) {
          state_.taskMenu.visible = false;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        return 0;
      }
      if (wParam >= '1' && wParam <= '6') {
        Tab newTab = static_cast<Tab>(wParam - '1');
        if (newTab != state_.activeTab) {
          state_.activeTab = newTab;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_ESCAPE) {
        state_.taskMenu.visible = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (state_.activeTab == Tab::Settings && HandleSettingsKey(wParam)) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (ctrlHeld && wParam == 'F' && state_.activeTab == Tab::Settings) {
        state_.shaderUi.panel.subTab = monix::renderer_vk::SettingsSubTab::Shaders;
        state_.shaderUi.searchFocused = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_F5 && !(GetKeyState(VK_SHIFT) & 0x8000) && state_.activeTab == Tab::Settings &&
          state_.shaderUi.panel.subTab == monix::renderer_vk::SettingsSubTab::Shaders) {
        if (shaderLibrary_) shaderLibrary_->rescan();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }



      if (wParam == VK_F1) {
        // Toggle pass output debug view
        openGl_.debugViewMode = (openGl_.debugViewMode == 1) ? 0 : 1;
        openGl_.debugPassIndex = 0;
        std::wstring msg = openGl_.debugViewMode == 1
          ? L"Debug: Pass output (F1=off, Up/Down=pass)"
          : L"Debug: OFF";
        SetToast(msg);
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=pass");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F2) {
        // Toggle InfoCache texture view
        openGl_.debugViewMode = (openGl_.debugViewMode == 2) ? 0 : 2;
        std::wstring msg = openGl_.debugViewMode == 2
          ? L"Debug: InfoCache geometry"
          : L"Debug: OFF";
        SetToast(msg);
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=infocache");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F3) {
        // Toggle feedback texture view
        openGl_.debugViewMode = (openGl_.debugViewMode == 3) ? 0 : 3;
        std::wstring msg = openGl_.debugViewMode == 3
          ? L"Debug: Feedback history"
          : L"Debug: OFF";
        SetToast(msg);
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=feedback");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F4 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        // Toggle external texture status view
        openGl_.debugViewMode = (openGl_.debugViewMode == 4) ? 0 : 4;
        std::wstring msg;
        if (openGl_.debugViewMode == 4) {
          msg = L"Debug: External textures";
          // Dead GL code removed - shader pipeline no longer used
        } else {
          msg = L"Debug: OFF";
        }
        SetToast(openGl_.debugViewMode == 4 ? L"Debug: External textures" : L"Debug: OFF");
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=external");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }



      if (wParam == VK_F5 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        // Toggle HSM parameters debug view
        openGl_.debugViewMode = (openGl_.debugViewMode == 5) ? 0 : 5;
        std::wstring msg;
        if (openGl_.debugViewMode == 5) {
          msg = L"Debug: HSM Parameters";
          // Dead GL code removed - shader pipeline no longer used
        } else {
          msg = L"Debug: OFF";
        }
        SetToast(openGl_.debugViewMode == 5 ? L"Debug: HSM Parameters" : L"Debug: OFF");
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=hsm");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F5 && (GetKeyState(VK_SHIFT) & 0x8000)) {
        // Shift+F5: reload config (was F5 before Phase 4)
        LoadConfig(true);
        CreateUiFonts();
        SetFrameTimer();
        RequestRefresh();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F6 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        ExportLogsJson();
        SetToast(L"Logs exported to JSON");
        PushLog(L"LOG", L"INFO", L"Structured log export generated in exports folder.", ColorRole::UserInput, L"log", L"export", L"format=json");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F7 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        ExportLogsCsv();
        SetToast(L"Logs exported to CSV");
        PushLog(L"LOG", L"INFO", L"CSV log export generated in exports folder.", ColorRole::UserInput, L"log", L"export", L"format=csv");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F8 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        const auto candidate = std::max_element(state_.snapshot.processes.begin(), state_.snapshot.processes.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
          const double leftScore = left.cpuPct * 2.0 + (left.ramBytes / 1048576.0) * 0.02 + left.gpuPct * 1.5;
          const double rightScore = right.cpuPct * 2.0 + (right.ramBytes / 1048576.0) * 0.02 + right.gpuPct * 1.5;
          return leftScore < rightScore;
        });
        if (candidate != state_.snapshot.processes.end()) {
          state_.scramState.trackedPid = candidate->pid;
          state_.activeTab = Tab::Scram;
          PushLog(L"SCRAM", L"INFO", L"Realtime triage pinned " + candidate->name + L" as the current risk focus.", ColorRole::Scram, L"scram", L"triage", L"action=autotrack pid=" + std::to_wstring(candidate->pid));
          SetToast(L"S.C.R.A.M triage pinned task");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F9 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        SwitchFont(1);
        if (!paths_.fontList.empty() && state_.currentFontIndex >= 0 && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
          const auto& font = paths_.fontList[state_.currentFontIndex];
          PushLog(L"ENGINE", L"INFO", L"Font switched to " + font.displayName + L" (" + font.faceName + L").", ColorRole::UserInput, L"engine", L"font", L"name=" + font.displayName);
          SetToast(L"Font: " + font.displayName);
        } else {
          SetToast(L"No fonts available");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_F10 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        // Screenshot capture to PNG
        if (openGl_.vkAvailable && openGl_.vk.isInitialized()) {
          openGl_.screenshotRequested = true;
          RECT cr;
          GetClientRect(hwnd, &cr);
          openGl_.screenshotWidth = cr.right - cr.left;
          openGl_.screenshotHeight = cr.bottom - cr.top;
          SetToast(L"Screenshot requested (" + std::to_wstring(openGl_.screenshotWidth) + L"x" + std::to_wstring(openGl_.screenshotHeight) + L")");
          PushLog(L"OPENGL", L"INFO", L"Screenshot capture scheduled.", ColorRole::UserInput, L"opengl", L"screenshot", L"action=request");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == 'B' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (!paths_.borderFiles.empty()) {
          currentBorderIndex_ = (currentBorderIndex_ + 1) % static_cast<int>(paths_.borderFiles.size());
          {
            std::unique_lock<std::shared_mutex> lock(configMutex_);
            config_.borderImage = paths_.borderFiles[currentBorderIndex_].wstring();
          }
          std::wstring borderName = paths_.borderFiles[currentBorderIndex_].stem().wstring();
          PushLog(L"OPENGL", L"INFO", L"Border switched to " + borderName + L".", ColorRole::UserInput, L"opengl", L"border", L"name=" + borderName);
          SetToast(L"Border: " + borderName);
        } else {
          SetToast(L"No border images found");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_F11) {
        SpawnDemoLogs();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }



      return 0;
    }
    case WM_USER + 77: {
      std::lock_guard<std::recursive_mutex> lock(stateMutex_);
      monix::updater::UpdateInfo info;
      {
        std::lock_guard<std::mutex> infoLock(s_updateInfoMutex);
        if (!s_updateInfoReady) return 0;
        info = s_pendingUpdateInfo;
        s_updateInfoReady = false;
      }
      state_.updateState.checking = false;
      state_.updateState.updateAvailable = info.available;
      state_.updateState.latestVersion = info.latestVersion;
      state_.updateState.currentVersion = info.currentVersion;
      state_.updateState.releaseNotes = info.body;
      state_.updateState.downloadUrl = info.htmlUrl;
      state_.updateState.checkedThisSession = true;
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_TIMER: {
      RECT client {};
      GetClientRect(hwnd, &client);
      ComputeViewport(client);
      {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        TickAnimations(state_.viewport_);
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MONIX_UPDATE:
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps {};
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT client {};
      GetClientRect(hwnd, &client);
      LARGE_INTEGER frameStart, frameEnd, freq;
      QueryPerformanceFrequency(&freq);
      QueryPerformanceCounter(&frameStart);
      bool painted = RenderOpenGlFrame(client);
      if (!painted) {
        HDC memoryDc = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
        if (memoryDc && bitmap) {
          HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
          Render(memoryDc, client);
          BitBlt(hdc, 0, 0, client.right, client.bottom, memoryDc, 0, 0, SRCCOPY);
          SelectObject(memoryDc, oldBitmap);
        }
        if (bitmap) DeleteObject(bitmap);
        if (memoryDc) DeleteDC(memoryDc);
      }
      QueryPerformanceCounter(&frameEnd);
      {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        state_.snapshot.frameTimeMs = static_cast<double>(frameEnd.QuadPart - frameStart.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      KillTimer(hwnd, kFrameTimerId);
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

