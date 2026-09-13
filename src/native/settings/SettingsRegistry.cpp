#include "SettingsRegistry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>

namespace monix {

namespace {

const wchar_t* kAccentColorNames[] = { L"GREEN", L"BLUE", L"RED", L"PURPLE", L"AMBER", L"CYAN" };
const wchar_t* kDefaultTabNames[] = { L"LOG", L"TASKS", L"HARDWARE", L"NETWORK", L"SCRAM", L"SETTINGS" };
const wchar_t* kCrtScanlineModeNames[] = { L"FLAT", L"CURVED", L"DIAMOND" };
const wchar_t* kBorderImagePresetNames[] = { L"CRT2", L"CRT3", L"WOOD", L"METAL", L"NONE" };
const wchar_t* kLanguageNames[] = { L"ENGLISH", L"ESPANOL", L"FRANCAIS" };
const wchar_t* kThemeModeNames[] = { L"DARK", L"LIGHT", L"SYSTEM" };
const wchar_t* kProcessPriorityNames[] = { L"NORMAL", L"HIGH", L"REALTIME" };

std::string ToLowerAscii(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string TrimAscii(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool TryParseBool(const std::string& value, bool& out) {
  std::string lower = ToLowerAscii(value);
  if (lower == "true" || lower == "1" || lower == "yes") { out = true; return true; }
  if (lower == "false" || lower == "0" || lower == "no") { out = false; return true; }
  return false;
}

bool TryParseInt(const std::string& value, int& out) {
  try { size_t pos; out = std::stoi(value, &pos); return pos == value.size(); }
  catch (...) { return false; }
}

bool TryParseUInt(const std::string& value, unsigned int& out) {
  try { size_t pos; long v = std::stol(value, &pos); if (v < 0 || pos != value.size()) return false; out = static_cast<unsigned int>(v); return true; }
  catch (...) { return false; }
}

bool TryParseDouble(const std::string& value, double& out) {
  try { size_t pos; out = std::stod(value, &pos); return pos == value.size(); }
  catch (...) { return false; }
}

}  // namespace

const std::array<SettingDef, kSettingCount>& BuildRegistry() {
  static const std::array<SettingDef, kSettingCount> s_registry = {{
      {
        SettingId::TelemetryIntervalMs,
        L"Refresh Delay",
        "delay_ms",
        SettingType::UInt,
        50.0, 5000.0, 25.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::FrameIntervalMs,
        L"Frame Interval",
        "frame_interval_ms",
        SettingType::UInt,
        16.0, 100.0, 1.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::FontScale,
        L"Font Scale",
        "font_scale",
        SettingType::Double,
        0.85, 1.8, 0.05, 2, nullptr, 0, nullptr
      },
      {
        SettingId::IntroEnabled,
        L"Intro",
        "intro_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::NotificationsEnabled,
        L"Notifications",
        "notifications_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::SoundEnabled,
        L"Alert Sound",
        "sound_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::NotificationDurationMs,
        L"Toast Duration",
        "notification_duration_ms",
        SettingType::UInt,
        1000.0, 12000.0, 200.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::NotificationMaxStack,
        L"Toast Stack",
        "notification_max_stack",
        SettingType::Int,
        1.0, 8.0, 1.0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::AnalyticsHistoryEnabled,
        L"History",
        "analytics_history_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::HistoryCapacity,
        L"History Capacity",
        "history_capacity",
        SettingType::Int,
        24.0, 720.0, 12.0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogVisibleLines,
        L"Visible Log Lines",
        "log_visible_lines",
        SettingType::Int,
        8.0, 48.0, 1.0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogLevel,
        L"Log Level",
        "log_level",
        SettingType::Special,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogViewMode,
        L"Log View",
        "log_view",
        SettingType::Special,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogMilliseconds,
        L"Milliseconds",
        "log_milliseconds",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogJsonEnabled,
        L"JSONL Output",
        "log_json_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogPlainEnabled,
        L"Plain Log Output",
        "log_plain_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::LogFlushIntervalMs,
        L"Flush Interval",
        "log_flush_interval_ms",
        SettingType::UInt,
        250.0, 10000.0, 100.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::LogRetentionDays,
        L"Retention",
        "log_retention_days",
        SettingType::UInt,
        1.0, 90.0, 1.0, 0, L" days", 0, nullptr
      },
      {
        SettingId::LogDeduplicate,
        L"Deduplicate",
        "log_deduplicate",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::PauseLiveLogs,
        L"Pause Live Logs",
        "pause_live_logs",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::CrtEnabled,
        L"CRT Postprocess",
        "crt_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::CrtCurvatureStrength,
        L"Curvature",
        "crt_curvature_strength",
        SettingType::Double,
        0.0, 0.6, 0.02, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtScanlineIntensity,
        L"Scanline Intensity",
        "crt_scanline_intensity",
        SettingType::Double,
        0.0, 0.6, 0.02, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtScanlineSpacing,
        L"Scanline Spacing",
        "crt_scanline_spacing",
        SettingType::Int,
        2.0, 8.0, 1.0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::CrtChromaticAberration,
        L"RGB Separation",
        "crt_chromatic_aberration",
        SettingType::Double,
        0.0, 0.16, 0.005, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtPhosphorGlow,
        L"Phosphor Glow",
        "crt_phosphor_glow",
        SettingType::Double,
        0.0, 0.5, 0.01, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtFlickerAmount,
        L"Analog Flicker",
        "crt_flicker_amount",
        SettingType::Double,
        0.0, 0.08, 0.002, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtNoiseAmount,
        L"Analog Noise",
        "crt_noise_amount",
        SettingType::Double,
        0.0, 0.08, 0.002, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtVignetteStrength,
        L"CRT Vignette",
        "crt_vignette_strength",
        SettingType::Double,
        0.0, 0.6, 0.02, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtSharpness,
        L"Pixel Softness",
        "crt_sharpness",
        SettingType::Double,
        0.0, 1.0, 0.02, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtGhostOffset,
        L"Glow Offset",
        "crt_ghost_offset",
        SettingType::Int,
        0.0, 2.0, 1.0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::IntroStepPx,
        L"Intro Step",
        "intro_step_px",
        SettingType::Int,
        4.0, 64.0, 2.0, 0, L" px", 0, nullptr
      },
      {
        SettingId::IntroHoldMs,
        L"Intro Hold",
        "intro_hold_ms",
        SettingType::UInt,
        200.0, 5000.0, 100.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::IntroCreditDelayMs,
        L"Credit Delay",
        "intro_credit_delay_ms",
        SettingType::UInt,
        0.0, 2000.0, 50.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::BorderEnabled,
        L"Border Overlay",
        "border_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::BorderChromaKeyR,
        L"Chroma Key R",
        "border_key_r",
        SettingType::Double,
        0.0, 1.0, 0.05, 3, nullptr, 0, nullptr
      },
      {
        SettingId::BorderChromaKeyG,
        L"Chroma Key G",
        "border_key_g",
        SettingType::Double,
        0.0, 1.0, 0.05, 3, nullptr, 0, nullptr
      },
      {
        SettingId::BorderChromaKeyB,
        L"Chroma Key B",
        "border_key_b",
        SettingType::Double,
        0.0, 1.0, 0.05, 3, nullptr, 0, nullptr
      },
      {
        SettingId::BorderChromaKeyTolerance,
        L"Chroma Tolerance",
        "border_key_tolerance",
        SettingType::Double,
        0.0, 1.0, 0.05, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtRgbShift,
        L"RGB Shift",
        "crt_rgb_shift",
        SettingType::Double,
        0.0, 0.1, 0.005, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtGrain,
        L"Film Grain",
        "crt_grain",
        SettingType::Double,
        0.0, 0.1, 0.005, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtJitter,
        L"Signal Jitter",
        "crt_jitter",
        SettingType::Double,
        0.0, 1.0, 0.01, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtSubpixelMode,
        L"Subpixel Mode",
        "crt_subpixel_mode",
        SettingType::Double,
        0.0, 1.0, 0.1, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtBloom,
        L"Bloom",
        "crt_bloom",
        SettingType::Double,
        0.0, 0.5, 0.02, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtBurnInEnabled,
        L"Burn-In",
        "crt_burn_in_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::CrtBurnInIntensity,
        L"Burn-In Intensity",
        "crt_burn_in_intensity",
        SettingType::Double,
        0.0, 1.0, 0.02, 3, nullptr, 0, nullptr
      },
      {
        SettingId::CrtBurnInDecayRate,
        L"Burn-In Decay",
        "crt_burn_in_decay_rate",
        SettingType::Double,
        0.01, 10.0, 0.1, 2, nullptr, 0, nullptr
      },
      {
        SettingId::AccentColorPreset,
        L"Accent Color",
        "accent_color_preset",
        SettingType::Enum,
        0.0, 5.0, 1.0, 0, nullptr, 6, kAccentColorNames
      },
      {
        SettingId::FontFaceIndex,
        L"Font Face",
        "font_face_index",
        SettingType::Special,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::DefaultTab,
        L"Default Tab",
        "default_tab",
        SettingType::Enum,
        0.0, 5.0, 1.0, 0, nullptr, 6, kDefaultTabNames
      },
      {
        SettingId::MinimizeToTray,
        L"Minimize to Tray",
        "minimize_to_tray",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::StartMaximized,
        L"Start Maximized",
        "start_maximized",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::SaveWindowPosition,
        L"Save Window Pos",
        "save_window_position",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::HotkeyToggleCrt,
        L"Hotkey: Toggle CRT",
        "hotkey_toggle_crt",
        SettingType::Hotkey,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::HotkeyReloadConfig,
        L"Hotkey: Reload Config",
        "hotkey_reload_config",
        SettingType::Hotkey,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::CrtBrightness,
        L"Brightness",
        "crt_brightness",
        SettingType::Double,
        0.1, 2.0, 0.05, 2, nullptr, 0, nullptr
      },
      {
        SettingId::CrtContrast,
        L"Contrast",
        "crt_contrast",
        SettingType::Double,
        0.1, 2.0, 0.05, 2, nullptr, 0, nullptr
      },
      {
        SettingId::CrtSaturation,
        L"Saturation",
        "crt_saturation",
        SettingType::Double,
        0.0, 3.0, 0.05, 2, nullptr, 0, nullptr
      },
      {
        SettingId::CrtGamma,
        L"Gamma",
        "crt_gamma",
        SettingType::Double,
        0.1, 3.0, 0.05, 2, nullptr, 0, nullptr
      },
      {
        SettingId::CrtScanlineMode,
        L"Scanline Mode",
        "crt_scanline_mode",
        SettingType::Enum,
        0.0, 2.0, 1.0, 0, nullptr, 3, kCrtScanlineModeNames
      },
      {
        SettingId::CrtInterlace,
        L"Interlace",
        "crt_interlace",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::BorderImagePreset,
        L"Border Preset",
        "border_image_preset",
        SettingType::Enum,
        0.0, 4.0, 1.0, 0, nullptr, 5, kBorderImagePresetNames
      },
      {
        SettingId::Language,
        L"Language",
        "language",
        SettingType::Enum,
        0.0, 2.0, 1.0, 0, nullptr, 3, kLanguageNames
      },
      {
        SettingId::ShowFps,
        L"Show FPS",
        "show_fps",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::WindowOpacity,
        L"Window Opacity",
        "window_opacity",
        SettingType::Double,
        0.3, 1.0, 0.05, 3, nullptr, 0, nullptr
      },
      {
        SettingId::AlwaysOnTop,
        L"Always On Top",
        "always_on_top",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::StartWithWindows,
        L"Start with Windows",
        "start_with_windows",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::ThemeMode,
        L"Theme",
        "theme_mode",
        SettingType::Enum,
        0.0, 2.0, 1.0, 0, nullptr, 3, kThemeModeNames
      },
      {
        SettingId::FrameTargetFps,
        L"Target FPS",
        "frame_target_fps",
        SettingType::Special,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::VSyncEnabled,
        L"VSync",
        "vsync_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::ProcessPriority,
        L"Process Priority",
        "process_priority",
        SettingType::Enum,
        0.0, 2.0, 1.0, 0, nullptr, 3, kProcessPriorityNames
      },
      {
        SettingId::TransitionEnabled,
        L"Tab Transitions",
        "transition_enabled",
        SettingType::Bool,
        0, 0, 0, 0, nullptr, 0, nullptr
      },
      {
        SettingId::TransitionDurationMs,
        L"Transition Duration",
        "transition_duration_ms",
        SettingType::Double,
        50.0, 1000.0, 10.0, 0, L" ms", 0, nullptr
      },
      {
        SettingId::TransitionScanlineWidth,
        L"Scanline Width",
        "transition_scanline_width",
        SettingType::Double,
        0.002, 0.05, 0.001, 4, nullptr, 0, nullptr
      },
      {
        SettingId::TransitionIntensity,
        L"Transition Intensity",
        "transition_intensity",
        SettingType::Double,
        0.1, 2.0, 0.05, 3, nullptr, 0, nullptr
      },
      {
        SettingId::TransitionDistortion,
        L"Scanline Distortion",
        "transition_distortion",
        SettingType::Double,
        0.0, 0.02, 0.001, 4, nullptr, 0, nullptr
      },
      {
        SettingId::TransitionNoise,
        L"Transition Noise",
        "transition_noise",
        SettingType::Double,
        0.0, 0.1, 0.005, 4, nullptr, 0, nullptr
      },
      {
        SettingId::TransitionFlicker,
        L"Transition Flicker",
        "transition_flicker",
        SettingType::Double,
        0.0, 0.1, 0.005, 4, nullptr, 0, nullptr
      },
  }};
  return s_registry;
}

const std::array<SettingDef, kSettingCount>& GetSettingsRegistry() {
  return BuildRegistry();
}

const SettingDef* FindSettingDef(SettingId id) {
  const auto& reg = GetSettingsRegistry();
  for (const auto& def : reg) {
    if (def.id == id) return &def;
  }
  return nullptr;
}

std::wstring RegistrySettingLabel(SettingId id) {
  const SettingDef* def = FindSettingDef(id);
  if (!def) return L"";
  return def->label ? def->label : L"";
}

std::wstring RegistrySettingValueText(SettingId id, const Config& config) {
  const SettingDef* def = FindSettingDef(id);
  if (!def) return L"";
  switch (id) {
    case SettingId::TelemetryIntervalMs: {
      return std::to_wstring(static_cast<unsigned int>(config.telemetryIntervalMs)) + L" ms";
    }
    case SettingId::FrameIntervalMs: {
      return std::to_wstring(static_cast<unsigned int>(config.frameIntervalMs)) + L" ms";
    }
    case SettingId::FontScale: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.2f", static_cast<double>(config.fontScale)); return buf; }
    }
    case SettingId::IntroEnabled: {
      return config.introEnabled ? L"YES" : L"NO";
    }
    case SettingId::NotificationsEnabled: {
      return config.notificationsEnabled ? L"YES" : L"NO";
    }
    case SettingId::SoundEnabled: {
      return config.soundEnabled ? L"YES" : L"NO";
    }
    case SettingId::NotificationDurationMs: {
      return std::to_wstring(static_cast<unsigned int>(config.notificationDurationMs)) + L" ms";
    }
    case SettingId::NotificationMaxStack: {
      return std::to_wstring(static_cast<int>(config.notificationMaxStack));
    }
    case SettingId::AnalyticsHistoryEnabled: {
      return config.analyticsHistoryEnabled ? L"YES" : L"NO";
    }
    case SettingId::HistoryCapacity: {
      return std::to_wstring(static_cast<int>(config.historyCapacity));
    }
    case SettingId::LogVisibleLines: {
      return std::to_wstring(static_cast<int>(config.logVisibleLines));
    }
    case SettingId::LogLevel: {
      return L"";
    }
    case SettingId::LogViewMode: {
      return L"";
    }
    case SettingId::LogMilliseconds: {
      return config.logMilliseconds ? L"YES" : L"NO";
    }
    case SettingId::LogJsonEnabled: {
      return config.logJsonEnabled ? L"YES" : L"NO";
    }
    case SettingId::LogPlainEnabled: {
      return config.logPlainEnabled ? L"YES" : L"NO";
    }
    case SettingId::LogFlushIntervalMs: {
      return std::to_wstring(static_cast<unsigned int>(config.logFlushIntervalMs)) + L" ms";
    }
    case SettingId::LogRetentionDays: {
      return std::to_wstring(static_cast<unsigned int>(config.logRetentionDays)) + L" days";
    }
    case SettingId::LogDeduplicate: {
      return config.logDeduplicate ? L"YES" : L"NO";
    }
    case SettingId::PauseLiveLogs: {
      return config.pauseLiveLogs ? L"YES" : L"NO";
    }
    case SettingId::CrtEnabled: {
      return config.crtEnabled ? L"YES" : L"NO";
    }
    case SettingId::CrtCurvatureStrength: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtCurvatureStrength)); return buf; }
    }
    case SettingId::CrtScanlineIntensity: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtScanlineIntensity)); return buf; }
    }
    case SettingId::CrtScanlineSpacing: {
      return std::to_wstring(static_cast<int>(config.crtScanlineSpacing));
    }
    case SettingId::CrtChromaticAberration: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtChromaticAberration)); return buf; }
    }
    case SettingId::CrtPhosphorGlow: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtPhosphorGlow)); return buf; }
    }
    case SettingId::CrtFlickerAmount: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtFlickerAmount)); return buf; }
    }
    case SettingId::CrtNoiseAmount: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtNoiseAmount)); return buf; }
    }
    case SettingId::CrtVignetteStrength: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtVignetteStrength)); return buf; }
    }
    case SettingId::CrtSharpness: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtSharpness)); return buf; }
    }
    case SettingId::CrtGhostOffset: {
      return std::to_wstring(static_cast<int>(config.crtGhostOffset));
    }
    case SettingId::IntroStepPx: {
      return std::to_wstring(static_cast<int>(config.introStepPx)) + L" px";
    }
    case SettingId::IntroHoldMs: {
      return std::to_wstring(static_cast<unsigned int>(config.introHoldMs)) + L" ms";
    }
    case SettingId::IntroCreditDelayMs: {
      return std::to_wstring(static_cast<unsigned int>(config.introCreditDelayMs)) + L" ms";
    }
    case SettingId::BorderEnabled: {
      return config.borderEnabled ? L"YES" : L"NO";
    }
    case SettingId::BorderChromaKeyR: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.borderChromaKeyR)); return buf; }
    }
    case SettingId::BorderChromaKeyG: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.borderChromaKeyG)); return buf; }
    }
    case SettingId::BorderChromaKeyB: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.borderChromaKeyB)); return buf; }
    }
    case SettingId::BorderChromaKeyTolerance: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.borderChromaKeyTolerance)); return buf; }
    }
    case SettingId::CrtRgbShift: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtRgbShift)); return buf; }
    }
    case SettingId::CrtGrain: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtGrain)); return buf; }
    }
    case SettingId::CrtJitter: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtJitter)); return buf; }
    }
    case SettingId::CrtSubpixelMode: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtSubpixelMode)); return buf; }
    }
    case SettingId::CrtBloom: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtBloom)); return buf; }
    }
    case SettingId::CrtBurnInEnabled: {
      return config.crtBurnInEnabled ? L"YES" : L"NO";
    }
    case SettingId::CrtBurnInIntensity: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.crtBurnInIntensity)); return buf; }
    }
    case SettingId::CrtBurnInDecayRate: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.2f", static_cast<double>(config.crtBurnInDecayRate)); return buf; }
    }
    case SettingId::AccentColorPreset: {
      if (static_cast<int>(config.accentColorPreset) >= 0 && static_cast<int>(config.accentColorPreset) < 6)
        return kAccentColorNames[static_cast<int>(config.accentColorPreset)];
      return L"UNKNOWN";
    }
    case SettingId::FontFaceIndex: {
      return L"";
    }
    case SettingId::DefaultTab: {
      if (static_cast<int>(config.defaultTab) >= 0 && static_cast<int>(config.defaultTab) < 6)
        return kDefaultTabNames[static_cast<int>(config.defaultTab)];
      return L"UNKNOWN";
    }
    case SettingId::MinimizeToTray: {
      return config.minimizeToTray ? L"YES" : L"NO";
    }
    case SettingId::StartMaximized: {
      return config.startMaximized ? L"YES" : L"NO";
    }
    case SettingId::SaveWindowPosition: {
      return config.saveWindowPosition ? L"YES" : L"NO";
    }
    case SettingId::HotkeyToggleCrt: {
      if (config.hotkeyToggleCrt == 0) return L"NONE";
      { wchar_t buf[32]; swprintf_s(buf, L"VK 0x%02X", static_cast<unsigned int>(config.hotkeyToggleCrt)); return buf; }
    }
    case SettingId::HotkeyReloadConfig: {
      if (config.hotkeyReloadConfig == 0) return L"NONE";
      { wchar_t buf[32]; swprintf_s(buf, L"VK 0x%02X", static_cast<unsigned int>(config.hotkeyReloadConfig)); return buf; }
    }
    case SettingId::CrtBrightness: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.2f", static_cast<double>(config.crtBrightness)); return buf; }
    }
    case SettingId::CrtContrast: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.2f", static_cast<double>(config.crtContrast)); return buf; }
    }
    case SettingId::CrtSaturation: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.2f", static_cast<double>(config.crtSaturation)); return buf; }
    }
    case SettingId::CrtGamma: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.2f", static_cast<double>(config.crtGamma)); return buf; }
    }
    case SettingId::CrtScanlineMode: {
      if (static_cast<int>(config.crtScanlineMode) >= 0 && static_cast<int>(config.crtScanlineMode) < 3)
        return kCrtScanlineModeNames[static_cast<int>(config.crtScanlineMode)];
      return L"UNKNOWN";
    }
    case SettingId::CrtInterlace: {
      return config.crtInterlace ? L"YES" : L"NO";
    }
    case SettingId::BorderImagePreset: {
      if (static_cast<int>(config.borderImagePreset) >= 0 && static_cast<int>(config.borderImagePreset) < 5)
        return kBorderImagePresetNames[static_cast<int>(config.borderImagePreset)];
      return L"UNKNOWN";
    }
    case SettingId::Language: {
      if (static_cast<int>(config.language) >= 0 && static_cast<int>(config.language) < 3)
        return kLanguageNames[static_cast<int>(config.language)];
      return L"UNKNOWN";
    }
    case SettingId::ShowFps: {
      return config.showFps ? L"YES" : L"NO";
    }
    case SettingId::WindowOpacity: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", static_cast<double>(config.windowOpacity)); return buf; }
    }
    case SettingId::AlwaysOnTop: {
      return config.alwaysOnTop ? L"YES" : L"NO";
    }
    case SettingId::StartWithWindows: {
      return config.startWithWindows ? L"YES" : L"NO";
    }
    case SettingId::ThemeMode: {
      if (static_cast<int>(config.themeMode) >= 0 && static_cast<int>(config.themeMode) < 3)
        return kThemeModeNames[static_cast<int>(config.themeMode)];
      return L"UNKNOWN";
    }
    case SettingId::FrameTargetFps: {
      return L"";
    }
    case SettingId::VSyncEnabled: {
      return config.vSyncEnabled ? L"YES" : L"NO";
    }
    case SettingId::ProcessPriority: {
      if (static_cast<int>(config.processPriority) >= 0 && static_cast<int>(config.processPriority) < 3)
        return kProcessPriorityNames[static_cast<int>(config.processPriority)];
      return L"UNKNOWN";
    }
    case SettingId::TransitionEnabled: {
      return config.transitionEnabled ? L"YES" : L"NO";
    }
    case SettingId::TransitionDurationMs: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.0f", config.transitionDurationMs); return buf; }
    }
    case SettingId::TransitionScanlineWidth: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.4f", config.transitionScanlineWidth); return buf; }
    }
    case SettingId::TransitionIntensity: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.3f", config.transitionIntensity); return buf; }
    }
    case SettingId::TransitionDistortion: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.4f", config.transitionDistortion); return buf; }
    }
    case SettingId::TransitionNoise: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.4f", config.transitionNoise); return buf; }
    }
    case SettingId::TransitionFlicker: {
      { wchar_t buf[64]; swprintf_s(buf, L"%.4f", config.transitionFlicker); return buf; }
    }
    default: break;
  }
  return L"";
}

bool RegistrySaveSetting(SettingId id, const Config& config, std::ostream& output) {
  const SettingDef* def = FindSettingDef(id);
  if (!def) return false;
  switch (id) {
    case SettingId::TelemetryIntervalMs: {
      output << "delay_ms=" << static_cast<unsigned int>(config.telemetryIntervalMs) << "\n";
      return true;
    }
    case SettingId::FrameIntervalMs: {
      output << "frame_interval_ms=" << static_cast<unsigned int>(config.frameIntervalMs) << "\n";
      return true;
    }
    case SettingId::FontScale: {
      output << "font_scale=" << config.fontScale << "\n";
      return true;
    }
    case SettingId::IntroEnabled: {
      output << "intro_enabled=" << (config.introEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::NotificationsEnabled: {
      output << "notifications_enabled=" << (config.notificationsEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::SoundEnabled: {
      output << "sound_enabled=" << (config.soundEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::NotificationDurationMs: {
      output << "notification_duration_ms=" << static_cast<unsigned int>(config.notificationDurationMs) << "\n";
      return true;
    }
    case SettingId::NotificationMaxStack: {
      output << "notification_max_stack=" << static_cast<int>(config.notificationMaxStack) << "\n";
      return true;
    }
    case SettingId::AnalyticsHistoryEnabled: {
      output << "analytics_history_enabled=" << (config.analyticsHistoryEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::HistoryCapacity: {
      output << "history_capacity=" << static_cast<int>(config.historyCapacity) << "\n";
      return true;
    }
    case SettingId::LogVisibleLines: {
      output << "log_visible_lines=" << static_cast<int>(config.logVisibleLines) << "\n";
      return true;
    }
    case SettingId::LogLevel: {
      return false;
    }
    case SettingId::LogViewMode: {
      return false;
    }
    case SettingId::LogMilliseconds: {
      output << "log_milliseconds=" << (config.logMilliseconds ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::LogJsonEnabled: {
      output << "log_json_enabled=" << (config.logJsonEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::LogPlainEnabled: {
      output << "log_plain_enabled=" << (config.logPlainEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::LogFlushIntervalMs: {
      output << "log_flush_interval_ms=" << static_cast<unsigned int>(config.logFlushIntervalMs) << "\n";
      return true;
    }
    case SettingId::LogRetentionDays: {
      output << "log_retention_days=" << static_cast<unsigned int>(config.logRetentionDays) << "\n";
      return true;
    }
    case SettingId::LogDeduplicate: {
      output << "log_deduplicate=" << (config.logDeduplicate ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::PauseLiveLogs: {
      output << "pause_live_logs=" << (config.pauseLiveLogs ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::CrtEnabled: {
      output << "crt_enabled=" << (config.crtEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::CrtCurvatureStrength: {
      output << "crt_curvature_strength=" << config.crtCurvatureStrength << "\n";
      return true;
    }
    case SettingId::CrtScanlineIntensity: {
      output << "crt_scanline_intensity=" << config.crtScanlineIntensity << "\n";
      return true;
    }
    case SettingId::CrtScanlineSpacing: {
      output << "crt_scanline_spacing=" << static_cast<int>(config.crtScanlineSpacing) << "\n";
      return true;
    }
    case SettingId::CrtChromaticAberration: {
      output << "crt_chromatic_aberration=" << config.crtChromaticAberration << "\n";
      return true;
    }
    case SettingId::CrtPhosphorGlow: {
      output << "crt_phosphor_glow=" << config.crtPhosphorGlow << "\n";
      return true;
    }
    case SettingId::CrtFlickerAmount: {
      output << "crt_flicker_amount=" << config.crtFlickerAmount << "\n";
      return true;
    }
    case SettingId::CrtNoiseAmount: {
      output << "crt_noise_amount=" << config.crtNoiseAmount << "\n";
      return true;
    }
    case SettingId::CrtVignetteStrength: {
      output << "crt_vignette_strength=" << config.crtVignetteStrength << "\n";
      return true;
    }
    case SettingId::CrtSharpness: {
      output << "crt_sharpness=" << config.crtSharpness << "\n";
      return true;
    }
    case SettingId::CrtGhostOffset: {
      output << "crt_ghost_offset=" << static_cast<int>(config.crtGhostOffset) << "\n";
      return true;
    }
    case SettingId::IntroStepPx: {
      output << "intro_step_px=" << static_cast<int>(config.introStepPx) << "\n";
      return true;
    }
    case SettingId::IntroHoldMs: {
      output << "intro_hold_ms=" << static_cast<unsigned int>(config.introHoldMs) << "\n";
      return true;
    }
    case SettingId::IntroCreditDelayMs: {
      output << "intro_credit_delay_ms=" << static_cast<unsigned int>(config.introCreditDelayMs) << "\n";
      return true;
    }
    case SettingId::BorderEnabled: {
      output << "border_enabled=" << (config.borderEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::BorderChromaKeyR: {
      output << "border_key_r=" << config.borderChromaKeyR << "\n";
      return true;
    }
    case SettingId::BorderChromaKeyG: {
      output << "border_key_g=" << config.borderChromaKeyG << "\n";
      return true;
    }
    case SettingId::BorderChromaKeyB: {
      output << "border_key_b=" << config.borderChromaKeyB << "\n";
      return true;
    }
    case SettingId::BorderChromaKeyTolerance: {
      output << "border_key_tolerance=" << config.borderChromaKeyTolerance << "\n";
      return true;
    }
    case SettingId::CrtRgbShift: {
      output << "crt_rgb_shift=" << config.crtRgbShift << "\n";
      return true;
    }
    case SettingId::CrtGrain: {
      output << "crt_grain=" << config.crtGrain << "\n";
      return true;
    }
    case SettingId::CrtJitter: {
      output << "crt_jitter=" << config.crtJitter << "\n";
      return true;
    }
    case SettingId::CrtSubpixelMode: {
      output << "crt_subpixel_mode=" << config.crtSubpixelMode << "\n";
      return true;
    }
    case SettingId::CrtBloom: {
      output << "crt_bloom=" << config.crtBloom << "\n";
      return true;
    }
    case SettingId::CrtBurnInEnabled: {
      output << "crt_burn_in_enabled=" << (config.crtBurnInEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::CrtBurnInIntensity: {
      output << "crt_burn_in_intensity=" << config.crtBurnInIntensity << "\n";
      return true;
    }
    case SettingId::CrtBurnInDecayRate: {
      output << "crt_burn_in_decay_rate=" << config.crtBurnInDecayRate << "\n";
      return true;
    }
    case SettingId::AccentColorPreset: {
      output << "accent_color_preset=" << static_cast<int>(config.accentColorPreset) << "\n";
      return true;
    }
    case SettingId::FontFaceIndex: {
      return false;
    }
    case SettingId::DefaultTab: {
      output << "default_tab=" << static_cast<int>(config.defaultTab) << "\n";
      return true;
    }
    case SettingId::MinimizeToTray: {
      output << "minimize_to_tray=" << (config.minimizeToTray ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::StartMaximized: {
      output << "start_maximized=" << (config.startMaximized ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::SaveWindowPosition: {
      output << "save_window_position=" << (config.saveWindowPosition ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::HotkeyToggleCrt: {
      output << "hotkey_toggle_crt=" << static_cast<int>(config.hotkeyToggleCrt) << "\n";
      return true;
    }
    case SettingId::HotkeyReloadConfig: {
      output << "hotkey_reload_config=" << static_cast<int>(config.hotkeyReloadConfig) << "\n";
      return true;
    }
    case SettingId::CrtBrightness: {
      output << "crt_brightness=" << config.crtBrightness << "\n";
      return true;
    }
    case SettingId::CrtContrast: {
      output << "crt_contrast=" << config.crtContrast << "\n";
      return true;
    }
    case SettingId::CrtSaturation: {
      output << "crt_saturation=" << config.crtSaturation << "\n";
      return true;
    }
    case SettingId::CrtGamma: {
      output << "crt_gamma=" << config.crtGamma << "\n";
      return true;
    }
    case SettingId::CrtScanlineMode: {
      output << "crt_scanline_mode=" << static_cast<int>(config.crtScanlineMode) << "\n";
      return true;
    }
    case SettingId::CrtInterlace: {
      output << "crt_interlace=" << (config.crtInterlace ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::BorderImagePreset: {
      output << "border_image_preset=" << static_cast<int>(config.borderImagePreset) << "\n";
      return true;
    }
    case SettingId::Language: {
      output << "language=" << static_cast<int>(config.language) << "\n";
      return true;
    }
    case SettingId::ShowFps: {
      output << "show_fps=" << (config.showFps ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::WindowOpacity: {
      output << "window_opacity=" << config.windowOpacity << "\n";
      return true;
    }
    case SettingId::AlwaysOnTop: {
      output << "always_on_top=" << (config.alwaysOnTop ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::StartWithWindows: {
      output << "start_with_windows=" << (config.startWithWindows ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::ThemeMode: {
      output << "theme_mode=" << static_cast<int>(config.themeMode) << "\n";
      return true;
    }
    case SettingId::FrameTargetFps: {
      return false;
    }
    case SettingId::VSyncEnabled: {
      output << "vsync_enabled=" << (config.vSyncEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::ProcessPriority: {
      output << "process_priority=" << static_cast<int>(config.processPriority) << "\n";
      return true;
    }
    case SettingId::TransitionEnabled: {
      output << "transition_enabled=" << (config.transitionEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::TransitionDurationMs: {
      output << "transition_duration_ms=" << config.transitionDurationMs << "\n";
      return true;
    }
    case SettingId::TransitionScanlineWidth: {
      output << "transition_scanline_width=" << config.transitionScanlineWidth << "\n";
      return true;
    }
    case SettingId::TransitionIntensity: {
      output << "transition_intensity=" << config.transitionIntensity << "\n";
      return true;
    }
    case SettingId::TransitionDistortion: {
      output << "transition_distortion=" << config.transitionDistortion << "\n";
      return true;
    }
    case SettingId::TransitionNoise: {
      output << "transition_noise=" << config.transitionNoise << "\n";
      return true;
    }
    case SettingId::TransitionFlicker: {
      output << "transition_flicker=" << config.transitionFlicker << "\n";
      return true;
    }
    default: break;
  }
  return false;
}

bool RegistryLoadSetting(const std::string& key, const std::string& value, Config& config) {
  const auto& reg = GetSettingsRegistry();
  for (const auto& def : reg) {
    if (def.configKey && key == def.configKey) {
      switch (def.id) {
        case SettingId::TelemetryIntervalMs: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.telemetryIntervalMs = v; return true; }
          return false;
        }
        case SettingId::FrameIntervalMs: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.frameIntervalMs = v; return true; }
          return false;
        }
        case SettingId::FontScale: {
          double d;
          if (TryParseDouble(value, d)) { config.fontScale = d; return true; }
          return false;
        }
        case SettingId::IntroEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.introEnabled = b; return true; }
          return false;
        }
        case SettingId::NotificationsEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.notificationsEnabled = b; return true; }
          return false;
        }
        case SettingId::SoundEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.soundEnabled = b; return true; }
          return false;
        }
        case SettingId::NotificationDurationMs: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.notificationDurationMs = v; return true; }
          return false;
        }
        case SettingId::NotificationMaxStack: {
          int v;
          if (TryParseInt(value, v)) { config.notificationMaxStack = v; return true; }
          return false;
        }
        case SettingId::AnalyticsHistoryEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.analyticsHistoryEnabled = b; return true; }
          return false;
        }
        case SettingId::HistoryCapacity: {
          int v;
          if (TryParseInt(value, v)) { config.historyCapacity = v; return true; }
          return false;
        }
        case SettingId::LogVisibleLines: {
          int v;
          if (TryParseInt(value, v)) { config.logVisibleLines = v; return true; }
          return false;
        }
        case SettingId::LogLevel: {
          return false;
        }
        case SettingId::LogViewMode: {
          return false;
        }
        case SettingId::LogMilliseconds: {
          bool b;
          if (TryParseBool(value, b)) { config.logMilliseconds = b; return true; }
          return false;
        }
        case SettingId::LogJsonEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.logJsonEnabled = b; return true; }
          return false;
        }
        case SettingId::LogPlainEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.logPlainEnabled = b; return true; }
          return false;
        }
        case SettingId::LogFlushIntervalMs: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.logFlushIntervalMs = v; return true; }
          return false;
        }
        case SettingId::LogRetentionDays: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.logRetentionDays = v; return true; }
          return false;
        }
        case SettingId::LogDeduplicate: {
          bool b;
          if (TryParseBool(value, b)) { config.logDeduplicate = b; return true; }
          return false;
        }
        case SettingId::PauseLiveLogs: {
          bool b;
          if (TryParseBool(value, b)) { config.pauseLiveLogs = b; return true; }
          return false;
        }
        case SettingId::CrtEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.crtEnabled = b; return true; }
          return false;
        }
        case SettingId::CrtCurvatureStrength: {
          double d;
          if (TryParseDouble(value, d)) { config.crtCurvatureStrength = d; return true; }
          return false;
        }
        case SettingId::CrtScanlineIntensity: {
          double d;
          if (TryParseDouble(value, d)) { config.crtScanlineIntensity = d; return true; }
          return false;
        }
        case SettingId::CrtScanlineSpacing: {
          int v;
          if (TryParseInt(value, v)) { config.crtScanlineSpacing = v; return true; }
          return false;
        }
        case SettingId::CrtChromaticAberration: {
          double d;
          if (TryParseDouble(value, d)) { config.crtChromaticAberration = d; return true; }
          return false;
        }
        case SettingId::CrtPhosphorGlow: {
          double d;
          if (TryParseDouble(value, d)) { config.crtPhosphorGlow = d; return true; }
          return false;
        }
        case SettingId::CrtFlickerAmount: {
          double d;
          if (TryParseDouble(value, d)) { config.crtFlickerAmount = d; return true; }
          return false;
        }
        case SettingId::CrtNoiseAmount: {
          double d;
          if (TryParseDouble(value, d)) { config.crtNoiseAmount = d; return true; }
          return false;
        }
        case SettingId::CrtVignetteStrength: {
          double d;
          if (TryParseDouble(value, d)) { config.crtVignetteStrength = d; return true; }
          return false;
        }
        case SettingId::CrtSharpness: {
          double d;
          if (TryParseDouble(value, d)) { config.crtSharpness = d; return true; }
          return false;
        }
        case SettingId::CrtGhostOffset: {
          int v;
          if (TryParseInt(value, v)) { config.crtGhostOffset = v; return true; }
          return false;
        }
        case SettingId::IntroStepPx: {
          int v;
          if (TryParseInt(value, v)) { config.introStepPx = v; return true; }
          return false;
        }
        case SettingId::IntroHoldMs: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.introHoldMs = v; return true; }
          return false;
        }
        case SettingId::IntroCreditDelayMs: {
          unsigned int v;
          if (TryParseUInt(value, v)) { config.introCreditDelayMs = v; return true; }
          return false;
        }
        case SettingId::BorderEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.borderEnabled = b; return true; }
          return false;
        }
        case SettingId::BorderChromaKeyR: {
          double d;
          if (TryParseDouble(value, d)) { config.borderChromaKeyR = d; return true; }
          return false;
        }
        case SettingId::BorderChromaKeyG: {
          double d;
          if (TryParseDouble(value, d)) { config.borderChromaKeyG = d; return true; }
          return false;
        }
        case SettingId::BorderChromaKeyB: {
          double d;
          if (TryParseDouble(value, d)) { config.borderChromaKeyB = d; return true; }
          return false;
        }
        case SettingId::BorderChromaKeyTolerance: {
          double d;
          if (TryParseDouble(value, d)) { config.borderChromaKeyTolerance = d; return true; }
          return false;
        }
        case SettingId::CrtRgbShift: {
          double d;
          if (TryParseDouble(value, d)) { config.crtRgbShift = d; return true; }
          return false;
        }
        case SettingId::CrtGrain: {
          double d;
          if (TryParseDouble(value, d)) { config.crtGrain = d; return true; }
          return false;
        }
        case SettingId::CrtJitter: {
          double d;
          if (TryParseDouble(value, d)) { config.crtJitter = d; return true; }
          return false;
        }
        case SettingId::CrtSubpixelMode: {
          double d;
          if (TryParseDouble(value, d)) { config.crtSubpixelMode = d; return true; }
          return false;
        }
        case SettingId::CrtBloom: {
          double d;
          if (TryParseDouble(value, d)) { config.crtBloom = d; return true; }
          return false;
        }
        case SettingId::CrtBurnInEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.crtBurnInEnabled = b; return true; }
          return false;
        }
        case SettingId::CrtBurnInIntensity: {
          double d;
          if (TryParseDouble(value, d)) { config.crtBurnInIntensity = d; return true; }
          return false;
        }
        case SettingId::CrtBurnInDecayRate: {
          double d;
          if (TryParseDouble(value, d)) { config.crtBurnInDecayRate = d; return true; }
          return false;
        }
        case SettingId::AccentColorPreset: {
          int v;
          if (TryParseInt(value, v)) { config.accentColorPreset = std::clamp(v, 0, 5); return true; }
          return false;
        }
        case SettingId::FontFaceIndex: {
          return false;
        }
        case SettingId::DefaultTab: {
          int v;
          if (TryParseInt(value, v)) { config.defaultTab = std::clamp(v, 0, 5); return true; }
          return false;
        }
        case SettingId::MinimizeToTray: {
          bool b;
          if (TryParseBool(value, b)) { config.minimizeToTray = b; return true; }
          return false;
        }
        case SettingId::StartMaximized: {
          bool b;
          if (TryParseBool(value, b)) { config.startMaximized = b; return true; }
          return false;
        }
        case SettingId::SaveWindowPosition: {
          bool b;
          if (TryParseBool(value, b)) { config.saveWindowPosition = b; return true; }
          return false;
        }
        case SettingId::HotkeyToggleCrt: {
          int v;
          if (TryParseInt(value, v)) { config.hotkeyToggleCrt = v; return true; }
          return false;
        }
        case SettingId::HotkeyReloadConfig: {
          int v;
          if (TryParseInt(value, v)) { config.hotkeyReloadConfig = v; return true; }
          return false;
        }
        case SettingId::CrtBrightness: {
          double d;
          if (TryParseDouble(value, d)) { config.crtBrightness = d; return true; }
          return false;
        }
        case SettingId::CrtContrast: {
          double d;
          if (TryParseDouble(value, d)) { config.crtContrast = d; return true; }
          return false;
        }
        case SettingId::CrtSaturation: {
          double d;
          if (TryParseDouble(value, d)) { config.crtSaturation = d; return true; }
          return false;
        }
        case SettingId::CrtGamma: {
          double d;
          if (TryParseDouble(value, d)) { config.crtGamma = d; return true; }
          return false;
        }
        case SettingId::CrtScanlineMode: {
          int v;
          if (TryParseInt(value, v)) { config.crtScanlineMode = v; return true; }
          return false;
        }
        case SettingId::CrtInterlace: {
          bool b;
          if (TryParseBool(value, b)) { config.crtInterlace = b; return true; }
          return false;
        }
        case SettingId::BorderImagePreset: {
          int v;
          if (TryParseInt(value, v)) { config.borderImagePreset = v; return true; }
          return false;
        }
        case SettingId::Language: {
          int v;
          if (TryParseInt(value, v)) { config.language = v; return true; }
          return false;
        }
        case SettingId::ShowFps: {
          bool b;
          if (TryParseBool(value, b)) { config.showFps = b; return true; }
          return false;
        }
        case SettingId::WindowOpacity: {
          double d;
          if (TryParseDouble(value, d)) { config.windowOpacity = d; return true; }
          return false;
        }
        case SettingId::AlwaysOnTop: {
          bool b;
          if (TryParseBool(value, b)) { config.alwaysOnTop = b; return true; }
          return false;
        }
        case SettingId::StartWithWindows: {
          bool b;
          if (TryParseBool(value, b)) { config.startWithWindows = b; return true; }
          return false;
        }
        case SettingId::ThemeMode: {
          int v;
          if (TryParseInt(value, v)) { config.themeMode = v; return true; }
          return false;
        }
        case SettingId::FrameTargetFps: {
          return false;
        }
        case SettingId::VSyncEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.vSyncEnabled = b; return true; }
          return false;
        }
        case SettingId::ProcessPriority: {
          int v;
          if (TryParseInt(value, v)) { config.processPriority = v; return true; }
          return false;
        }
        case SettingId::TransitionEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.transitionEnabled = b; return true; }
          return false;
        }
        case SettingId::TransitionDurationMs: {
          double d;
          if (TryParseDouble(value, d)) { config.transitionDurationMs = d; return true; }
          return false;
        }
        case SettingId::TransitionScanlineWidth: {
          double d;
          if (TryParseDouble(value, d)) { config.transitionScanlineWidth = d; return true; }
          return false;
        }
        case SettingId::TransitionIntensity: {
          double d;
          if (TryParseDouble(value, d)) { config.transitionIntensity = d; return true; }
          return false;
        }
        case SettingId::TransitionDistortion: {
          double d;
          if (TryParseDouble(value, d)) { config.transitionDistortion = d; return true; }
          return false;
        }
        case SettingId::TransitionNoise: {
          double d;
          if (TryParseDouble(value, d)) { config.transitionNoise = d; return true; }
          return false;
        }
        case SettingId::TransitionFlicker: {
          double d;
          if (TryParseDouble(value, d)) { config.transitionFlicker = d; return true; }
          return false;
        }
        default: break;
      }
      return false;
    }
  }
  return false;
}

}  // namespace monix

