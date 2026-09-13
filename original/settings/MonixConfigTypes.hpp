#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

namespace monix {

enum class LogLevel {
  Debug,
  Info,
  Warn,
  Error,
  Critical
};

enum class LogViewMode {
  Timeline,
  Structured,
  Compact
};

enum class SettingId {
  TelemetryIntervalMs,
  FrameIntervalMs,
  FontScale,
  IntroEnabled,
  NotificationsEnabled,
  SoundEnabled,
  NotificationDurationMs,
  NotificationMaxStack,
  AnalyticsHistoryEnabled,
  HistoryCapacity,
  LogVisibleLines,
  LogLevel,
  LogViewMode,
  LogMilliseconds,
  LogJsonEnabled,
  LogPlainEnabled,
  LogFlushIntervalMs,
  LogRetentionDays,
  LogDeduplicate,
  PauseLiveLogs,
  CrtEnabled,
  CrtCurvatureStrength,
  CrtScanlineIntensity,
  CrtScanlineSpacing,
  CrtChromaticAberration,
  CrtPhosphorGlow,
  CrtFlickerAmount,
  CrtNoiseAmount,
  CrtVignetteStrength,
  CrtSharpness,
  CrtGhostOffset,
  IntroStepPx,
  IntroHoldMs,
  IntroCreditDelayMs,
  BorderEnabled,
  BorderChromaKeyR,
  BorderChromaKeyG,
  BorderChromaKeyB,
  BorderChromaKeyTolerance,
  CrtRgbShift,
  CrtGrain,
  CrtJitter,
  CrtSubpixelMode,
  CrtBloom,
  CrtBurnInEnabled,
  CrtBurnInIntensity,
  CrtBurnInDecayRate,
  AccentColorPreset,
  FontFaceIndex,
  DefaultTab,
  MinimizeToTray,
  StartMaximized,
  SaveWindowPosition,
  HotkeyToggleCrt,
  HotkeyReloadConfig,
  CrtBrightness,
  CrtContrast,
  CrtSaturation,
  CrtGamma,
  CrtScanlineMode,
  CrtInterlace,
  BorderImagePreset,
  Language,
  ShowFps,
  WindowOpacity,
  AlwaysOnTop,
  StartWithWindows,
  ThemeMode,
  FrameTargetFps,
  VSyncEnabled,
  ProcessPriority,
  TransitionEnabled,
  TransitionDurationMs,
  TransitionScanlineWidth,
  TransitionIntensity,
  TransitionDistortion,
  TransitionNoise,
  TransitionFlicker
};

struct SettingMutation {
  bool changed = false;
  bool recreateFonts = false;
  bool resetFrameTimer = false;
  bool requestRefresh = false;
  bool trimHistory = false;
  bool clearNotifications = false;
  SettingId id = SettingId::TelemetryIntervalMs;
  std::wstring label;
};

struct SettingActionRect {
  SettingId id;
  RECT row { 0, 0, 0, 0 };
  RECT decrease { 0, 0, 0, 0 };
  RECT increase { 0, 0, 0, 0 };
  RECT value { 0, 0, 0, 0 };
};

struct Config {
  UINT frameIntervalMs = 33;
  UINT telemetryIntervalMs = 75;
  int logBufferSize = 480;
  int logVisibleLines = 26;
  double fontScale = 1.12;
  bool introEnabled = true;
  int introStepPx = 18;
  UINT introHoldMs = 1050;
  UINT introCreditDelayMs = 260;
  bool crtEnabled = true;
  int crtScanlineSpacing = 3;
  int crtGhostOffset = 1;
  double crtCurvatureStrength = 0.02;
  double crtScanlineIntensity = 0.30;
  double crtChromaticAberration = 0.03;
  double crtPhosphorGlow = 0.12;
  double crtFlickerAmount = 0.012;
  double crtNoiseAmount = 0.018;
  double crtVignetteStrength = 0.22;
  double crtSharpness = 0.55;
  double crtRgbShift = 0.0;
  double crtGrain = 0.0;
  double crtJitter = 0.0;
  double crtSubpixelMode = 0.0;
  double crtBloom = 0.0;
  bool crtBurnInEnabled = false;
  double crtBurnInIntensity = 0.0;
  double crtBurnInDecayRate = 1.0;
  bool borderEnabled = true;
  std::wstring borderImage = L"borders\\CRT2.png";
  double borderChromaKeyR = 0.0;
  double borderChromaKeyG = 1.0;
  double borderChromaKeyB = 0.0;
  double borderChromaKeyTolerance = 0.5;
  bool notificationsEnabled = true;
  bool soundEnabled = true;
  UINT notificationDurationMs = 4200;
  int notificationMaxStack = 4;
  LogLevel logLevel = LogLevel::Debug;
  LogViewMode logViewMode = LogViewMode::Structured;
  bool logMilliseconds = true;
  bool logJsonEnabled = true;
  bool logPlainEnabled = true;
  UINT logFlushIntervalMs = 1200;
  std::uint64_t logMaxFileBytes = 1024ull * 1024ull;
  UINT logRetentionDays = 7;
  bool logDeduplicate = true;
  bool pauseLiveLogs = false;
  bool analyticsHistoryEnabled = true;
  int historyCapacity = 120;
  std::wstring userId = L"local";
  int accentColorPreset = 0;
  int fontFaceIndex = 0;
  int defaultTab = 0;
  bool minimizeToTray = false;
  bool startMaximized = false;
  bool saveWindowPosition = true;
  int hotkeyToggleCrt = 0;
  int hotkeyReloadConfig = 0;
  double crtBrightness = 1.0;
  double crtContrast = 1.0;
  double crtSaturation = 1.0;
  double crtGamma = 1.0;
  int crtScanlineMode = 0;
  int crtInterlace = 0;
  int borderImagePreset = 0;
  int language = 0;
  bool showFps = false;
  double windowOpacity = 1.0;
  bool alwaysOnTop = false;
  bool startWithWindows = false;
  int themeMode = 0;
  int frameTargetFps = 0;
  bool vSyncEnabled = true;
  int processPriority = 0;
  bool transitionEnabled = true;
  double transitionDurationMs = 220.0;
  double transitionScanlineWidth = 0.012;
  double transitionIntensity = 0.8;
  double transitionDistortion = 0.002;
  double transitionNoise = 0.015;
  double transitionFlicker = 0.025;
};

}  // namespace monix
