#include "ui/render/RenderCoreMonitor.hpp"
#include "ui/CoreMonitorTheme.hpp"
#include "ui/Win98Theme.hpp"

bool MonixApp::IsCoreMonitorThemeActive() const {
  return config_.themeMode >= monix::ui::kCoreMonitorThemeMode;
}

void MonixApp::RenderCoreMonitorTheme(HDC dc, const RECT& clientRect) {
  if (config_.themeMode == monix::ui::kWin98ThemeMode) {
    monix::ui::CoreMonitorThemeContext ctx;
    ctx.snapshot = &state_.snapshot;
    ctx.logs = &state_.logState.entries;
    ctx.counters = &state_.logState.counters;
    ctx.cpuHistory = &state_.history.cpu;
    ctx.ramHistory = &state_.history.ram;
    ctx.gpuHistory = &state_.history.gpu;
    ctx.netHistory = &state_.history.net;
    ctx.netUploadHistory = &state_.history.netUpload;
    ctx.config = config_;
    ctx.fonts.title = titleFont_;
    ctx.fonts.body = bodyFont_;
    ctx.fonts.smallText = smallFont_;
    ctx.fonts.logText = logFont_;
    ctx.fonts.bodyLineHeight = bodyLineHeight_;
    ctx.fonts.smallLineHeight = smallLineHeight_;
    ctx.fonts.logLineHeight = logLineHeight_;
    ctx.rendererName = openGl_.renderer.empty() ? L"VULKAN 1.3" : openGl_.renderer;
    ctx.shaderName = paths_.crtShaderFile.empty() ? L"crt-lottes-with-bezel" : paths_.crtShaderFile.stem().wstring();
    ctx.fontName = L"MS Sans Serif";
    if (!paths_.fontList.empty() && state_.currentFontIndex >= 0 && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
      ctx.fontName = paths_.fontList[state_.currentFontIndex].displayName;
    }
    ctx.menuIndex = state_.coreMonitorMenuIndex;
    ctx.settingsCategory = state_.settingsCategory;
    ctx.selectedTaskIndex = state_.selectedTaskIndex;
    ctx.selectedTaskPid = state_.selectedTaskPid;
    ctx.taskScroll = state_.taskScroll;
    ctx.livePaused = state_.logState.livePaused || config_.pauseLiveLogs;
    ctx.frameCount = openGl_.frameCount;
    ctx.updateState = &state_.updateState;
    ctx.activeFilter = static_cast<int>(state_.logState.activeFilter);
    ctx.pressedButton = state_.pressedButton;
    ctx.hoveredMenuIndex = state_.hoveredMenuIndex;

    if (win98Assets_.windowBase == nullptr) {
      monix::ui::Win98Theme::LoadAssets(win98Assets_, paths_.rootDir);
    }
    monix::ui::Win98Theme::Render(dc, clientRect, ctx, win98Fonts_, win98Assets_, paths_.rootDir);
    return;
  }

  monix::ui::CoreMonitorThemeContext ctx;
  ctx.snapshot = &state_.snapshot;
  ctx.logs = &state_.logState.entries;
  ctx.counters = &state_.logState.counters;
  ctx.cpuHistory = &state_.history.cpu;
  ctx.ramHistory = &state_.history.ram;
  ctx.gpuHistory = &state_.history.gpu;
  ctx.netHistory = &state_.history.net;
  ctx.netUploadHistory = &state_.history.netUpload;
  ctx.config = config_;
  ctx.fonts.title = titleFont_;
  ctx.fonts.body = bodyFont_;
  ctx.fonts.smallText = smallFont_;
  ctx.fonts.logText = logFont_;
  ctx.fonts.bodyLineHeight = bodyLineHeight_;
  ctx.fonts.smallLineHeight = smallLineHeight_;
  ctx.fonts.logLineHeight = logLineHeight_;
  ctx.rendererName = openGl_.renderer.empty() ? L"VULKAN 1.3" : openGl_.renderer;
  ctx.shaderName = paths_.crtShaderFile.empty() ? L"crt-lottes-with-bezel" : paths_.crtShaderFile.stem().wstring();
  ctx.fontName = L"Terminal";
  if (!paths_.fontList.empty() && state_.currentFontIndex >= 0 && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
    ctx.fontName = paths_.fontList[state_.currentFontIndex].displayName;
  }
  ctx.menuIndex = state_.coreMonitorMenuIndex;
  ctx.settingsCategory = state_.settingsCategory;
  ctx.selectedTaskIndex = state_.selectedTaskIndex;
  ctx.selectedTaskPid = state_.selectedTaskPid;
  ctx.taskScroll = state_.taskScroll;
  ctx.livePaused = state_.logState.livePaused || config_.pauseLiveLogs;
  ctx.frameCount = openGl_.frameCount;
  ctx.updateState = &state_.updateState;
  ctx.activeFilter = static_cast<int>(state_.logState.activeFilter);
  ctx.pressedButton = state_.pressedButton;
  ctx.hoveredMenuIndex = state_.hoveredMenuIndex;
  monix::ui::CoreMonitorTheme::Render(dc, clientRect, ctx);
}
