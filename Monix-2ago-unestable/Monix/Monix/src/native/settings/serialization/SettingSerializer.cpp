#include "SettingSerializer.hpp"
#include "../registry/SettingDefs.hpp"

#include <algorithm>
#include <sstream>

namespace monix {

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
      switch (config.logLevel) {
        case LogLevel::Debug:    return L"DEBUG";
        case LogLevel::Info:     return L"INFO";
        case LogLevel::Warn:     return L"WARN";
        case LogLevel::Error:    return L"ERROR";
        case LogLevel::Critical: return L"CRITICAL";
      }
      return L"DEBUG";
    }
    case SettingId::LogViewMode: {
      switch (config.logViewMode) {
        case LogViewMode::Timeline:    return L"Timeline";
        case LogViewMode::Structured:  return L"Structured";
        case LogViewMode::Compact:     return L"Compact";
      }
      return L"Structured";
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
      const wchar_t* const* names = GetAccentColorNames();
      if (static_cast<int>(config.accentColorPreset) >= 0 && static_cast<int>(config.accentColorPreset) < 6)
        return names[static_cast<int>(config.accentColorPreset)];
      return L"UNKNOWN";
    }
    case SettingId::FontFaceIndex: {
      return std::to_wstring(config.fontFaceIndex);
    }
    case SettingId::DefaultTab: {
      const wchar_t* const* names = GetDefaultTabNames();
      if (static_cast<int>(config.defaultTab) >= 0 && static_cast<int>(config.defaultTab) < 6)
        return names[static_cast<int>(config.defaultTab)];
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
      const wchar_t* const* names = GetCrtScanlineModeNames();
      if (static_cast<int>(config.crtScanlineMode) >= 0 && static_cast<int>(config.crtScanlineMode) < 3)
        return names[static_cast<int>(config.crtScanlineMode)];
      return L"UNKNOWN";
    }
    case SettingId::CrtInterlace: {
      return config.crtInterlace ? L"YES" : L"NO";
    }
    case SettingId::BorderImagePreset: {
      const wchar_t* const* names = GetBorderImagePresetNames();
      if (static_cast<int>(config.borderImagePreset) >= 0 && static_cast<int>(config.borderImagePreset) < 5)
        return names[static_cast<int>(config.borderImagePreset)];
      return L"UNKNOWN";
    }
    case SettingId::Language: {
      const wchar_t* const* names = GetLanguageNames();
      if (static_cast<int>(config.language) >= 0 && static_cast<int>(config.language) < 3)
        return names[static_cast<int>(config.language)];
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
      const wchar_t* const* names = GetThemeModeNames();
      if (static_cast<int>(config.themeMode) >= 0 && static_cast<int>(config.themeMode) < 6)
        return names[static_cast<int>(config.themeMode)];
      return L"UNKNOWN";
    }
    case SettingId::FrameTargetFps: {
      if (config.frameTargetFps == 0) return L"VSYNC";
      return std::to_wstring(config.frameTargetFps) + L" FPS";
    }
    case SettingId::VSyncEnabled: {
      return config.vSyncEnabled ? L"YES" : L"NO";
    }
    case SettingId::ProcessPriority: {
      const wchar_t* const* names = GetProcessPriorityNames();
      if (static_cast<int>(config.processPriority) >= 0 && static_cast<int>(config.processPriority) < 3)
        return names[static_cast<int>(config.processPriority)];
      return L"UNKNOWN";
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
      output << "log_level=" << static_cast<int>(config.logLevel) << "\n";
      return true;
    }
    case SettingId::LogViewMode: {
      output << "log_view=" << static_cast<int>(config.logViewMode) << "\n";
      return true;
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
      output << "font_face_index=" << config.fontFaceIndex << "\n";
      return true;
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
      output << "frame_target_fps=" << config.frameTargetFps << "\n";
      return true;
    }
    case SettingId::VSyncEnabled: {
      output << "vsync_enabled=" << (config.vSyncEnabled ? "true" : "false") << "\n";
      return true;
    }
    case SettingId::ProcessPriority: {
      output << "process_priority=" << static_cast<int>(config.processPriority) << "\n";
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
          int v;
          if (TryParseInt(value, v)) { config.logLevel = static_cast<LogLevel>(std::clamp(v, 0, 4)); return true; }
          return false;
        }
        case SettingId::LogViewMode: {
          int v;
          if (TryParseInt(value, v)) { config.logViewMode = static_cast<LogViewMode>(std::clamp(v, 0, 2)); return true; }
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
          int v;
          if (TryParseInt(value, v)) { config.fontFaceIndex = (std::max)(0, v); return true; }
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
          if (TryParseInt(value, v)) { config.crtScanlineMode = std::clamp(v, 0, 2); return true; }
          return false;
        }
        case SettingId::CrtInterlace: {
          bool b;
          if (TryParseBool(value, b)) { config.crtInterlace = b; return true; }
          return false;
        }
        case SettingId::BorderImagePreset: {
          int v;
          if (TryParseInt(value, v)) { config.borderImagePreset = std::clamp(v, 0, 4); return true; }
          return false;
        }
        case SettingId::Language: {
          int v;
          if (TryParseInt(value, v)) { config.language = std::clamp(v, 0, 2); return true; }
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
          if (TryParseInt(value, v)) { config.themeMode = std::clamp(v, 0, 5); return true; }
          return false;
        }
        case SettingId::FrameTargetFps: {
          int v;
          if (TryParseInt(value, v)) { config.frameTargetFps = std::clamp(v, 0, 240); return true; }
          return false;
        }
        case SettingId::VSyncEnabled: {
          bool b;
          if (TryParseBool(value, b)) { config.vSyncEnabled = b; return true; }
          return false;
        }
        case SettingId::ProcessPriority: {
          int v;
          if (TryParseInt(value, v)) { config.processPriority = std::clamp(v, 0, 2); return true; }
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
