#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "vulkan_renderer.h"
#include "ui/shaders/ShaderBrowserPanel.hpp"
#include "renderer_vk/RenderState.hpp"
#include "ui/AppStateData.hpp"
#include "telemetry/TelemetryEvents.hpp"

#include "../../login/AuthManager.hpp"
#include "../../login/MonixKernel.hpp"
#include "../../login/KernelDisplay.hpp"
#include "../../sensors/sensors.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "config/MonixConfig.hpp"
#include "ui/Win98Types.hpp"
#include "core/TextUtils.hpp"
#include "core/StringUtils.hpp"
#include "logging/LogEntry.hpp"
#include "logging/LogManager.hpp"
#include "logging/SoundPlayer.hpp"
#include "logging/NotificationQueue.hpp"
#include "scram/ScramEngine.hpp"
#include "settings/MonixConfigTypes.hpp"
#include "settings/SettingsRegistry.hpp"
#include "settings/SettingGroups.hpp"
#include "telemetry/Snapshot.hpp"
#include "telemetry/Collectors.hpp"
#include "telemetry/TelemetryBaselines.hpp"
#include "app/bootstrap/AppConstants.hpp"

using monix::Tab;
using monix::ColorRole;
using monix::LogToolbarRect;
using monix::SettingId;
using monix::SettingMutation;
using monix::SettingActionRect;
using monix::NativeProcessSample;
using monix::LogLevel;
using monix::ProcessInfo;
using monix::LogEntry;
using monix::Snapshot;
using monix::Config;
using monix::AppPaths;

using monix::ResolveColor;
using monix::ShrinkRect;
using monix::FormatPercent;
using monix::FormatBytes;
using monix::FormatRate;
using monix::FormatTemperature;
using monix::FormatDuration;
using monix::BuildBar;
using monix::LogViewModeText;
using monix::Utf8ToWide;
using monix::LogFilter;
using monix::LogViewMode;
using monix::NetworkFlow;
using monix::RuntimeSettings;
using monix::IntroAnimSettings;
using monix::GeneralExtendedSettings;
using monix::BorderSettings;
using monix::LoggingSettings;
using monix::ThemeSettings;
using monix::WindowStartupSettings;
using monix::SystemExtendedSettings;
using monix::HotkeySettings;
using monix::PerformanceSettings;

inline void FillSolid(HDC dc, const RECT& rect, COLORREF color) {
  HBRUSH br = CreateSolidBrush(color);
  FillRect(dc, &rect, br);
  DeleteObject(br);
}

inline void DrawRectOutline(HDC dc, const RECT& rect, COLORREF color) {
  HPEN pen = CreatePen(PS_SOLID, 1, color);
  HBRUSH oldBr = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
  HPEN oldPen = (HPEN)SelectObject(dc, pen);
  Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
  SelectObject(dc, oldPen);
  SelectObject(dc, oldBr);
  DeleteObject(pen);
}

class MonixApp {
 public:
  MonixApp();
  ~MonixApp();

  int Run(HINSTANCE instance, int showCommand);
  void SetTestMode() { testMode_ = true; }
  void SetParityMode(bool capture, bool compare, const std::wstring& refDir) {
    parityCapture_ = capture;
    parityCompare_ = compare;
    parityRefDir_ = refDir;
  }
  void SetRuntimeStateMode(bool enabled, const std::wstring& retroarchSnap) {
    runtimeStateEnabled_ = enabled;
    retroarchSnapshotPath_ = retroarchSnap;
  }
  void SetRegressionTestMode(bool enabled, const std::wstring& presetsDir, int frames) {
    regressionTestMode_ = enabled;
    regressionPresetsDir_ = presetsDir;
    regressionFrames_ = frames;
  }

 private:
  static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

  bool CreateMainWindow(HINSTANCE instance, int showCommand);
  bool InitializeOpenGlBootstrap();
  void ShutdownOpenGlBootstrap();
  void LoadConfig(bool logEvent);
  void SaveConfig() const;
  void WriteDefaultConfigIfMissing() const;
  void LoadRuntimeAssets();
  void SwitchFont(int direction);
  void LoadRecentLogHistory();
  void CreateUiFonts();
  void DestroyUiFonts();
  void MeasureFontMetrics();
  bool EnsureOpenGlUiSurface(int width, int height);
  void DestroyOpenGlUiSurface();
  void DestroyOpenGlResources();
  bool RenderOpenGlFrame(const RECT& clientRect);
  void SetFrameTimer();
  void FlushLogQueues(bool force);
  void CleanupLogFiles();
  void CleanupStaleLogFiles();
  void QueueNotification(const LogEntry& entry);
  void PlayAlertSound(LogLevel level);
  void PlayLogSound();
  void PlayClickSound();
  void AppendHistoryPoint(const Snapshot& snapshot);
  void ExportLogsJson() const;
  void ExportLogsCsv() const;
  std::filesystem::path ResolveLogFilePath(const std::wstring& extension) const;
  std::filesystem::path ResolveLogFilePathLocked(const std::wstring& extension, ULONGLONG maxFileBytes) const;

  void StartTelemetry();
  void StopTelemetry();
public:
  void TelemetryLoop();
  const Config& GetConfig() const {
    std::shared_lock<std::shared_mutex> lock(configMutex_);
    return config_;
  }
private:
  Snapshot PollSnapshot();
  Snapshot PollNativeSnapshot();
  std::vector<ProcessInfo> BuildFallbackProcesses(const Snapshot& snapshot);
  std::string RunProcessCapture(const std::wstring& commandLine) const;
  void ConsumeSnapshot(Snapshot snapshot);
  void PushLog(
    std::wstring domain,
    std::wstring severity,
    std::wstring message,
    ColorRole color,
    std::wstring module = L"core",
    std::wstring service = L"monix",
    std::wstring metadata = L"",
    monix::EventType eventType = monix::EventType::TelemetrySample
  );
  void SeedReferenceState();
  void RequestRefresh();
  void SetToast(const std::wstring& message);
  void TrimHistoryBuffers();
  SettingMutation AdjustSetting(SettingId id, int direction);
  void CommitSettingMutation(const SettingMutation& mutation);
  std::wstring SettingLabel(SettingId id) const;
  std::wstring SettingValueText(SettingId id) const;
  std::vector<SettingActionRect> BuildSettingActionRects(const RECT& clientRect) const;
  bool HandleSettingsClick(const RECT& clientRect, POINT point);
  bool HandleSettingsKey(WPARAM key);

  bool HitTestTabs(const RECT& clientRect, POINT point, Tab& tab) const;
  int HitTestTaskRow(const RECT& clientRect, POINT point) const;
  void OpenTaskMenu(const RECT& clientRect, POINT point, int processIndex);
  bool HandleTaskMenuClick(POINT point);
  void ExecuteTaskMenuAction(int itemIndex);
  void SpawnDemoLogs();
  void CopyToClipboard(const std::wstring& text) const;
  bool ApplyPriorityToProcess(int pid, DWORD priorityClass);
  bool TerminateProcessById(int pid);
  void TickAnimations(const RECT& clientRect);
  void ComputeViewport(const RECT& client);
  POINT MapToViewport(POINT pt) const;
  void ComputeIntroLogoMetrics(const RECT& clientRect, int& cellW, int& cellH, int& logoWidth, int& logoHeight, int& maxColumns) const;

  void Render(HDC dc, const RECT& clientRect);
  void TransitionToLoggedIn();
  void RenderCoreMonitorTheme(HDC dc, const RECT& clientRect);
  bool IsCoreMonitorThemeActive() const;
  void DrawBackground(HDC dc, const RECT& clientRect);
  void DrawSparkline(HDC dc, const RECT& rect, const std::vector<double>& values, double maxValue, ColorRole color) const;
  void DrawTabs(HDC dc, const RECT& clientRect);
  void DrawFooter(HDC dc, const RECT& clientRect);
  void DrawToast(HDC dc, const RECT& clientRect);
  void DrawClickDebug(HDC dc, const RECT& clientRect);
  void DrawNotifications(HDC dc, const RECT& clientRect);
  void DrawDevShell(HDC dc, const RECT& clientRect);
  void DrawIntro(HDC dc, const RECT& clientRect);
  void DrawPanel(HDC dc, const RECT& rect, const std::wstring& title, ColorRole accent, HFONT titleFont) const;
  void DrawTextRect(HDC dc, const RECT& rect, const std::wstring& text, ColorRole color, HFONT font, UINT format) const;
  void DrawTextLine(HDC dc, int x, int y, int width, const std::wstring& text, ColorRole color, HFONT font, UINT format = 0) const;
  void DrawProgressBar(HDC dc, const RECT& rect, double pct, ColorRole accent) const;
  void DrawLogView(HDC dc, const RECT& clientRect);
  void DrawLogToolbar(HDC dc, const RECT& clientRect);
  void DrawLogFilterBadges(HDC dc, const RECT& clientRect);
  void DrawLogColumnHeaders(HDC dc, const RECT& logArea);
  void DrawLogSidebar(HDC dc, const RECT& sidebarRect);
  bool HitTestLogToolbar(const RECT& clientRect, POINT point);
  bool HitTestLogFilters(const RECT& clientRect, POINT point);
  std::array<RECT, 6> LogFilterRects(const RECT& clientRect) const;
  LogToolbarRect LogToolbarRects(const RECT& clientRect) const;
  int CountFilteredLogs() const;
  void DrawTasksView(HDC dc, const RECT& clientRect);
  void DrawHardwareView(HDC dc, const RECT& clientRect);
  void DrawNetworkView(HDC dc, const RECT& clientRect);
  void DrawScramView(HDC dc, const RECT& clientRect);
  void DrawSettingsView(HDC dc, const RECT& clientRect);
  void DrawTaskContextMenu(HDC dc) const;
  RECT ContentRect(const RECT& clientRect) const;
  std::array<RECT, 6> TabRects(const RECT& clientRect) const;
  std::wstring ComposeLogLine(const LogEntry& entry) const;
  int VisibleLogLines(const RECT& logRect) const;
  int VisibleTaskRows(const RECT& tableRect) const;

  std::atomic<HWND> hwnd_{nullptr};
  HICON largeIcon_ = nullptr;
  HICON smallIcon_ = nullptr;
  HFONT tabFont_ = nullptr;
  HFONT bodyFont_ = nullptr;
  HFONT smallFont_ = nullptr;
  HFONT titleFont_ = nullptr;
  HFONT logFont_ = nullptr;
  HFONT logoFont_ = nullptr;
  int bodyLineHeight_ = 32;
  int smallLineHeight_ = 24;
  int logLineHeight_ = 28;
  int logoLineHeight_ = 34;
  HINSTANCE instance_ = nullptr;
  monix::ui::Win98Assets win98Assets_;
  monix::ui::Win98ThemeFonts win98Fonts_;
  Monix::Security::AuthManager auth_;
  Monix::Kernel::MonixKernel kernel_;
  Monix::Kernel::KernelDisplay kernelDisplay_;
  std::unique_ptr<monix::renderer_vk::ShaderLibrary> shaderLibrary_;
  std::unique_ptr<monix::renderer_vk::ShaderRuntime> shaderRuntime_;
  std::unique_ptr<monix::renderer_vk::ShaderLibraryCompiler> shaderCompiler_;
  std::unique_ptr<monix::renderer_vk::ShaderBrowserPanel> shaderBrowserPanel_;
  monix::renderer_vk::TransactionalShaderState txState_;
  int dpiScale_ = 96;
  AppPaths paths_;
  mutable std::shared_mutex configMutex_;
  Config config_;
  std::wstring fontFace_ = L"Terminal";
  bool privateFontLoaded_ = false;
  ULONGLONG lastLogFlushAtMs_ = 0;
  ULONGLONG lastRetentionSweepAtMs_ = 0;
  std::vector<std::wstring> pendingPlainLogs_;
  std::vector<std::wstring> pendingJsonLogs_;
  std::atomic<bool> running_ = false;
  std::atomic<bool> refreshRequested_ = false;
  bool testMode_ = false;
  bool parityCapture_ = false;
  bool parityCompare_ = false;
  std::wstring parityRefDir_;
  bool runtimeStateEnabled_ = false;
  std::wstring retroarchSnapshotPath_;
  bool regressionTestMode_ = false;
  std::wstring regressionPresetsDir_;
  int regressionFrames_ = 3;
  HANDLE telemetryHandle_{nullptr};
  RenderState openGl_;
  ULONG_PTR gdiplusToken_ = 0;
  int currentBorderIndex_ = 0;
  LARGE_INTEGER qpcFrequency_ {};
  LARGE_INTEGER lastNativeSampleQpc_ {};
  bool nativeBaselineReady_ = false;
  std::uint64_t lastSystemIdleTime_ = 0;
  std::uint64_t lastSystemKernelTime_ = 0;
  std::uint64_t lastSystemUserTime_ = 0;
  std::uint64_t lastNetworkInBytes_ = 0;
  std::uint64_t lastNetworkOutBytes_ = 0;
  std::uint64_t lastDiskReadBytes_ = 0;
  std::uint64_t lastDiskWriteBytes_ = 0;
  std::uint64_t lastDiskReadBytesPerSec_ = 0;
  std::uint64_t lastDiskWriteBytesPerSec_ = 0;
  std::uint64_t lastDiskReadIops_ = 0;
  std::uint64_t lastDiskWriteIops_ = 0;
  bool cpuTimesInitialized_ = false;
  std::map<int, NativeProcessSample> previousProcessSamples_;
  CpuInfo cpuBaseline_ = {};
  bool cpuBaselineCaptured_ = false;
  uint64_t cpuBaseTscPerSec_ = 0;
  monix::TelemetryBaselines baseline_;
  monix::ScramEngine scramEngine_;
  std::unique_ptr<monix::LogManager> logManager_;
  std::unique_ptr<monix::SoundPlayer> soundPlayer_;
  std::unique_ptr<monix::NotificationQueue> notifQueue_;
  mutable std::recursive_mutex stateMutex_;
  AppState state_;
};
