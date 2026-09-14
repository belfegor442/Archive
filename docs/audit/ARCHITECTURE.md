# Monix Architecture — Comprehensive Audit

**Generated:** 2026-09-14  
**Repository:** `D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo`  
**Scope:** Native C++ Win32 system monitoring application

---

## 1. Executive Summary

Monix is a **single-process, single-thread Win32 GDI+Vulkan application** that collects ~400 telemetry fields from the Windows OS, evaluates risk through 29 SCRAM rules, and renders a retro Win98-styled dashboard. The entire application is orchestrated by one monolithic class — `MonixApp` — which owns the window, the telemetry thread, the SCRAM engine, the log manager, the Vulkan shader pipeline, and every `WM_PAINT` Draw call.

The codebase lives in **two parallel directory trees** with overlapping responsibilities, creating confusion about which tree is authoritative. The `Monix/Monix/src/native/` tree is the active, extracted version; the root `src/native/main.cpp` is a 10,423-line legacy monolith that has been partially decomposed but never removed.

**Key architectural characteristics:**
- **God Object:** `MonixApp` is 322-line header, ~400+ member variables (legacy), 200+ methods
- **Tight coupling:** Every subsystem (SCRAM, logging, telemetry, UI) is wired through `MonixApp` pointers
- **No dependency injection:** Subsystems call `static_cast<MonixApp*>(param)` to reach each other
- **Monolithic Snapshot:** 413-line struct with ~400 flat fields covering 15+ domains
- **Win32 API surface:** Direct HDC/GDI rendering, `SetWindowLongPtrW`, `_beginthreadex`

---

## 2. Dual-Tree Problem

### Tree Layout

```
Monix-31ago-stable-repo/
├── src/native/                          ← LEGACY TREE (monolith)
│   ├── main.cpp                         ← 10,423 lines, self-contained MonixApp class
│   ├── telemetry/                       ← Partially extracted modules
│   ├── ui/                              ← UI state helpers
│   ├── logging/                         ← Log subsystem
│   ├── settings/                        ← Settings registry
│   ├── config/                          ← Config types
│   ├── renderer_vk/                     ← Vulkan renderer (shared)
│   ├── GL/                              ← OpenGL headers
│   ├── kernel/                          ← Auth/kernel bridge
│   ├── transitions/                     ← UI transitions
│   └── *_unity.cpp                      ← Compilation-unit aggregation files
│
└── Monix/Monix/src/native/              ← ACTIVE TREE (extracted)
    ├── MonixApp.hpp / MonixApp.cpp      ← 322-line class (partial extraction)
    ├── TelemetryThread.cpp              ← WndProc dispatch only
    ├── main.cpp                         ← Entry point
    ├── telemetry/                       ← Full pipeline (25 entries)
    │   ├── runtime/                     ← TelemetryThread, ProcessCapture
    │   ├── snapshot/                    ← Poller, Consumer, ReferenceSeeder
    │   ├── state/                       ← StateStore, TimelineRing, ChangeDetectors
    │   ├── correlation/                 ← CorrelationEngine, CausalChain
    │   ├── contract/                    ← SensorProvider, Measurement interfaces
    │   ├── normalization/               ← Normalizer, Validator
    │   ├── collector/                   ← EtwCollector
    │   ├── collectors/                  ← 9 domain-specific subdirs
    │   ├── etw/                         ← ETW session/provider/collector
    │   ├── export/                      ← WebSocket, HTTP, SCRAM history
    │   └── ...                          ← ChangeDetector, Collectors, baselines
    ├── scram/                           ← SCRAM engine + 29 risk rules
    │   ├── ScramEngine.hpp/.cpp
    │   ├── RiskRule.hpp
    │   ├── risk/                        ← RiskScorer, SeverityTracker, HeadlineGenerator
    │   └── rules/                       ← 29 rule implementations
    ├── logging/                         ← LogManager, LogSink, SoundPlayer, NotificationQueue
    ├── ui/                              ← AppState, Win98 theme, font manager, shaders
    ├── settings/                        ← SettingsRegistry, adjustment logic
    ├── config/                          ← MonixConfig.hpp
    ├── core/                            ← TextUtils, StringUtils, FileUtils, Types
    ├── crash/                           ← CrashHandler, CrashHandling (SEH/VEH)
    ├── renderer_vk/                     ← Vulkan shader library/runtime/compiler
    ├── platform/                        ← Win32 platform layer
    ├── debug/                           ← GpuValidator
    ├── app/                             ← bootstrap (AppConstants) + lifecycle (DemoLogs)
    ├── updater/                         ← AutoUpdater, Version
    ├── tests/                           ← Unit tests
    └── shader/                          ← Shader sources
```

### Overlap Inventory

| Module | `src/native/` | `Monix/Monix/src/native/` | Status |
|--------|---------------|---------------------------|--------|
| MonixApp class | `main.cpp:709-1111` (legacy 322-line definition) | `MonixApp.hpp` (322 lines) | **Duplicate** |
| TelemetryThread | `telemetry/` | `telemetry/runtime/TelemetryThread.cpp` | **Extracted** |
| SnapshotPoller | `telemetry/` | `telemetry/snapshot/SnapshotPoller.cpp` | **Extracted** |
| SnapshotConsumer | `telemetry/` | `telemetry/snapshot/SnapshotConsumer.cpp` | **Extracted** |
| SCRAM rules | `scram/rules/` | `scram/rules/` (29 rules) | **Duplicate** |
| Log subsystem | `logging/` | `logging/` | **Duplicate** |
| Settings | `settings/` | `settings/` | **Duplicate** |
| Config | `config/` | `config/` | **Duplicate** |
| Renderer | `renderer_vk/` | `renderer_vk/` | **Duplicate** |
| UI state | `ui/` | `ui/` | **Duplicate** |

**Impact:** Build system ambiguity, merge conflicts, inconsistent APIs, dead code retention.

---

## 3. Module Map

All paths relative to `Monix/Monix/src/native/`.

| Directory | Purpose | Key Files | Dependencies |
|-----------|---------|-----------|--------------|
| `app/bootstrap/` | Application constants (timer IDs, paths) | `AppConstants.hpp` | — |
| `app/lifecycle/` | Demo log generation | `DemoLogs.hpp` | logging, settings |
| `config/` | Configuration type definitions | `MonixConfig.hpp` | settings |
| `core/` | Text formatting, string utils, file utils, types | `TextUtils.cpp/.hpp`, `StringUtils.hpp`, `FileUtils.hpp`, `Types.hpp` | — |
| `crash/` | SEH/VEH crash handling, crash log writer | `CrashHandler.cpp/.hpp`, `CrashHandling.cpp/.hpp` | — |
| `debug/` | GPU validation layer | `GpuValidator.hpp` | renderer_vk |
| `logging/` | Log manager, sink, sound player, notification queue | `LogManager.cpp/.hpp`, `LogSink.cpp/.hpp`, `SoundPlayer.cpp/.hpp`, `NotificationQueue.cpp/.hpp` | core |
| `logging/export/` | Log file export (JSON, CSV) | `LogExport.hpp` | logging |
| `logging/history/` | Log history append | `HistoryAppend.hpp` | logging |
| `logging/notify/` | Toast notifications | `Notifications.hpp` | logging |
| `logging/sound/` | Sound effects (WAV playback) | `SoundEffects.hpp` | logging |
| `platform/win32/` | Win32 API wrappers | — | — |
| `renderer_vk/` | Vulkan shader library, runtime, compiler, browser panel | 15 subdirectories | — |
| `scram/` | SCRAM engine orchestration | `ScramEngine.cpp/.hpp` | telemetry, core |
| `scram/risk/` | Risk scoring, severity tracking, headline generation | `RiskScorer.hpp`, `SeverityTracker.hpp`, `HeadlineGenerator.hpp` | scram |
| `scram/rules/` | 29 risk rule implementations | `CpuPressureRule.hpp`, `RamExhaustionRule.hpp`, ... | scram, telemetry |
| `settings/` | Settings registry, adjustment logic, serialization | `SettingsRegistry.cpp/.hpp`, `AdjustSetting.hpp`, `SettingGroups.hpp` | config |
| `shader/` | Shader source files (.slang, .hlsl) | — | renderer_vk |
| `telemetry/` | Top-level: Collectors.hpp, Snapshot.hpp, baselines, events | `Collectors.cpp/.hpp`, `Snapshot.hpp`, `TelemetryBaselines.hpp`, `TelemetryEvents.hpp` | — |
| `telemetry/collector/` | ETW collector implementation | `EtwCollector.cpp/.hpp` | telemetry/etw |
| `telemetry/collectors/audio/` | Audio subsystem collector | — | telemetry |
| `telemetry/collectors/fs/` | Filesystem collector | — | telemetry |
| `telemetry/collectors/gpu/` | GPU collector | — | telemetry |
| `telemetry/collectors/hw/` | Hardware sensors collector | — | telemetry |
| `telemetry/collectors/network/` | Network collector | — | telemetry |
| `telemetry/collectors/power/` | Power/battery collector | — | telemetry |
| `telemetry/collectors/security/` | Security/integrity collector | — | telemetry |
| `telemetry/collectors/sys/` | System/process collector | — | telemetry |
| `telemetry/collectors/thermal/` | Thermal/fan collector | — | telemetry |
| `telemetry/contract/` | Interfaces: SensorProvider, SensorState, Measurement, EntityIdentity, TemporalModel | 5 header files | — |
| `telemetry/correlation/` | Correlation engine (16 rules), causal chains | `CorrelationEngine.cpp/.hpp`, `CausalChain.hpp` | telemetry/state |
| `telemetry/etw/` | ETW session management, providers | `EtwSession.hpp`, `EtwProvider.hpp`, `KernelTraceCollector.hpp` | — |
| `telemetry/export/` | WebSocket server, HTTP bridge, SCRAM history ring | `WebSocketServer.hpp`, `TelemetryRelay.hpp`, `ScramHistoryRing.hpp`, `HttpBridge.hpp` | scram |
| `telemetry/normalization/` | Value clamping, validation | `Normalizer.hpp`, `Validator.hpp` | telemetry |
| `telemetry/runtime/` | Telemetry thread lifecycle, process capture | `TelemetryThread.cpp/.hpp`, `ProcessCapture.cpp/.hpp` | MonixApp |
| `telemetry/snapshot/` | Snapshot polling, consumption, reference seeding | `SnapshotPoller.cpp/.hpp`, `SnapshotConsumer.cpp/.hpp`, `ReferenceSeeder.cpp/.hpp` | telemetry, scram, logging |
| `telemetry/state/` | State store, timeline ring, entity tracker, change detectors | `StateStore.cpp/.hpp`, `TimelineRing.hpp`, `EntityTracker.cpp/.hpp`, `ThresholdDetector.cpp/.hpp`, `SnapshotChangeDetector.cpp/.hpp` | telemetry/contract |
| `tests/` | Unit tests | `*_unity.cpp` files | all modules |
| `ui/` | App state, Win98 theme, font manager, render helpers | `AppState.hpp`, `Win98Theme.hpp`, `FontManager.cpp/.hpp` | core |
| `ui/render/` | Tab renderers, sparklines, layout | — | ui |
| `ui/shaders/` | Shader browser panel UI | `ShaderBrowserPanel.hpp` | renderer_vk |
| `updater/` | Auto-updater, version info | `AutoUpdater.cpp/.hpp`, `Version.hpp` | — |

---

## 4. Data Flow

### End-to-End Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SENSOR COLLECTORS                               │
│  9 domain-specific collectors under telemetry/collectors/              │
│  ├── sys/    (CPU, RAM, processes, threads, handles)                   │
│  ├── gpu/    (utilization, VRAM, temperature, drivers, frame time)     │
│  ├── thermal/ (CPU/GPU/board temps, fans, throttling, heat soak)      │
│  ├── network/ (adapters, connections, DNS, latency, VPN)               │
│  ├── disk/   (read/write IOPS, queue length, SMART, NVMe temp)        │
│  ├── power/  (battery, AC, power plan, CPU/system power cap)           │
│  ├── hw/     (SMBIOS, ACPI, TPM, CMOS, PCIe, USB, SATA)              │
│  ├── security/ (unsigned drivers, debug ports, hooks, PE tamper)       │
│  ├── fs/     (file counts, hidden files, USN journal, corruption)     │
│  └── audio/  (devices, volume, sample rate, codec errors)              │
│                                                                        │
│  Additionally:                                                         │
│  ├── telemetry/collector/EtwCollector  (kernel ETW events)             │
│  └── telemetry/etw/KernelTraceCollector (syscall/IRQ/DPC tracking)    │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ raw Snapshot (~400 fields)
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       SNAPSHOT POLLER                                   │
│  telemetry/snapshot/SnapshotPoller.cpp                                  │
│  - Calls PollNativeSnapshot() on MonixApp                              │
│  - Accumulates delta fields (disk bytes/sec, page faults, etc.)        │
│  - Snapshots previous state for each metric domain                    │
│  - Timestamps via QueryPerformanceCounter                              │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    NORMALIZATION + VALIDATION                           │
│  telemetry/normalization/Normalizer.hpp                                 │
│  - Clamps CPU 0-100%, RAM within total, normalizes units               │
│  telemetry/normalization/Validator.hpp                                  │
│  - Validates ranges, detects suspicious/invalid values                 │
│  - Reports ValidationLevel per field                                   │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ normalized Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         STATE STORE                                     │
│  telemetry/state/StateStore.cpp/.hpp                                    │
│  - TimelineRing: 64-snapshot circular buffer                           │
│  - EntityTracker: process/driver lifecycle detection                   │
│  - ChangeSet: field-level change events                                │
│  - Update() → DetectChanges() → Push to timeline                      │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ current + previous Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      CHANGE DETECTION                                   │
│  telemetry/state/IChangeDetector.hpp (interface)                       │
│  ├── ThresholdDetector      (warn/error/critical thresholds)          │
│  ├── SnapshotChangeDetector (multi-field delta comparison)            │
│  └── ProcessChangeDetector  (EntityTracker lifecycle wrapper)          │
│                                                                        │
│  Output: ChangeSet with ChangeEvent vector                             │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ ChangeSet + Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                     CORRELATION ENGINE                                  │
│  telemetry/correlation/CorrelationEngine.cpp/.hpp                      │
│  - 16 pre-configured rules (CPU↔Temp, GPU↔FrameTime, etc.)           │
│  - 6 correlation types (Positive/Negative/Lagging/Leading/            │
│    Threshold/Inverse)                                                   │
│  - EMA-smoothed confidence (0.8 decay)                                 │
│  CausalChain.hpp: cause→effect with lag detection                     │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ CorrelationEvent[]
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    SNAPSHOT CONSUMER                                    │
│  telemetry/snapshot/SnapshotConsumer.cpp                                │
│  - ComputeRiskScore(): CPU(30) + RAM(25) + GPU(15) + Thermal(15)      │
│  - ProcessChangeDetector for process lifecycle events                  │
│  - ThresholdDetector for metric breach detection                       │
│  - Feeds SCRAM engine with current snapshot                            │
│  - Pushes log entries via PushLog()                                    │
│  - Appends history points for sparklines                               │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ Snapshot + ChangeSet
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                      SCRAM RISK EVALUATION                              │
│  scram/ScramEngine.cpp/.hpp                                            │
│  - 29 RiskRule implementations (one per hardware/OS domain)           │
│  ├── RiskScorer:      budget-based scoring, 60pts max, EMA α=0.3    │
│  ├── SeverityTracker: hysteresis Info→Warning→Error→Critical          │
│  └── HeadlineGenerator: formats diagnostic headlines                   │
│                                                                        │
│  Output: ScramResult { headline, insight, diagnostics[], riskScore }  │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ ScramResult
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       LOGGING + NOTIFICATIONS                           │
│  logging/LogManager.cpp/.hpp                                           │
│  - Ring buffer of LogEntry, deduplication, min-level filter            │
│  - onNotification callback → NotificationQueue                        │
│  - onSound callback → SoundPlayer                                      │
│  logging/NotificationQueue.cpp/.hpp                                     │
│  - Toast stacking, expiry timers                                       │
│  logging/SoundPlayer.cpp/.hpp                                           │
│  - WAV playback for alerts, log events, clicks                        │
└──────────────────────────────┬──────────────────────────────────────────┘
                               │ LogEntry[], toast[], sound triggers
                               ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                           UI RENDERING                                  │
│  MonixApp::Render() → Draw*() methods                                  │
│  - DrawBackground, DrawTabs, DrawFooter, DrawToast                    │
│  - DrawLogView, DrawTasksView, DrawHardwareView                       │
│  - DrawNetworkView, DrawScramView, DrawSettingsView                   │
│  - DrawSparkline, DrawProgressBar, DrawNotifications                  │
│  - RenderCoreMonitorTheme (Vulkan overlay)                            │
│                                                                        │
│  Rendering backends:                                                   │
│  - GDI (HDC): primary rendering path                                   │
│  - OpenGL: post-process overlay (optional)                             │
│  - Vulkan: shader browser panel (experimental)                        │
│                                                                        │
│  UI state: ui/AppState.hpp → AppStateData.hpp                          │
│  Theme: ui/Win98Theme.hpp (Win98 look-and-feel)                        │
│  Fonts: ui/FontManager.cpp (private font loading)                      │
└─────────────────────────────────────────────────────────────────────────┘
```

### Export/External Path

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      EXPORT + HISTORY                                   │
│  telemetry/export/TelemetryRelay.hpp                                    │
│  - Broadcasts JSON snapshots + SCRAM updates to WebSocket clients     │
│  - Zero-dependency RFC6455 implementation                             │
│                                                                        │
│  telemetry/export/ScramHistoryRing.hpp                                  │
│  - 128-event circular buffer for SCRAM events                         │
│  - Query: GetRecent(), GetByRiskThreshold(), SerializeRecent()        │
│                                                                        │
│  telemetry/export/HttpBridge.hpp                                        │
│  - GET /status, /history, /health (CORS-enabled)                     │
│                                                                        │
│  telemetry/export/WebSocketServer.hpp                                   │
│  - Port 8421 (WebSocket), Port 8422 (HTTP)                            │
└─────────────────────────────────────────────────────────────────────────┘
         │
         ▼
  External clients:
  ├── Web dashboard (browser)
  ├── Python monitoring scripts
  ├── C# managed bridge (WMI/ETW)
  └── Rust sensor crates
```

---

## 5. Dependency Graph

```
                          ┌──────────┐
                          │  main.cpp │
                          └────┬─────┘
                               │ creates MonixApp
                               ▼
                    ┌─────────────────────┐
                    │      MonixApp        │
                    │  (God Object)        │
                    │  - 200+ methods      │
                    │  - 400+ members      │
                    └──┬──┬──┬──┬──┬──┬───┘
                       │  │  │  │  │  │
        ┌──────────────┘  │  │  │  │  └──────────────┐
        ▼                 ▼  │  ▼  ▼                  ▼
  ┌───────────┐    ┌────────┐│ ┌────────┐      ┌────────────┐
  │  SCRAM    │    │ Logging││ │  UI    │      │ Renderer   │
  │  Engine   │    │ Manager││ │ (Draw) │      │ (Vulkan)   │
  │ 29 Rules  │    └───┬────┘│ └───┬────┘      └────────────┘
  └─────┬─────┘        │     │     │
        │              ▼     │     ▼
        │         ┌────────┐ │  ┌──────────┐
        │         │Sound   │ │  │ AppState │
        │         │Player  │ │  │ (struct) │
        │         └────────┘ │  └──────────┘
        │                    │
        ▼                    ▼
  ┌──────────────────────────────────┐
  │       Telemetry Pipeline         │
  │  ┌─────────────────────────────┐ │
  │  │ SnapshotPoller              │ │
  │  │  → Normalizer/Validator     │ │
  │  │    → StateStore             │ │
  │  │      → ChangeDetectors      │ │
  │  │        → CorrelationEngine  │ │
  │  │          → SnapshotConsumer │ │
  │  └─────────────────────────────┘ │
  └──────────────────────────────────┘
        │
        ▼
  ┌──────────────────────┐
  │  9 Domain Collectors  │
  │  sys gpu thermal net  │
  │  disk power hw sec    │
  │  fs audio             │
  └──────────────────────┘
```

### Key Dependency Edges

| From | To | Coupling Type |
|------|----|---------------|
| MonixApp | ScramEngine | Direct ownership (member) |
| MonixApp | LogManager | Direct ownership (unique_ptr) |
| MonixApp | AppState | Direct ownership (member) |
| MonixApp | SoundPlayer | Direct ownership (unique_ptr) |
| MonixApp | NotificationQueue | Direct ownership (unique_ptr) |
| MonixApp | ShaderLibrary/Runtime | Direct ownership (unique_ptr) |
| TelemetryThread | MonixApp | `static_cast<MonixApp*>(param)` — raw pointer |
| SnapshotPoller | MonixApp | Direct member access (`state_`, `previousProcessSamples_`) |
| SnapshotConsumer | MonixApp | Calls `PushLog()`, `AppendHistoryPoint()`, `SeedReferenceState()` |
| ScramEngine rules | Snapshot | Read-only const ref |
| All UI Draw methods | MonixApp | `this` pointer — member access |

---

## 6. Staged Extraction Plan (10 Phases)

### Phase 1: Remove Legacy Monolith
**Target:** `src/native/main.cpp` (10,423 lines)
- [ ] Verify all functionality in `Monix/Monix/src/native/` covers legacy behavior
- [ ] Delete `src/native/main.cpp`
- [ ] Remove `src/native/*_unity.cpp` compilation units
- [ ] Update build system to exclude `src/native/` tree
- **Risk:** High — legacy may have unique features not yet extracted
- **Estimated effort:** 2-3 days

### Phase 2: Extract Snapshot Struct into Domain Segments
**Target:** `telemetry/Snapshot.hpp` (413 lines, ~400 flat fields)
- [ ] Create `SnapshotCpu`, `SnapshotGpu`, `SnapshotRam`, `SnapshotDisk`, `SnapshotNet`, `SnapshotThermal`, `SnapshotPower`, `SnapshotSecurity`, `SnapshotFs`, `SnapshotAudio`, `SnapshotReliability` sub-structs
- [ ] Compose `Snapshot` from domain sub-structs
- [ ] Update all consumers to use domain-specific accessors
- **Risk:** Medium — many files reference Snapshot fields
- **Estimated effort:** 3-4 days

### Phase 3: Break God Object — Extract Telemetry Loop
**Target:** `MonixApp` telemetry methods
- [ ] Create `TelemetryOrchestrator` class owning the thread + poll + consume loop
- [ ] Move `StartTelemetry()`, `StopTelemetry()`, `TelemetryLoop()`, `PollSnapshot()`, `ConsumeSnapshot()` out of MonixApp
- [ ] Inject `TelemetryOrchestrator` into MonixApp via composition
- [ ] Decouple SnapshotPoller from MonixApp state (pass StateStore instead)
- **Risk:** Medium — requires passing state accessors
- **Estimated effort:** 2-3 days

### Phase 4: Break God Object — Extract UI Renderer
**Target:** MonixApp Draw* methods (20+ methods)
- [ ] Create `MonixRenderer` class owning all `Draw*()` methods
- [ ] Move `Render()`, `DrawBackground()`, `DrawTabs()`, `DrawLogView()`, etc.
- [ ] Pass `AppState` as read-only ref instead of accessing `state_` directly
- [ ] Create `RenderingContext` struct (HDC, fonts, DPI scale)
- **Risk:** Low — Draw methods are mostly self-contained
- **Estimated effort:** 2 days

### Phase 5: Break God Object — Extract Settings Controller
**Target:** MonixApp settings methods
- [ ] Create `SettingsController` class owning settings adjustment logic
- [ ] Move `AdjustSetting()`, `CommitSettingMutation()`, `SettingLabel()`, `SettingValueText()`, `BuildSettingActionRects()`, `HandleSettingsClick()`, `HandleSettingsKey()`
- [ ] Inject `SettingsRegistry` reference
- **Risk:** Low
- **Estimated effort:** 1 day

### Phase 6: Break God Object — Extract Log Controller
**Target:** MonixApp logging methods
- [ ] Create `LogController` class owning log management
- [ ] Move `FlushLogQueues()`, `CleanupLogFiles()`, `QueueNotification()`, `PushLog()`, `ExportLogsJson()`, `ExportLogsCsv()`, `ComposeLogLine()`
- [ ] Inject `LogManager`, `NotificationQueue`, `SoundPlayer` references
- **Risk:** Low
- **Estimated effort:** 1 day

### Phase 7: Introduce Dependency Injection for Telemetry Thread
**Target:** `TelemetryThreadProc` raw pointer passing
- [ ] Create `TelemetryContext` interface with pure virtual methods
- [ ] TelemetryThread receives `TelemetryContext*` instead of `MonixApp*`
- [ ] MonixApp implements `TelemetryContext`
- [ ] SnapshotPoller/SnapshotConsumer receive context reference
- **Risk:** Medium — breaks current coupling
- **Estimated effort:** 1-2 days

### Phase 8: Consolidate Collectors Under Unified Interface
**Target:** `telemetry/collectors/` (9 directories)
- [ ] Define `ICollector` interface in `telemetry/contract/`
- [ ] Each domain collector implements `ICollector::Collect(Snapshot&)` 
- [ ] Create `CollectorRegistry` that runs all collectors
- [ ] Replace direct calls in `PollNativeSnapshot()` with registry iteration
- **Risk:** Low-Medium
- **Estimated effort:** 2 days

### Phase 9: Extract SCRAM Engine to Standalone Library
**Target:** `scram/` directory
- [ ] Remove MonixApp dependency from all 29 rules (pass Snapshot const ref only)
- [ ] Move `ScramEngine` to standalone `.lib` or `.dll`
- [ ] Create `ScramInterface.hpp` with `Evaluate(Snapshot) → ScramResult`
- [ ] MonixApp holds `unique_ptr<ScramInterface>`
- **Risk:** Medium — rules read snapshot directly, should be clean
- **Estimated effort:** 2-3 days

### Phase 10: Eliminate Remaining Cross-Module Coupling
**Target:** Remaining `#include "../../MonixApp.hpp"` in subsystem files
- [ ] Audit all subsystem files for MonixApp includes
- [ ] Replace with interface-based dependencies
- [ ] Ensure no subsystem includes MonixApp.hpp directly
- [ ] Add include-what-you-use enforcement
- [ ] Run build + full test suite
- **Risk:** Low (cleanup phase)
- **Estimated effort:** 1-2 days

### Extraction Summary

| Phase | Target | Effort | Risk | Blockers |
|-------|--------|--------|------|----------|
| 1 | Legacy monolith removal | 2-3d | High | Feature parity verification |
| 2 | Snapshot domain segmentation | 3-4d | Medium | Field reference audit |
| 3 | Telemetry loop extraction | 2-3d | Medium | State access patterns |
| 4 | UI renderer extraction | 2d | Low | — |
| 5 | Settings controller extraction | 1d | Low | — |
| 6 | Log controller extraction | 1d | Low | — |
| 7 | DI for telemetry thread | 1-2d | Medium | Phase 3 |
| 8 | Collector registry | 2d | Low-Med | — |
| 9 | SCRAM standalone | 2-3d | Medium | — |
| 10 | Cross-module cleanup | 1-2d | Low | Phases 1-9 |
| **Total** | | **18-24 days** | | |

---

## 7. Current Issues

### 7.1 God Object (`MonixApp`)

`MonixApp` is the central anti-pattern. It:
- **Owns 200+ methods** spanning window creation, telemetry, rendering, SCRAM, logging, settings, Vulkan shaders, and auth
- **Has 400+ member variables** (legacy version in `src/native/main.cpp:847-1111`) including per-metric delta tracking (`prevRegKeyCountRun`, `prevFanSpeeds_`, etc.)
- **Mixed responsibilities:** Window procedure, telemetry thread, GDI rendering, Vulkan initialization, config I/O, process management, clipboard, context menus
- **Static WndProc trampoline:** Forces `SetWindowLongPtrW`/`GetWindowLongPtrW` pattern

### 7.2 Tight Coupling

| Problem | Example |
|---------|---------|
| Subsystems call back into MonixApp | `SnapshotConsumer.cpp:3` includes `../../MonixApp.hpp` |
| Raw pointer thread parameter | `TelemetryThread.cpp:16`: `static_cast<MonixApp*>(param)` |
| State access via mutex + direct member | `stateMutex_` + `state_` accessed everywhere |
| Config access via shared_mutex | `GetConfig()` returns const ref with lock |
| SCRAM rules depend on MonixApp state | Rules read `Snapshot` but MonixApp mediates |

### 7.3 Monolithic Snapshot Struct

`telemetry/Snapshot.hpp` is 413 lines with ~400 flat fields. Problems:
- **No encapsulation:** Any field can be read/written by any consumer
- **Domain mixing:** CPU, GPU, thermal, audio, filesystem, registry, security, reliability all in one struct
- **Delta tracking in MonixApp:** Previous-sample deltas stored as MonixApp members (lines 298-315) instead of in a dedicated DeltaStore
- **Copy overhead:** 400+ fields copied per telemetry tick

### 7.4 Dual-Tree Maintenance Burden

- Two copies of `MonixApp`, `TelemetryThread`, SCRAM rules, logging, settings, config, renderer
- Build system must choose which tree to compile
- Merge conflicts when changes span both trees
- Dead code retention in legacy tree

### 7.5 No Dependency Injection

- All subsystems obtain MonixApp via raw pointer or `this`
- No interfaces for testability (only `IChangeDetector` exists as an abstraction)
- SCRAM rules could be independently tested but depend on MonixApp for state access

### 7.6 Missing Separation of Concerns

| Concern | Current Owner | Should Be |
|---------|--------------|-----------|
| Window management | MonixApp | Win32Window class |
| Telemetry orchestration | MonixApp | TelemetryOrchestrator |
| UI rendering | MonixApp | MonixRenderer |
| Settings adjustment | MonixApp | SettingsController |
| Log management | MonixApp | LogController |
| Config I/O | MonixApp (via config/) | ConfigManager |
| Process enumeration | MonixApp (via telemetry/) | CollectorRegistry |
| SCRAM evaluation | MonixApp (via scram/) | ScramEngine (standalone) |

### 7.7 Thread Safety Concerns

- `stateMutex_` is a `recursive_mutex` — potential for deadlocks in nested locks
- `configMutex_` is a `shared_mutex` — readers can starve writers
- `running_` and `refreshRequested_` are `atomic<bool>` — correct but inconsistent with mutex usage elsewhere
- `hwnd_` is `atomic<HWND>` — proper for cross-thread access

### 7.8 Build System Complexity

- Unity files (`*_unity.cpp`) aggregate compilation units — hides include dependencies
- No module system (C++20 modules not used)
- Header-only dependencies in many subdirectories
- Vulkan headers vendored in `vulkan-headers/`

---

## Appendix: Key File Sizes

| File | Lines | Role |
|------|-------|------|
| `src/native/main.cpp` (legacy) | 10,423 | Monolith — entire application |
| `MonixApp.hpp` | 322 | God Object header |
| `MonixApp.cpp` | 281 | Constructor, destructor, config, Run() |
| `telemetry/Snapshot.hpp` | 413 | 400-field telemetry struct |
| `telemetry/snapshot/SnapshotPoller.cpp` | 718 | Raw telemetry collection |
| `telemetry/snapshot/SnapshotConsumer.cpp` | 400 | Snapshot processing + risk scoring |
| `telemetry/runtime/TelemetryThread.cpp` | 124 | Thread lifecycle + main loop |
| `scram/rules/` (29 files) | ~58 total | Risk rule implementations |
| `ARCHITECTURE.md` (existing) | 242 | Previous architecture doc |

---

*End of audit.*
