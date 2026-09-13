#pragma once

#include "MonixConfigTypes.hpp"
#include "SettingsRegistry.hpp"
#include "../config/MonixConfig.hpp"
#include "../core/TextUtils.hpp"
#include "../ui/CoreMonitorTheme.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace monix {

inline SettingMutation AdjustSetting(Config& cfg, SettingId id, int direction,
                                     const std::vector<FontEntry>& fontList,
                                     int& currentFontIndex) {
  SettingMutation mutation;
  mutation.id = id;
  mutation.label = RegistrySettingLabel(id);
  const int step = direction < 0 ? -1 : 1;

  const auto changeBool = [&](bool& value) {
    value = !value;
    mutation.changed = true;
  };
  const auto changeUInt = [&](UINT& value, UINT delta, UINT minimum, UINT maximum) {
    const UINT candidate = step < 0 ?
      (value > minimum ? value - std::min(delta, value - minimum) : value) :
      (value < maximum ? std::min(maximum, value + delta) : value);
    if (candidate != value) {
      value = candidate;
      mutation.changed = true;
    }
  };
  const auto changeInt = [&](int& value, int delta, int minimum, int maximum) {
    const int candidate = std::clamp(value + (step * delta), minimum, maximum);
    if (candidate != value) {
      value = candidate;
      mutation.changed = true;
    }
  };
  const auto changeDouble = [&](double& value, double delta, double minimum, double maximum) {
    const double candidate = std::clamp(value + (step * delta), minimum, maximum);
    if (std::fabs(candidate - value) > 0.0005) {
      value = candidate;
      mutation.changed = true;
    }
  };

  switch (id) {
    case SettingId::TelemetryIntervalMs:
      changeUInt(cfg.telemetryIntervalMs, 25u, 50u, 5000u);
      mutation.requestRefresh = mutation.changed;
      break;
    case SettingId::FrameIntervalMs:
      changeUInt(cfg.frameIntervalMs, 1u, 16u, 100u);
      mutation.resetFrameTimer = mutation.changed;
      break;
    case SettingId::FontScale:
      changeDouble(cfg.fontScale, 0.05, 0.85, 1.8);
      mutation.recreateFonts = mutation.changed;
      break;
    case SettingId::IntroEnabled:
      changeBool(cfg.introEnabled);
      break;
    case SettingId::NotificationsEnabled:
      changeBool(cfg.notificationsEnabled);
      mutation.clearNotifications = mutation.changed && !cfg.notificationsEnabled;
      break;
    case SettingId::SoundEnabled:
      changeBool(cfg.soundEnabled);
      break;
    case SettingId::NotificationDurationMs:
      changeUInt(cfg.notificationDurationMs, 200u, 1000u, 12000u);
      break;
    case SettingId::NotificationMaxStack:
      changeInt(cfg.notificationMaxStack, 1, 1, 8);
      break;
    case SettingId::AnalyticsHistoryEnabled:
      changeBool(cfg.analyticsHistoryEnabled);
      break;
    case SettingId::HistoryCapacity:
      changeInt(cfg.historyCapacity, 12, 24, 720);
      mutation.trimHistory = mutation.changed;
      break;
    case SettingId::LogVisibleLines:
      changeInt(cfg.logVisibleLines, 1, 8, 48);
      break;
    case SettingId::LogLevel: {
      int index = LogLevelRank(cfg.logLevel);
      index = std::clamp(index + step, 0, 4);
      const LogLevel next = static_cast<LogLevel>(index);
      if (next != cfg.logLevel) {
        cfg.logLevel = next;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::LogViewMode: {
      int index = static_cast<int>(cfg.logViewMode);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      const LogViewMode next = static_cast<LogViewMode>(index);
      if (next != cfg.logViewMode) {
        cfg.logViewMode = next;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::LogMilliseconds:
      changeBool(cfg.logMilliseconds);
      break;
    case SettingId::LogJsonEnabled:
      changeBool(cfg.logJsonEnabled);
      break;
    case SettingId::LogPlainEnabled:
      changeBool(cfg.logPlainEnabled);
      break;
    case SettingId::LogFlushIntervalMs:
      changeUInt(cfg.logFlushIntervalMs, 100u, 250u, 10000u);
      break;
    case SettingId::LogRetentionDays:
      changeUInt(cfg.logRetentionDays, 1u, 1u, 90u);
      break;
    case SettingId::LogDeduplicate:
      changeBool(cfg.logDeduplicate);
      break;
    case SettingId::PauseLiveLogs:
      changeBool(cfg.pauseLiveLogs);
      break;
    case SettingId::CrtEnabled:
      changeBool(cfg.crtEnabled);
      break;
    case SettingId::CrtCurvatureStrength:
      changeDouble(cfg.crtCurvatureStrength, 0.02, 0.0, 0.6);
      break;
    case SettingId::CrtScanlineIntensity:
      changeDouble(cfg.crtScanlineIntensity, 0.02, 0.0, 0.6);
      break;
    case SettingId::CrtScanlineSpacing:
      changeInt(cfg.crtScanlineSpacing, 1, 2, 8);
      break;
    case SettingId::CrtChromaticAberration:
      changeDouble(cfg.crtChromaticAberration, 0.005, 0.0, 0.16);
      break;
    case SettingId::CrtPhosphorGlow:
      changeDouble(cfg.crtPhosphorGlow, 0.01, 0.0, 0.5);
      break;
    case SettingId::CrtFlickerAmount:
      changeDouble(cfg.crtFlickerAmount, 0.002, 0.0, 0.08);
      break;
    case SettingId::CrtNoiseAmount:
      changeDouble(cfg.crtNoiseAmount, 0.002, 0.0, 0.08);
      break;
    case SettingId::CrtVignetteStrength:
      changeDouble(cfg.crtVignetteStrength, 0.02, 0.0, 0.6);
      break;
    case SettingId::CrtSharpness:
      changeDouble(cfg.crtSharpness, 0.02, 0.0, 1.0);
      break;
    case SettingId::CrtGhostOffset:
      changeInt(cfg.crtGhostOffset, 1, 0, 2);
      break;
    case SettingId::IntroStepPx:
      changeInt(cfg.introStepPx, 2, 4, 64);
      break;
    case SettingId::IntroHoldMs:
      changeUInt(cfg.introHoldMs, 100u, 200u, 5000u);
      break;
    case SettingId::IntroCreditDelayMs:
      changeUInt(cfg.introCreditDelayMs, 50u, 0u, 2000u);
      break;
    case SettingId::BorderEnabled:
      changeBool(cfg.borderEnabled);
      break;
    case SettingId::BorderChromaKeyR:
      changeDouble(cfg.borderChromaKeyR, 0.05, 0.0, 1.0);
      break;
    case SettingId::BorderChromaKeyG:
      changeDouble(cfg.borderChromaKeyG, 0.05, 0.0, 1.0);
      break;
    case SettingId::BorderChromaKeyB:
      changeDouble(cfg.borderChromaKeyB, 0.05, 0.0, 1.0);
      break;
    case SettingId::BorderChromaKeyTolerance:
      changeDouble(cfg.borderChromaKeyTolerance, 0.05, 0.0, 1.0);
      break;
    case SettingId::CrtRgbShift:
      changeDouble(cfg.crtRgbShift, 0.005, 0.0, 0.1);
      break;
    case SettingId::CrtGrain:
      changeDouble(cfg.crtGrain, 0.005, 0.0, 0.1);
      break;
    case SettingId::CrtJitter:
      changeDouble(cfg.crtJitter, 0.01, 0.0, 1.0);
      break;
    case SettingId::CrtSubpixelMode:
      changeDouble(cfg.crtSubpixelMode, 0.1, 0.0, 1.0);
      break;
    case SettingId::CrtBloom:
      changeDouble(cfg.crtBloom, 0.02, 0.0, 0.5);
      break;
    case SettingId::CrtBurnInEnabled:
      changeBool(cfg.crtBurnInEnabled);
      break;
    case SettingId::CrtBurnInIntensity:
      changeDouble(cfg.crtBurnInIntensity, 0.02, 0.0, 1.0);
      break;
    case SettingId::CrtBurnInDecayRate:
      changeDouble(cfg.crtBurnInDecayRate, 0.1, 0.01, 10.0);
      break;
    case SettingId::AccentColorPreset: {
      int index = std::clamp(cfg.accentColorPreset, 0, 5);
      index += step;
      if (index < 0) index = 5;
      if (index > 5) index = 0;
      if (index != cfg.accentColorPreset) {
        cfg.accentColorPreset = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::FontFaceIndex: {
      if (!fontList.empty()) {
        int idx = currentFontIndex + step;
        if (idx < 0) idx = static_cast<int>(fontList.size()) - 1;
        if (idx >= static_cast<int>(fontList.size())) idx = 0;
        if (idx != currentFontIndex) {
          currentFontIndex = idx;
          mutation.changed = true;
        }
      }
      mutation.recreateFonts = mutation.changed;
      break;
    }
    case SettingId::DefaultTab: {
      int index = std::clamp(cfg.defaultTab, 0, 5);
      index += step;
      if (index < 0) index = 5;
      if (index > 5) index = 0;
      if (index != cfg.defaultTab) {
        cfg.defaultTab = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::MinimizeToTray:
      changeBool(cfg.minimizeToTray);
      break;
    case SettingId::StartMaximized:
      changeBool(cfg.startMaximized);
      break;
    case SettingId::SaveWindowPosition:
      changeBool(cfg.saveWindowPosition);
      break;
    case SettingId::HotkeyToggleCrt: {
      int vk = cfg.hotkeyToggleCrt;
      vk += (step > 0) ? 1 : -1;
      if (vk < 0) vk = 0;
      if (vk > 0xFF) vk = 0;
      if (vk != cfg.hotkeyToggleCrt) {
        cfg.hotkeyToggleCrt = vk;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::HotkeyReloadConfig: {
      int vk = cfg.hotkeyReloadConfig;
      vk += (step > 0) ? 1 : -1;
      if (vk < 0) vk = 0;
      if (vk > 0xFF) vk = 0;
      if (vk != cfg.hotkeyReloadConfig) {
        cfg.hotkeyReloadConfig = vk;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::CrtBrightness:
      changeDouble(cfg.crtBrightness, 0.05, 0.1, 2.0);
      break;
    case SettingId::CrtContrast:
      changeDouble(cfg.crtContrast, 0.05, 0.1, 2.0);
      break;
    case SettingId::CrtSaturation:
      changeDouble(cfg.crtSaturation, 0.05, 0.0, 3.0);
      break;
    case SettingId::CrtGamma:
      changeDouble(cfg.crtGamma, 0.05, 0.1, 3.0);
      break;
    case SettingId::CrtScanlineMode: {
      int index = std::clamp(cfg.crtScanlineMode, 0, 2);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      if (index != cfg.crtScanlineMode) {
        cfg.crtScanlineMode = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::CrtInterlace:
      cfg.crtInterlace = (cfg.crtInterlace != 0) ? 0 : 1;
      mutation.changed = true;
      break;
    case SettingId::BorderImagePreset: {
      int index = std::clamp(cfg.borderImagePreset, 0, 4);
      index += step;
      if (index < 0) index = 4;
      if (index > 4) index = 0;
      if (index != cfg.borderImagePreset) {
        cfg.borderImagePreset = index;
        const wchar_t* names[] = { L"CRT2", L"CRT3", L"WOOD", L"METAL", L"" };
        if (index >= 0 && index <= 4) {
          if (index < 4)
            cfg.borderImage = std::wstring(L"borders\\") + names[index] + L".png";
          else
            cfg.borderImage = L"";
        }
        mutation.changed = true;
      }
      break;
    }
    case SettingId::Language: {
      int index = std::clamp(cfg.language, 0, 2);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      if (index != cfg.language) {
        cfg.language = index;
        mutation.changed = true;
        mutation.recreateFonts = true;
      }
      break;
    }
    case SettingId::ShowFps:
      cfg.showFps = !cfg.showFps;
      mutation.changed = true;
      break;
    case SettingId::WindowOpacity:
      changeDouble(cfg.windowOpacity, 0.05, 0.3, 1.0);
      break;
    case SettingId::AlwaysOnTop:
      cfg.alwaysOnTop = !cfg.alwaysOnTop;
      mutation.changed = true;
      break;
    case SettingId::StartWithWindows:
      cfg.startWithWindows = !cfg.startWithWindows;
      mutation.changed = true;
      break;
    case SettingId::ThemeMode: {
      int index = std::clamp(cfg.themeMode, 0, monix::ui::kModernThemeMode);
      index += step;
      if (index < 0) index = monix::ui::kModernThemeMode;
      if (index > monix::ui::kModernThemeMode) index = 0;
      if (index != cfg.themeMode) {
        cfg.themeMode = index;
        mutation.changed = true;
        mutation.recreateFonts = true;
      }
      break;
    }
    case SettingId::FrameTargetFps: {
      int index = std::clamp(cfg.frameTargetFps, 0, 120);
      if (index == 0) index = 30;
      else if (index == 30) index = 60;
      else if (index == 60) index = 120;
      else index = 0;
      cfg.frameTargetFps = index;
      mutation.changed = true;
      break;
    }
    case SettingId::VSyncEnabled:
      cfg.vSyncEnabled = !cfg.vSyncEnabled;
      mutation.changed = true;
      break;
    case SettingId::ProcessPriority: {
      int index = std::clamp(cfg.processPriority, 0, 2);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      if (index != cfg.processPriority) {
        cfg.processPriority = index;
        mutation.changed = true;
      }
      break;
    }
  }

  return mutation;
}

}
