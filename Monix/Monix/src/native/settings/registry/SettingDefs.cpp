#include "SettingDefs.hpp"
#include "../../core/TextUtils.hpp"

#include <algorithm>
#include <cstring>

namespace monix {

namespace {

const wchar_t* kAccentColorNames[] = { L"GREEN", L"BLUE", L"RED", L"PURPLE", L"AMBER", L"CYAN" };
const wchar_t* kDefaultTabNames[] = { L"LOG", L"TASKS", L"HARDWARE", L"NETWORK", L"SCRAM", L"SETTINGS" };
const wchar_t* kCrtScanlineModeNames[] = { L"FLAT", L"CURVED", L"DIAMOND" };
const wchar_t* kBorderImagePresetNames[] = { L"CRT2", L"CRT3", L"WOOD", L"METAL", L"NONE" };
const wchar_t* kLanguageNames[] = { L"ENGLISH", L"ESPANOL", L"FRANCAIS" };
const wchar_t* kThemeModeNames[] = { L"DARK", L"LIGHT", L"SYSTEM", L"CORE MONITOR", L"WINDOWS 98", L"MODERN" };
const wchar_t* kProcessPriorityNames[] = { L"NORMAL", L"HIGH", L"REALTIME" };

}  // namespace

const wchar_t* const* GetAccentColorNames() { return kAccentColorNames; }
const wchar_t* const* GetDefaultTabNames() { return kDefaultTabNames; }
const wchar_t* const* GetCrtScanlineModeNames() { return kCrtScanlineModeNames; }
const wchar_t* const* GetBorderImagePresetNames() { return kBorderImagePresetNames; }
const wchar_t* const* GetLanguageNames() { return kLanguageNames; }
const wchar_t* const* GetThemeModeNames() { return kThemeModeNames; }
const wchar_t* const* GetProcessPriorityNames() { return kProcessPriorityNames; }

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
        0.0, 5.0, 1.0, 0, nullptr, 6, kThemeModeNames
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

}  // namespace monix
