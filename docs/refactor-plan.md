# Monix Architectural Refactoring Plan

## FASE 1: Complete Analysis

### File Structure: main.cpp (12,913 lines)

| Line Range | Content | Size |
|---|---|---|
| 1-85 | Includes, pragmas, using | 85 |
| 85-300 | Constants, enums (Tab, ColorRole, LogFilter), structs (LogButtonRect, FontEntry, AppPaths, LogEntry) | 215 |
| 300-680 | Snapshot struct (378 lines, ~200 fields) | 380 |
| 680-765 | SessionCounters, IntroState, ContextMenuState, NotificationItem, NativeProcessSample, CRTSettings | 85 |
| 765-896 | OpenGlState, AppState struct | 131 |
| 898-1199 | Utility functions (Utf8ToWide, WideToUtf8, Trim, Split, ResolveAppPaths, ReadTextFile, etc.) | 301 |
| 1199-1765 | MonixConfigTypes, MonixApp class forward decls | 566 |
| 1765-2200 | **MonixApp class definition** (62+ member vars, 90+ methods) | 435 |
| 2200-2950 | MonixApp: constructor, LoadConfig, SaveConfig, SettingLabel, SettingValueText, AdjustSetting | 750 |
| 2950-3400 | MonixApp: CommitSettingMutation, fonts, Run, CreateMainWindow, InitializeOpenGlBootstrap | 450 |
| 3400-4200 | MonixApp: RenderOpenGlFrame, rendering pipeline, DrawBackground, DrawTabs, DrawFooter | 800 |
| 4200-5200 | CollectGpuDisplayInfo, CollectNetworkDiagnostics, Collect* functions | 1000 |
| 5200-6300 | PollNativeSnapshot (~1100 lines) | 1100 |
| 6300-10030 | **UpdateScramSummary** (~3730 lines - if/else chain with ~100 rules) | 3730 |
| 10030-12913 | StaticWndProc, WndProc, DrawSettingsView, DrawHardwareView, DrawNetworkView, etc. | 2883 |

### God Functions (>500 lines)

1. **UpdateScramSummary** (L6333-10030, ~3730 lines) - Monolithic if/else chain with ~100 anomaly rules
2. **PollNativeSnapshot** (L5242-6330, ~1088 lines) - Single function polling ALL hardware
3. **WndProc** (L10155-12700, ~2545 lines) - Message handler for all 6 tabs
4. **DrawSettingsView** (L10550-12798, ~2248 lines) - Settings tab rendering
5. **Snapshot struct** (L303-680, ~377 lines, ~200 fields) - Monolithic data structure
6. **AppState struct** (L835-896, ~61 lines, ~62 fields) - Monolithic application state
7. **MonixApp class** (L1765-2113, ~348 lines) - God Object with 62+ member vars, 90+ methods
8. **AdjustSetting** (L2591-2953, ~362 lines) - Large switch/case
9. **DrawHardwareView** - Hardware tab rendering
10. **DrawNetworkView** - Network tab rendering

### Duplicated Patterns

1. Settings row drawing lambda: repeated in General, Display, Logging, System, Performance sub-tabs (5x)
2. Setting click handling: repeated across sub-tabs (3x)
3. Shader delete confirmation: repeated in click and key handlers (2x)
4. Sub-tab rect calculation: repeated in click and draw (2x)

### Dead Code

1. `#if 0` blocks: ~135 lines of dead GL pipeline code
2. `DrawScramView` stub: 5 lines (empty function body)
3. "Dead GL code removed" comments

---

## FASE 2: Target Architecture

### Target Directory Tree

```
src/native/
├── main.cpp                          # ~300 lines: entry point only (wWinMain + command parsing)
│
├── core/
│   ├── Types.hpp                     # Tab, ColorRole, LogFilter, LogButtonRect, LogToolbarRect enums
│   ├── Types.cpp                     # helper_text(), LogViewModeText(), FormatDecimal()
│   └── TextUtils.hpp/.cpp            # Utf8ToWide, WideToUtf8, Trim, TrimAscii, ToLowerAscii, ToUpper, Split, EscapeJson, FormatTemperature, FormatBytes, FormatPercent, ReadTextFile, ReadTtfFaceName, ScanFontDirectory
│
├── config/
│   ├── MonixConfigTypes.hpp          # Config, SettingId (71 enums), SettingMutation, SettingActionRect, LogLevel, LogViewMode, CRTSettings, FontEntry, BorderPreset, Language
│   ├── SettingsRegistry.hpp/.cpp     # SettingDef, 71-entry registry, RegistryLoadSetting, RegistrySaveSetting, RegistrySettingLabel, RegistrySettingValueText
│   └── MonixConfig.hpp/.cpp          # AppPaths, ResolveAppPaths(), BuildCrtSettings(), LoadConfig(), SaveConfig(), TrimHistoryBuffers()
│
├── telemetry/
│   ├── Snapshot.hpp                  # CpuSnapshot, MemorySnapshot, DiskSnapshot, NetworkSnapshot, GpuSnapshot, ThermalSnapshot, PowerSnapshot, SecuritySnapshot, AudioSnapshot, ReliabilitySnapshot, HardwareSnapshot, FilesystemSnapshot, RegistrySnapshot, OsSnapshot + Snapshot (composed)
│   ├── TelemetryState.hpp            # TelemetryState: all prev* delta tracking variables
│   ├── Collectors.hpp/.cpp           # CollectCpu(), CollectMemory(), CollectDisk(), CollectGpuDisplay(), CollectNetworkDiagnostics(), CollectThermal(), CollectPower(), CollectSecurityData(), CollectAudio(), CollectReliability(), CollectFilesystem(), CollectRegistry(), CollectOs()
│   ├── PollNativeSnapshot.hpp/.cpp   # PollNativeSnapshot() orchestrator
│   ├── ProcessUtils.hpp/.cpp         # NativeProcessSample, BuildFallbackProcesses(), RunProcessCapture()
│   └── FormatUtils.hpp/.cpp          # FormatTemperature(), FormatBytes(), FormatPercent(), FormatMemoryStatus(), FormatUptime()
│
├── security/
│   ├── ScramEngine.hpp               # ScramRule interface, ScramResult
│   ├── ScramEngine.cpp               # ScramEngine: evaluates all rules, returns ScramResult
│   ├── ScramRules.hpp                # CpuRule, GpuRule, MemoryRule, ThermalRule, NetworkRule, DiskRule, ProcessRule, SecurityRule, PowerRule, DisplayRule, ReliabilityRule, FilesystemRule, RegistryRule, AudioRule, HardwareRule
│   └── ScramRules.cpp                # Each rule implements IRiskRule
│
├── logging/
│   ├── LogEntry.hpp                  # LogEntry struct, ParseLogLevel(), resolveColor()
│   ├── LogHistory.hpp/.cpp           # LoadRecentLogHistory(), FlushLogQueues(), CleanupLogFiles(), RetryLogEntry(), ParseLogLine()
│   └── LogExport.hpp/.cpp            # ExportLogsJson(), ExportLogsCsv(), ResolveLogFilePath()
│
├── renderer/
│   ├── OpenGlState.hpp               # OpenGlState struct (VulkanRenderer + ShaderRenderer)
│   ├── Renderer.hpp/.cpp             # InitializeOpenGlBootstrap(), ShutdownOpenGlBootstrap(), RenderOpenGlFrame(), EnsureOpenGlUiSurface(), DestroyOpenGlUiSurface()
│   └── VulkanLoadPreset.hpp/.cpp     # LoadPreset(), shader loading logic
│
├── ui/
│   ├── UiFonts.hpp/.cpp              # CreateUiFonts(), DestroyUiFonts(), MeasureFontMetrics()
│   ├── UiDrawing.hpp/.cpp            # DrawPanel(), DrawTextRect(), DrawTextLine(), DrawProgressBar(), DrawSparkline(), ResolveColor(), FillSolid(), DrawRectOutline()
│   ├── LayoutUtils.hpp/.cpp          # ContentRect(), TabRects(), HitTestTabs(), VisibleLogLines(), VisibleTaskRows()
│   ├── DrawLogView.hpp/.cpp          # DrawLogView(), DrawLogToolbar(), DrawLogFilterBadges(), DrawLogColumnHeaders(), DrawLogSidebar(), ComposeLogLine(), CountFilteredLogs()
│   ├── DrawTasksView.hpp/.cpp        # DrawTasksView(), HitTestTaskRow(), OpenTaskMenu(), HandleTaskMenuClick(), ExecuteTaskMenuAction(), DrawTaskContextMenu()
│   ├── DrawHardwareView.hpp/.cpp     # DrawHardwareView()
│   ├── DrawNetworkView.hpp/.cpp      # DrawNetworkView()
│   ├── DrawSettingsView.hpp/.cpp     # DrawSettingsView(), BuildSettingActionRects(), HandleSettingsClick(), HandleSettingsKey()
│   ├── DrawScramView.hpp/.cpp        # DrawScramView()
│   ├── DrawIntro.hpp/.cpp            # DrawIntro(), ComputeIntroLogoMetrics(), SpawnDemoLogs()
│   ├── Notifications.hpp/.cpp        # DrawNotifications(), DrawToast(), QueueNotification(), PlayAlertSound(), PlayLogSound(), PlayClickSound()
│   └── AppState.hpp                  # AppState struct (split into domain-specific sub-structs)
│
├── auth/
│   ├── AuthManager.hpp/.cpp          # (already exists externally)
│   └── LoginOverlay.hpp/.cpp         # (already exists externally)
│
├── platform/
│   ├── Platform.hpp/.cpp             # DPI awareness, DPI scaling, session ID, copy-to-clipboard, GenerateSessionId()
│   └── CrashHandler.hpp/.cpp         # CrashVehHandler, HeapOk, g_phase
│
├── assets/
│   ├── AssetManager.hpp/.cpp         # LoadRuntimeAssets(), border scanning, font scanning
│   └── SoundPlayer.hpp/.cpp          # PlayAlertSound(), PlayLogSound(), PlayClickSound()
│
├── settings/                         # (already exists - no changes)
│   ├── MonixConfigTypes.hpp
│   ├── SettingsRegistry.hpp
│   └── SettingsRegistry.cpp
│
├── vulkan_renderer.h                 # (existing - no changes)
├── vulkan_renderer.cpp               # (existing - no changes)
├── renderer_vk/                      # (existing - no changes)
│   └── ... (57 .hpp + 63 .cpp)
│
├── *_test_unity.cpp                  # (existing test files - no changes)
├── render_backend.h                  # (existing)
└── runtime_state.h                   # (existing)
```

### Current → Destination Mapping

| Current Location (main.cpp) | Destination | Notes |
|---|---|---|
| L1-85: Includes, pragmas | Distributed to each module | Each module includes only what it needs |
| L85-130: Tab, ColorRole, LogFilter enums | `core/Types.hpp` | |
| L130-160: LogButtonRect, FontEntry | `core/Types.hpp` | |
| L160-245: ReadTtfFaceName, ScanFontDirectory | `core/TextUtils.hpp/.cpp` | |
| L247-262: AppPaths struct | `config/MonixConfig.hpp` | |
| L264-300: LogEntry, ProcessInfo, NetworkFlow | `logging/LogEntry.hpp` + `telemetry/ProcessUtils.hpp` | |
| L303-680: Snapshot struct (378 lines) | `telemetry/Snapshot.hpp` | Split into domain sub-structs |
| L682-765: SessionCounters, IntroState, etc. | `ui/AppState.hpp` + `logging/LogEntry.hpp` | |
| L727-766: CRTSettings, BuildCrtSettings | `config/MonixConfigTypes.hpp` + `config/MonixConfig.hpp` | |
| L773-800: Forward decls, TrimQuoted | `core/TextUtils.hpp` | |
| L804-833: OpenGlState | `renderer/OpenGlState.hpp` | |
| L835-896: AppState | `ui/AppState.hpp` | Split into domain sub-structs |
| L898-980: Utf8ToWide, WideToUtf8, Trim, etc. | `core/TextUtils.hpp/.cpp` | |
| L980-1199: ResolveAppPaths, ReadTextFile, etc. | `config/MonixConfig.hpp/.cpp` + `core/TextUtils.hpp/.cpp` | |
| L1199-1765: MonixApp class decl + forward decls | `ui/AppState.hpp` | Class moves to `ui/MonixApp.hpp` |
| L1765-2200: MonixApp class def | `ui/MonixApp.hpp` | |
| L2200-2535: Constructor, LoadConfig, SaveConfig | `config/MonixConfig.hpp/.cpp` | |
| L2535-2556: TrimHistoryBuffers | `config/MonixConfig.hpp/.cpp` | |
| L2557-2589: SettingLabel, SettingValueText | `config/SettingsRegistry.hpp/.cpp` | |
| L2591-2953: AdjustSetting | `config/SettingsRegistry.hpp/.cpp` | |
| L2955-3006: CommitSettingMutation, fonts | `ui/UiFonts.hpp/.cpp` + `config/MonixConfig.hpp/.cpp` | |
| L3006-3041: Font metrics, measure | `ui/UiFonts.hpp/.cpp` | |
| L3043-3072: CrashVehHandler, g_phase | `platform/CrashHandler.hpp/.cpp` | |
| L3074-3101: Run() | `ui/MonixApp.hpp/.cpp` | |
| L3103-3231: InitializeOpenGlBootstrap | `renderer/Renderer.hpp/.cpp` | |
| L3233-3360: DestroyOpenGlUiSurface, EnsureOpenGlUiSurface, RenderOpenGlFrame | `renderer/Renderer.hpp/.cpp` | |
| L3361-3400: SetFrameTimer, CreateMainWindow | `platform/Platform.hpp/.cpp` + `ui/MonixApp.hpp/.cpp` | |
| L3400-4200: DrawBackground, DrawTabs, DrawFooter, DrawSparkline, etc. | `ui/UiDrawing.hpp/.cpp` + `ui/LayoutUtils.hpp/.cpp` | |
| L4200-5200: CollectGpuDisplayInfo, CollectNetworkDiagnostics | `telemetry/Collectors.hpp/.cpp` | |
| L5200-6300: PollNativeSnapshot | `telemetry/PollNativeSnapshot.hpp/.cpp` | |
| L6300-10030: UpdateScramSummary | `security/ScramEngine.hpp/.cpp` + `security/ScramRules.hpp/.cpp` | Split into ~15 rule classes |
| L10030-10067: StaticWndProc, HitTest*, Visible* | `ui/LayoutUtils.hpp/.cpp` + `ui/MonixApp.hpp/.cpp` | |
| L10068-10152: BuildSettingActionRects | `ui/DrawSettingsView.hpp/.cpp` | |
| L10154-10440: HandleSettingsClick | `ui/DrawSettingsView.hpp/.cpp` | |
| L10441-10550: HandleSettingsKey | `ui/DrawSettingsView.hpp/.cpp` | |
| L10550-12798: DrawSettingsView, DrawHardwareView, DrawNetworkView | `ui/DrawSettingsView.hpp/.cpp`, `ui/DrawHardwareView.hpp/.cpp`, `ui/DrawNetworkView.hpp/.cpp` | |
| L12800-12835: DrawTaskContextMenu | `ui/DrawTasksView.hpp/.cpp` | |
| L12835-12913: wWinMain | `main.cpp` | Only entry point stays |

### Dependency Graph (Module → Depends On)

```
main.cpp → ui/MonixApp, platform/CrashHandler
ui/MonixApp → ui/*, config/*, renderer/*, security/*, logging/*, platform/*
ui/* → core/*, config/*
renderer/* → core/*, renderer_vk/*
telemetry/* → core/*, platform/*
security/* → telemetry/Snapshot, core/*
logging/* → core/*
config/* → core/*, settings/*
settings/* → (standalone)
core/* → (standalone)
platform/* → (standalone, Win32)
```

### FASE 3-12 Extraction Order

| Phase | What | Source Lines | Target File | Est. Lines |
|---|---|---|---|---|
| 3 | Core types + TextUtils | L85-130, L130-160, L898-980, L1184-1198 | `core/Types.hpp`, `core/Types.cpp`, `core/TextUtils.hpp/.cpp` | ~250 |
| 4 | Snapshot struct (split into domain sub-structs) | L303-680 | `telemetry/Snapshot.hpp` | ~400 |
| 5 | Remaining structs (LogEntry, ProcessInfo, etc.) | L264-300, L682-765, L804-896 | `logging/LogEntry.hpp`, `telemetry/ProcessUtils.hpp`, `ui/AppState.hpp` | ~300 |
| 6 | Config module (AppPaths, LoadConfig, SaveConfig, BuildCrtSettings) | L247-262, L727-766, L980-1199, L2200-2556 | `config/MonixConfig.hpp/.cpp` | ~600 |
| 7 | Telemetry collectors | L4200-5200, L5200-6300 | `telemetry/Collectors.hpp/.cpp`, `telemetry/PollNativeSnapshot.hpp/.cpp` | ~1500 |
| 8 | SCRAM rules (UpdateScramSummary split into ~15 rule classes) | L6300-10030 | `security/ScramEngine.hpp/.cpp`, `security/ScramRules.hpp/.cpp` | ~4000 |
| 9 | Logging (LogHistory, LogExport, PushLog, ParseLogLine) | L1199-1765 (partial), L2200-2449, L6250-6332 | `logging/LogHistory.hpp/.cpp`, `logging/LogExport.hpp/.cpp` | ~400 |
| 10 | Renderer + UI (fonts, drawing, tabs, views) | L2955-3400, L3400-4200, L10030-12835 | `renderer/Renderer.hpp/.cpp`, `ui/UiFonts.hpp/.cpp`, `ui/UiDrawing.hpp/.cpp`, `ui/LayoutUtils.hpp/.cpp`, `ui/Draw*.hpp/.cpp` | ~3500 |
| 11 | Platform (DPI, crash handler, session) | L3043-3072, L3361-3400 | `platform/Platform.hpp/.cpp`, `platform/CrashHandler.hpp/.cpp` | ~150 |
| 12 | Clean main.cpp (entry point only) | Rewrite | `main.cpp` | ~300 |
| 13 | Build + test verification | — | — | — |
| 14 | Integration error fixes | — | — | — |

### SCRAM Rules Decomposition (Phase 8)

Each rule implements `IRiskRule`:

```cpp
struct ScramResult {
  int riskDelta = 0;
  std::wstring headline;
  std::wstring insight;
  std::vector<std::wstring> diagnostics;
};

struct IRiskRule {
  virtual ~IRiskRule() = default;
  virtual ScramResult evaluate(const Snapshot& current, const Snapshot* previous, const ScramState& state) = 0;
  virtual const char* name() const = 0;
};
```

| Rule Class | Lines Extracted | Responsibility |
|---|---|---|
| CpuPressureRule | L6344-6349 | CPU scheduling pressure |
| GpuSaturationRule | L6351-6427 | GPU utilization, driver changes, resets |
| MemoryPressureRule | L6358-6388 | RAM exhaustion/critical/elevated |
| ThermalDriftRule | L6391-6396 | CPU/GPU/storage thermal |
| VramRule | L6429-6451 | VRAM saturation and growth |
| FrameTimeRule | L6453-6470 | Frame time spikes and backlog |
| DisplayRule | L6472-6498 | Refresh rate, resolution, monitor hotplug |
| NetworkRule | L6499-6700 (approx) | Ping, DNS, adapter changes, VPN, proxy, retransmits |
| DiskRule | ~L6700-6900 | Disk I/O, latency, SMART, NVMe temp |
| ProcessRule | ~L6900-7100 | Process count, thread count, handle leaks |
| SecurityRule | ~L7100-7300 | Signature, hooks, PE tamper, scheduled tasks |
| PowerRule | ~L7300-7500 | Battery, power plan, AC status |
| FilesystemRule | ~L7500-7700 | Volume dirty bit, corruption, USN journal |
| RegistryRule | ~L7700-8100 | Registry hash changes across all hives |
| AudioRule | ~L8100-8300 | Audio device changes, service status |
| HardwareRule | ~L8300-8500 | BIOS changes, TPM, voltage, PCIe errors |
| ReliabilityRule | ~L8500-8700 | SEH exceptions, crashes, watchdog resets |
| OsRule | ~L8700-8900 | Driver count, handles, objects, sessions |
| ScramEngine | L8900-10030 | Orchestrator: evaluates all rules, computes smoothed risk, manages severity |
