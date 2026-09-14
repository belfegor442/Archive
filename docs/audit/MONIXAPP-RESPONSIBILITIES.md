# MonixApp Responsibility Map

> Source files: `MonixApp.hpp` (322 lines), `AppWindowProc.cpp` (1835 lines),
> `TelemetryThread.cpp` (35 lines), `telemetry/runtime/TelemetryThread.cpp` (124 lines),
> `telemetry/snapshot/SnapshotConsumer.cpp` (400 lines)

---

## 1. Window Management

| Member / Method | Source | Notes |
|---|---|---|
| `CreateMainWindow(HINSTANCE, int)` | MonixApp.hpp:128 | Creates Win32 HWND |
| `StaticWndProc(HWND, UINT, WPARAM, LPARAM)` | TelemetryThread.cpp:21-35 | Static thunk; sets `GWLP_USERDATA` on `WM_NCCREATE` |
| `WndProc(HWND, UINT, WPARAM, LPARAM)` | AppWindowProc.cpp:861-1834 | Main message pump (~973 lines) |
| `SetFrameTimer()` | MonixApp.hpp:144 | Posts `WM_TIMER` for repaint cadence |
| `hwnd_` | MonixApp.hpp:247 | `std::atomic<HWND>` |
| `instance_` | MonixApp.hpp:260 | `HINSTANCE` |

**Dependencies:** Font Management (fonts for metrics), Vulkan/OpenGL (for `WM_PAINT` paint path)
**Extraction difficulty:** MEDIUM
**Extraction priority:** 3

---

## 2. Input Handling

All input is dispatched inside `WndProc` (AppWindowProc.cpp:861-1834).

| Message / Handler | Lines | Notes |
|---|---|---|
| `WM_GETMINMAXINFO` | 863-868 | Min track size |
| `WM_SIZE` | 871-873 | Triggers `InvalidateRect` |
| `WM_LBUTTONDOWN` | 874-1229 | Tab clicks, task row clicks, settings, log toolbar, task menu, update dialog (~355 lines) |
| `WM_RBUTTONDOWN` | 1230-1263 | Right-click opens task context menu |
| `WM_LBUTTONUP` | 1265-1273 | Releases button capture |
| `WM_MOUSEMOVE` | 1274-1311 | Hover tracking for task menu, core monitor |
| `WM_MOUSEWHEEL` | 1313-1361 | Scroll logs/tasks/shaders |
| `WM_CHAR` | 1362-1397 | DevShell input, login kernel, shader search |
| `WM_KEYDOWN` | 1398-1763 | Hotkeys, DevShell commands, settings nav, F1-F11, Ctrl+B border, Ctrl+T CRT toggle (~365 lines) |
| `WM_USER+77` | 1765-1783 | Auto-updater callback |
| `WM_TIMER` | 1784-1794 | Animation tick |
| `WM_PAINT` | 1798-1825 | Paint dispatch |

**Helper methods called from input handlers:**
| Method | Lines |
|---|---|
| `HitTestTabs` | 23-31 |
| `HitTestTaskRow` | 595-623 |
| `HitTestLogToolbar` | 44-87 |
| `HitTestLogFilters` | 89-104 |
| `HandleSettingsClick` | 192-477 |
| `HandleSettingsKey` | 480-593 |
| `HandleTaskMenuClick` | 642-657 |
| `OpenTaskMenu` | 625-640 |
| `ExecuteTaskMenuAction` | 699-766 |
| `ComputeViewport` | 768-770 |
| `MapToViewport` | 772-786 |

**Dependencies:** State Management (reads/writes `state_`), UI Rendering (triggers `InvalidateRect`), Configuration (reads `config_`), Logging (`PushLog`), Sound (`PlayClickSound`)
**Extraction difficulty:** HARD — deeply interleaved with state mutation and rendering triggers
**Extraction priority:** 2

---

## 3. UI Rendering

| Method | Approx Line Count | Notes |
|---|---|---|
| `Render(HDC, RECT)` | ~50 | Main GDI render dispatcher (DrawBackground, DrawTabs, per-tab views) |
| `RenderCoreMonitorTheme(HDC, RECT)` | ~40 | Win98 theme alternative render path |
| `IsCoreMonitorThemeActive()` | ~5 | Checks theme mode |
| `TransitionToLoggedIn()` | ~5 | Transitions from login to main UI |
| `DrawBackground(HDC, RECT)` | ~15 | Background fill |
| `DrawSparkline(HDC, RECT, vector, ...)` | ~20 | Mini chart renderer |
| `DrawTabs(HDC, RECT)` | ~30 | Tab strip |
| `DrawFooter(HDC, RECT)` | ~15 | Footer bar |
| `DrawToast(HDC, RECT)` | ~15 | Toast popup |
| `DrawClickDebug(HDC, RECT)` | ~10 | Debug overlay |
| `DrawNotifications(HDC, RECT)` | ~15 | Notification banners |
| `DrawDevShell(HDC, RECT)` | ~20 | DevShell console |
| `DrawIntro(HDC, RECT)` | ~30 | Intro animation screen |
| `DrawPanel(HDC, RECT, ...)` | ~15 | Generic panel |
| `DrawTextRect(HDC, ...)` | ~5 | Text in rect |
| `DrawTextLine(HDC, ...)` | ~5 | Single text line |
| `DrawProgressBar(HDC, ...)` | ~10 | Progress bar |
| `DrawLogView(HDC, RECT)` | ~60 | Log list |
| `DrawLogToolbar(HDC, RECT)` | ~20 | Log toolbar buttons |
| `DrawLogFilterBadges(HDC, RECT)` | ~15 | Log filter pills |
| `DrawLogColumnHeaders(HDC, RECT)` | ~10 | Log column headers |
| `DrawLogSidebar(HDC, RECT)` | ~10 | Log sidebar |
| `DrawTasksView(HDC, RECT)` | ~60 | Process table |
| `DrawHardwareView(HDC, RECT)` | ~40 | Hardware metrics |
| `DrawNetworkView(HDC, RECT)` | ~30 | Network panel |
| `DrawScramView(HDC, RECT)` | ~40 | SCRAM status panel |
| `DrawSettingsView(HDC, RECT)` | ~80 | Settings UI |
| `DrawTaskContextMenu(HDC)` | ~30 | Context menu overlay |
| `ContentRect(RECT)` | ~5 | Content area calculator |
| `TabRects(RECT)` | ~10 | Tab hit-test rects |
| `LogToolbarRects(RECT)` | ~10 | Toolbar rects |
| `LogFilterRects(RECT)` | ~10 | Filter rects |
| `ComposeLogLine(LogEntry)` | ~15 | Log line formatter |
| `VisibleLogLines(RECT)` | ~3 | Line count |
| `VisibleTaskRows(RECT)` | ~3 | Row count |
| `TickAnimations(RECT)` | ~55 | Animation state machine |
| `ComputeIntroLogoMetrics(...)` | ~15 | Intro logo sizing |

**Dependencies:** Font Management (HFONT handles), Configuration (theme, display settings), Telemetry (snapshot data), State Management (`state_`), Shaders (browser panel rendering), SCRAM Engine (SCRAM view data)
**Extraction difficulty:** HARD — tightly coupled to `state_`, `config_`, fonts, and GDI
**Extraction priority:** 1

---

## 4. Vulkan / OpenGL

| Member / Method | Notes |
|---|---|
| `openGl_` (RenderState) | Main renderer state |
| `InitializeOpenGlBootstrap()` | Vulkan init |
| `ShutdownOpenGlBootstrap()` | Vulkan teardown |
| `EnsureOpenGlUiSurface(int, int)` | Surface resize |
| `DestroyOpenGlUiSurface()` | Surface cleanup |
| `DestroyOpenGlResources()` | Resource cleanup |
| `RenderOpenGlFrame(RECT)` | Per-frame render (called from WM_PAINT) |

**Member variables:** `openGl_`, `gdiplusToken_`, `txState_`
**Dependencies:** Configuration (border, display settings), Font Management (font metrics for layout)
**Extraction difficulty:** MEDIUM — well-isolated behind RenderState interface
**Extraction priority:** 2

---

## 5. Telemetry

| Member / Method | Source | Notes |
|---|---|---|
| `StartTelemetry()` | TelemetryThread.cpp:21-24 | Spawns thread |
| `StopTelemetry()` | TelemetryThread.cpp:26-47 | Joins thread (10s timeout) |
| `TelemetryLoop()` | TelemetryThread.cpp:53-124 | Main loop: poll → consume → sleep |
| `PollSnapshot()` | MonixApp.hpp:167 | Gathers system snapshot |
| `PollNativeSnapshot()` | MonixApp.hpp:168 | Native WMI/ETW data |
| `ConsumeSnapshot(Snapshot)` | MonixApp.hpp:171 | Processes snapshot into state |
| `BuildFallbackProcesses(Snapshot)` | MonixApp.hpp:169 | Fallback process list |
| `RunProcessCapture(wstring)` | MonixApp.hpp:170 | External process runner |
| `AppendHistoryPoint(Snapshot)` | MonixApp.hpp:152 | History buffer |
| `TrimHistoryBuffers()` | MonixApp.hpp:185 | History cleanup |
| `SeedReferenceState()` | MonixApp.hpp:182 | Initial baseline |

**Member variables:** `telemetryHandle_`, `baseline_`
**Telemetry subprocess files:**
- `telemetry/snapshot/SnapshotConsumer.cpp` (400 lines) — `ConsumeSnapshot` implementation with risk scoring, threshold detection, UEO correlator
- `telemetry/runtime/TelemetryThread.cpp` (124 lines) — thread lifecycle
- `telemetry/snapshot/SnapshotPoller.hpp` — polling logic

**Dependencies:** Process Monitoring (process samples), Network Monitoring (network counters), SCRAM Engine (risk scoring), Configuration (interval), State Management (`stateMutex_`, `state_`), Logging (`PushLog`), Window Management (`hwnd_` for `PostMessage`)
**Extraction difficulty:** MEDIUM — already partially extracted into `telemetry/` subdirectory
**Extraction priority:** 3

---

## 6. Process Monitoring

| Member Variables | Notes |
|---|---|
| `previousProcessSamples_` | `std::map<int, NativeProcessSample>` — per-PID delta tracking |
| `cpuBaseline_` | `CpuInfo` — initial CPU times |
| `cpuBaselineCaptured_` | bool |
| `cpuTimesInitialized_` | bool |
| `cpuBaseTscPerSec_` | uint64 |

**No dedicated methods** — logic lives inside `PollSnapshot()` / `PollNativeSnapshot()` and `SnapshotConsumer.cpp`.
**Dependencies:** Telemetry (called from telemetry loop), Configuration (interval)
**Extraction difficulty:** EASY — data-only members, logic already in `telemetry/snapshot/`
**Extraction priority:** 5

---

## 7. Network Monitoring

| Member Variables | Notes |
|---|---|
| `lastNetworkInBytes_` | uint64 — previous sample |
| `lastNetworkOutBytes_` | uint64 — previous sample |
| `lastDiskReadBytes_` | uint64 |
| `lastDiskWriteBytes_` | uint64 |
| `lastDiskReadBytesPerSec_` | uint64 |
| `lastDiskWriteBytesPerSec_` | uint64 |
| `lastDiskReadIops_` | uint64 |
| `lastDiskWriteIops_` | uint64 |

**No dedicated methods** — delta computed in `PollSnapshot()` / collectors.
**Dependencies:** Telemetry, SnapshotConsumer
**Extraction difficulty:** EASY — pure data, collectible into a `NetworkCollector` / `DiskCollector`
**Extraction priority:** 4

---

## 8. Configuration

| Member / Method | Notes |
|---|---|
| `config_` | `Config` struct |
| `configMutex_` | `std::shared_mutex` — reader/writer |
| `GetConfig()` | const ref with shared_lock (MonixApp.hpp:162-165) |
| `LoadConfig(bool)` | Reloads from disk |
| `SaveConfig()` | Persists to disk |
| `WriteDefaultConfigIfMissing()` | First-run defaults |
| `paths_` | `AppPaths` — resolved directories |

**Settings system (via `CommitSettingMutation` + `AdjustSetting`):**
| Method | Notes |
|---|---|
| `AdjustSetting(SettingId, int)` | Returns `SettingMutation` |
| `CommitSettingMutation(SettingMutation)` | Applies to `config_` + `state_` |
| `SettingLabel(SettingId)` | Display name |
| `SettingValueText(SettingId)` | Current value string |
| `BuildSettingActionRects(RECT)` | Hit-test rects for settings UI |

**Dependencies:** State Management (settings affect `state_`), Logging (logs config changes)
**Extraction difficulty:** EASY — config is a POD struct behind mutex
**Extraction priority:** 4

---

## 9. Logging

| Member / Method | Notes |
|---|---|
| `PushLog(domain, severity, message, ...)` | Enqueues log entry + optional sound/notification |
| `FlushLogQueues(bool)` | Flushes `pendingPlainLogs_` / `pendingJsonLogs_` to disk |
| `CleanupLogFiles()` | Deletes old log files |
| `CleanupStaleLogFiles()` | Removes stale logs |
| `ExportLogsJson()` | JSON export |
| `ExportLogsCsv()` | CSV export |
| `ResolveLogFilePath(wstring)` | Path resolver |
| `ResolveLogFilePathLocked(wstring, ULONGLONG)` | Path resolver with size cap |

**Member variables:** `logManager_`, `pendingPlainLogs_`, `pendingJsonLogs_`, `lastLogFlushAtMs_`, `lastRetentionSweepAtMs_`
**Dependencies:** Configuration (log settings), Sound (`PlayAlertSound`), Notifications (`QueueNotification`)
**Extraction difficulty:** MEDIUM — `PushLog` is called from everywhere; flush/timer logic mixed with `TickAnimations`
**Extraction priority:** 3

---

## 10. Sound / Audio

| Member / Method | Notes |
|---|---|
| `soundPlayer_` | `std::unique_ptr<SoundPlayer>` |
| `PlayAlertSound(LogLevel)` | Plays sound for error/critical |
| `PlayLogSound()` | Plays log entry sound |
| `PlayClickSound()` | UI click feedback |

**Dependencies:** Configuration (`soundEnabled`), Logging (called from `PushLog`)
**Extraction difficulty:** EASY — `SoundPlayer` is already a separate class
**Extraction priority:** 5

---

## 11. Notifications

| Member / Method | Notes |
|---|---|
| `notifQueue_` | `std::unique_ptr<NotificationQueue>` |
| `QueueNotification(LogEntry)` | Enqueues toast/banner |

**Also uses:** `state_.notifState.toastMessage`, `state_.notifState.items` (in `TickAnimations`)
**Dependencies:** Configuration (`notificationsEnabled`), Logging (`PushLog`)
**Extraction difficulty:** EASY — queue is already extracted; UI rendering is in `DrawNotifications`
**Extraction priority:** 5

---

## 12. SCRAM Engine

| Member / Method | Notes |
|---|---|
| `scramEngine_` | `monix::ScramEngine` |
| `UpdateScramSummary()` | (implied — used in ConsumeSnapshot) |

**Also:** `state_.scramState` (risk score, severity, tracked PID, headline, insight)
**Dependencies:** Telemetry (feeds process data), Process Monitoring (process samples), Configuration (SCRAM settings)
**Extraction difficulty:** MEDIUM — engine is its own class; integration touches `state_`
**Extraction priority:** 4

---

## 13. Shaders

| Member / Method | Notes |
|---|---|
| `shaderLibrary_` | `unique_ptr<ShaderLibrary>` — shader file scanning |
| `shaderRuntime_` | `unique_ptr<ShaderRuntime>` — runtime compilation |
| `shaderCompiler_` | `unique_ptr<ShaderLibraryCompiler>` — batch compile |
| `shaderBrowserPanel_` | `unique_ptr<ShaderBrowserPanel>` — settings UI panel |
| `txState_` | `TransactionalShaderState` — Vulkan shader state |

**Dependencies:** Vulkan/OpenGL (render pipeline), Configuration (shader paths), Window Management (file dialogs)
**Extraction difficulty:** MEDIUM — panel already extracted; integration points in `HandleSettingsClick`, `HandleSettingsKey`, `WM_CHAR`
**Extraction priority:** 4

---

## 14. Authentication

| Member / Method | Notes |
|---|---|
| `auth_` | `Monix::Security::AuthManager` |
| `kernel_` | `Monix::Kernel::MonixKernel` — boot-time auth sequence |
| `kernelDisplay_` | `Monix::Kernel::KernelDisplay` — login screen renderer |

**Dependencies:** Window Management (login screen painting in `DrawIntro`/`Render`), Input (`WM_CHAR`/`WM_KEYDOWN` forwarded to `kernel_`)
**Extraction difficulty:** MEDIUM — login flow is interleaved with render and input
**Extraction priority:** 3

---

## 15. Font Management

| Member / Method | Notes |
|---|---|
| `tabFont_`, `bodyFont_`, `smallFont_`, `titleFont_`, `logFont_`, `logoFont_` | HFONT handles |
| `bodyLineHeight_`, `smallLineHeight_`, `logLineHeight_`, `logoLineHeight_` | Metrics |
| `fontFace_` | Current face name |
| `privateFontLoaded_` | Private font flag |
| `win98Fonts_` | Win98 theme font set |
| `CreateUiFonts()` | Creates all HFONTs |
| `DestroyUiFonts()` | Deletes all HFONTs |
| `MeasureFontMetrics()` | Computes line heights |
| `SwitchFont(int)` | Cycles available fonts |

**Dependencies:** Configuration (font scale, font list from `paths_`)
**Extraction difficulty:** EASY — pure HFONT lifecycle
**Extraction priority:** 5

---

## 16. State Management

| Member / Method | Notes |
|---|---|
| `state_` | `AppState` — all mutable UI/data state |
| `stateMutex_` | `std::recursive_mutex` — guards `state_` |
| `SetToast(wstring)` | Sets toast message |
| `RequestRefresh()` | Sets `refreshRequested_` flag |

**`state_` sub-structs used across categories:**
- `state_.activeTab` — current tab
- `state_.snapshot` — latest telemetry snapshot
- `state_.logState` — log entries, scroll, filter
- `state_.taskScroll`, `state_.selectedTaskIndex`, `state_.selectedTaskPid`
- `state_.taskMenu` — context menu state
- `state_.scramState` — SCRAM risk display
- `state_.intro` — intro animation state
- `state_.shaderUi` — shader browser state
- `state_.notifState` — toast/notification state
- `state_.devShell` — DevShell console
- `state_.updateState` — auto-updater state
- `state_.clickDebug` — debug overlay
- `state_.coreMonitorMenuIndex` — Win98 theme menu
- `state_.settingsCategory` — settings sub-category
- `state_.viewport_` — current viewport rect
- `state_.loggedIn` — auth state

**Dependencies:** Used by EVERY other category
**Extraction difficulty:** HARD — shared mutable state with recursive mutex
**Extraction priority:** 1

---

## Cross-Category Dependency Matrix

```
                  WinMgmt  Input  Render  VK/GL  Telemetry  ProcMon  NetMon  Config  Log    Sound  Notif  SCRAM  Shader  Auth   Font   State
Window Mgmt         -       .      .      .       .         .        .       .       .      .      .      .      .      .      .      .
Input Handling      .       -      .      .       .         .        .       .       .      .      .      .      .      .      .      .
UI Rendering        .       .      -      .       .         .        .       .       .      .      .      .      .      .      .      .
Vulkan/OpenGL       .       .      .      -       .         .        .       .       .      .      .      .      .      .      .      .
Telemetry           .       .      .      .       -         .        .       .       .      .      .      .      .      .      .      .
Proc Monitoring     .       .      .      .       .         -        .       .       .      .      .      .      .      .      .      .
Net Monitoring      .       .      .      .       .         .        -       .       .      .      .      .      .      .      .      .
Configuration       .       .      .      .       .         .        .       -       .      .      .      .      .      .      .      .
Logging             .       .      .      .       .         .        .       .       -      .      .      .      .      .      .      .
Sound               .       .      .      .       .         .        .       .       .      -      .      .      .      .      .      .
Notifications       .       .      .      .       .         .        .       .       .      .      -      .      .      .      .      .
SCRAM               .       .      .      .       .         .        .       .       .      .      .      -      .      .      .      .
Shaders             .       .      .      .       .         .        .       .       .      .      .      .      -      .      .      .
Authentication      .       .      .      .       .         .        .       .       .      .      .      .      .      -      .      .
Font Management     .       .      .      .       .         .        .       .       .      .      .      .      .      .      -      .
State Mgmt          .       .      .      .       .         .        .       .       .      .      .      .      .      .      .      -
```

Key: `X` = hard dependency, `.` = soft/no dependency

---

## Extraction Priority Summary

| Priority | Category | Difficulty | Rationale |
|---|---|---|---|
| **1** | State Management | HARD | All other categories read/write `state_`; must extract first or extract simultaneously |
| **1** | UI Rendering | HARD | Largest surface area (~800+ lines); direct dependency on every data source |
| **2** | Input Handling | HARD | ~973 lines in `WndProc`; deeply interleaved with state mutation |
| **2** | Vulkan/OpenGL | MEDIUM | Already behind `RenderState` interface; clean boundary |
| **3** | Window Management | MEDIUM | Structural — creates HWND, message loop |
| **3** | Telemetry | MEDIUM | Already partially extracted into `telemetry/` subdirectory |
| **3** | Logging | MEDIUM | `PushLog` called everywhere; flush logic mixed with `TickAnimations` |
| **3** | Authentication | MEDIUM | Login flow mixed with render/input |
| **4** | Configuration | EASY | POD struct behind mutex; clean extraction |
| **4** | Network Monitoring | EASY | Pure delta counters; collect into `NetworkCollector` |
| **4** | SCRAM Engine | MEDIUM | Engine class exists; integration touches `state_` |
| **4** | Shaders | MEDIUM | Browser panel already extracted; integration points remain |
| **5** | Process Monitoring | EASY | Data-only members; logic already in `telemetry/snapshot/` |
| **5** | Sound / Audio | EASY | `SoundPlayer` is already a separate class |
| **5** | Notifications | EASY | `NotificationQueue` is already extracted |
| **5** | Font Management | EASY | Pure HFONT lifecycle; no business logic |

---

## Member Variable Count by Category

| Category | Count | Variables |
|---|---|---|
| Window Management | 3 | `hwnd_`, `instance_`, `gdiplusToken_` |
| Input Handling | 0 | (state is in `state_`) |
| UI Rendering | 2 | `currentBorderIndex_`, `dpiScale_` |
| Vulkan/OpenGL | 2 | `openGl_`, `txState_` |
| Telemetry | 1 | `telemetryHandle_` |
| Process Monitoring | 5 | `previousProcessSamples_`, `cpuBaseline_`, `cpuBaselineCaptured_`, `cpuTimesInitialized_`, `cpuBaseTscPerSec_` |
| Network Monitoring | 8 | `lastNetworkInBytes_`, `lastNetworkOutBytes_`, `lastDiskReadBytes_`, `lastDiskWriteBytes_`, `lastDiskReadBytesPerSec_`, `lastDiskWriteBytesPerSec_`, `lastDiskReadIops_`, `lastDiskWriteIops_` |
| Configuration | 3 | `config_`, `configMutex_`, `paths_` |
| Logging | 5 | `logManager_`, `pendingPlainLogs_`, `pendingJsonLogs_`, `lastLogFlushAtMs_`, `lastRetentionSweepAtMs_` |
| Sound / Audio | 1 | `soundPlayer_` |
| Notifications | 1 | `notifQueue_` |
| SCRAM Engine | 1 | `scramEngine_` |
| Shaders | 5 | `shaderLibrary_`, `shaderRuntime_`, `shaderCompiler_`, `shaderBrowserPanel_`, `txState_` |
| Authentication | 3 | `auth_`, `kernel_`, `kernelDisplay_` |
| Font Management | 10 | `tabFont_`, `bodyFont_`, `smallFont_`, `titleFont_`, `logFont_`, `logoFont_`, `bodyLineHeight_`, `smallLineHeight_`, `logLineHeight_`, `logoLineHeight_`, `fontFace_`, `privateFontLoaded_`, `win98Fonts_` |
| State Management | 2 | `state_`, `stateMutex_` |
| Shared / Misc | 5 | `running_`, `refreshRequested_`, `testMode_`, `parityCapture_`, `parityCompare_`, `parityRefDir_`, `runtimeStateEnabled_`, `retroarchSnapshotPath_`, `regressionTestMode_`, `regressionPresetsDir_`, `regressionFrames_`, `win98Assets_`, `qpcFrequency_`, `lastNativeSampleQpc_`, `nativeBaselineReady_`, `lastSystemIdleTime_`, `lastSystemKernelTime_`, `lastSystemUserTime_`, `baseline_` |

**Total member variables:** ~67 (excluding inline methods)
**Total methods:** ~65
**Total method lines (estimated):** ~3,500+ across all `.cpp` files

---

## Recommended Extraction Order

1. **Phase 1 (parallel):** State Management + Font Management + Configuration
2. **Phase 2 (parallel):** Logging + Sound + Notifications + Process Monitoring + Network Monitoring
3. **Phase 3:** Telemetry (depends on Phase 2 collectors) + SCRAM Engine + Shaders
4. **Phase 4:** Input Handling + Authentication
5. **Phase 5:** Vulkan/OpenGL + Window Management
6. **Phase 6:** UI Rendering (last — depends on everything else being extractable)
