#pragma once

#include "settings/MonixConfigTypes.hpp"

#include <array>

namespace monix {

inline const std::array<SettingId, 10>& RuntimeSettings() {
  static const std::array<SettingId, 10> settings {
    SettingId::TelemetryIntervalMs,
    SettingId::FrameIntervalMs,
    SettingId::FontScale,
    SettingId::IntroEnabled,
    SettingId::NotificationsEnabled,
    SettingId::SoundEnabled,
    SettingId::NotificationDurationMs,
    SettingId::NotificationMaxStack,
    SettingId::AnalyticsHistoryEnabled,
    SettingId::HistoryCapacity
  };
  return settings;
}

inline const std::array<SettingId, 10>& LoggingSettings() {
  static const std::array<SettingId, 10> settings {
    SettingId::LogVisibleLines,
    SettingId::LogLevel,
    SettingId::LogViewMode,
    SettingId::LogMilliseconds,
    SettingId::LogJsonEnabled,
    SettingId::LogPlainEnabled,
    SettingId::LogFlushIntervalMs,
    SettingId::LogRetentionDays,
    SettingId::LogDeduplicate,
    SettingId::PauseLiveLogs
  };
  return settings;
}

inline const std::array<SettingId, 11>& FullCrtSettings() {
  static const std::array<SettingId, 11> settings {
    SettingId::CrtEnabled,
    SettingId::CrtCurvatureStrength,
    SettingId::CrtScanlineIntensity,
    SettingId::CrtScanlineSpacing,
    SettingId::CrtChromaticAberration,
    SettingId::CrtPhosphorGlow,
    SettingId::CrtFlickerAmount,
    SettingId::CrtNoiseAmount,
    SettingId::CrtVignetteStrength,
    SettingId::CrtSharpness,
    SettingId::CrtGhostOffset
  };
  return settings;
}

inline const std::array<SettingId, 5>& AdvancedCrtSettings() {
  static const std::array<SettingId, 5> settings {
    SettingId::CrtRgbShift,
    SettingId::CrtGrain,
    SettingId::CrtJitter,
    SettingId::CrtSubpixelMode,
    SettingId::CrtBloom
  };
  return settings;
}

inline const std::array<SettingId, 3>& BurnInSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::CrtBurnInEnabled,
    SettingId::CrtBurnInIntensity,
    SettingId::CrtBurnInDecayRate
  };
  return settings;
}

inline const std::array<SettingId, 6>& BorderSettings() {
  static const std::array<SettingId, 6> settings {
    SettingId::BorderEnabled,
    SettingId::BorderImagePreset,
    SettingId::BorderChromaKeyR,
    SettingId::BorderChromaKeyG,
    SettingId::BorderChromaKeyB,
    SettingId::BorderChromaKeyTolerance
  };
  return settings;
}

inline const std::array<SettingId, 3>& IntroAnimSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::IntroStepPx,
    SettingId::IntroHoldMs,
    SettingId::IntroCreditDelayMs
  };
  return settings;
}

inline const std::array<SettingId, 2>& ThemeSettings() {
  static const std::array<SettingId, 2> settings {
    SettingId::AccentColorPreset,
    SettingId::FontFaceIndex
  };
  return settings;
}

inline const std::array<SettingId, 4>& WindowStartupSettings() {
  static const std::array<SettingId, 4> settings {
    SettingId::DefaultTab,
    SettingId::MinimizeToTray,
    SettingId::StartMaximized,
    SettingId::SaveWindowPosition
  };
  return settings;
}

inline const std::array<SettingId, 2>& HotkeySettings() {
  static const std::array<SettingId, 2> settings {
    SettingId::HotkeyToggleCrt,
    SettingId::HotkeyReloadConfig
  };
  return settings;
}

inline const std::array<SettingId, 4>& ColorCorrectionSettings() {
  static const std::array<SettingId, 4> settings {
    SettingId::CrtBrightness,
    SettingId::CrtContrast,
    SettingId::CrtSaturation,
    SettingId::CrtGamma
  };
  return settings;
}

inline const std::array<SettingId, 2>& ScanlineStyleSettings() {
  static const std::array<SettingId, 2> settings {
    SettingId::CrtScanlineMode,
    SettingId::CrtInterlace
  };
  return settings;
}

inline const std::array<SettingId, 3>& GeneralExtendedSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::Language,
    SettingId::ShowFps,
    SettingId::WindowOpacity
  };
  return settings;
}

inline const std::array<SettingId, 3>& SystemExtendedSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::AlwaysOnTop,
    SettingId::StartWithWindows,
    SettingId::ThemeMode
  };
  return settings;
}

inline const std::array<SettingId, 3>& PerformanceSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::FrameTargetFps,
    SettingId::VSyncEnabled,
    SettingId::ProcessPriority
  };
  return settings;
}

inline const std::array<SettingId, 71>& EditableSettings() {
  static const std::array<SettingId, 71> settings {
    SettingId::TelemetryIntervalMs,
    SettingId::FrameIntervalMs,
    SettingId::FontScale,
    SettingId::IntroEnabled,
    SettingId::IntroStepPx,
    SettingId::IntroHoldMs,
    SettingId::IntroCreditDelayMs,
    SettingId::NotificationsEnabled,
    SettingId::SoundEnabled,
    SettingId::NotificationDurationMs,
    SettingId::NotificationMaxStack,
    SettingId::AnalyticsHistoryEnabled,
    SettingId::HistoryCapacity,
    SettingId::LogVisibleLines,
    SettingId::LogLevel,
    SettingId::LogViewMode,
    SettingId::LogMilliseconds,
    SettingId::LogJsonEnabled,
    SettingId::LogPlainEnabled,
    SettingId::LogFlushIntervalMs,
    SettingId::LogRetentionDays,
    SettingId::LogDeduplicate,
    SettingId::PauseLiveLogs,
    SettingId::CrtEnabled,
    SettingId::CrtCurvatureStrength,
    SettingId::CrtScanlineIntensity,
    SettingId::CrtScanlineSpacing,
    SettingId::CrtChromaticAberration,
    SettingId::CrtPhosphorGlow,
    SettingId::CrtFlickerAmount,
    SettingId::CrtNoiseAmount,
    SettingId::CrtVignetteStrength,
    SettingId::CrtSharpness,
    SettingId::CrtGhostOffset,
    SettingId::CrtRgbShift,
    SettingId::CrtGrain,
    SettingId::CrtJitter,
    SettingId::CrtSubpixelMode,
    SettingId::CrtBloom,
    SettingId::CrtBurnInEnabled,
    SettingId::CrtBurnInIntensity,
    SettingId::CrtBurnInDecayRate,
    SettingId::BorderEnabled,
    SettingId::BorderChromaKeyR,
    SettingId::BorderChromaKeyG,
    SettingId::BorderChromaKeyB,
    SettingId::BorderChromaKeyTolerance,
    SettingId::BorderImagePreset,
    SettingId::AccentColorPreset,
    SettingId::FontFaceIndex,
    SettingId::DefaultTab,
    SettingId::MinimizeToTray,
    SettingId::StartMaximized,
    SettingId::SaveWindowPosition,
    SettingId::AlwaysOnTop,
    SettingId::StartWithWindows,
    SettingId::ThemeMode,
    SettingId::HotkeyToggleCrt,
    SettingId::HotkeyReloadConfig,
    SettingId::CrtBrightness,
    SettingId::CrtContrast,
    SettingId::CrtSaturation,
    SettingId::CrtGamma,
    SettingId::CrtScanlineMode,
    SettingId::CrtInterlace,
    SettingId::Language,
    SettingId::ShowFps,
    SettingId::WindowOpacity,
    SettingId::FrameTargetFps,
    SettingId::VSyncEnabled,
    SettingId::ProcessPriority
  };
  return settings;
}

} // namespace monix
