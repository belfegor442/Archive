# Monix Threading Model — Audit Report

**Date:** 2026-09-14
**Scope:** Complete thread/synchronization audit of `Monix-31ago-stable-repo`

---

## 1. Thread Inventory

### 1.1 Application Threads

| # | Thread Name / Purpose | Creation Mechanism | Location (file:line) | Join / Stop Mechanism | Lifetime |
|---|----------------------|--------------------|---------------------|-----------------------|----------|
| T1 | **UI / Main Thread** | OS entry point (`WinMain` → `MonixApp::Run`) | `main.cpp:2052` | `GetMessageW` loop exits on `WM_QUIT` | Application duration |
| T2 | **Telemetry Thread** | `_beginthreadex` (Win32) | `main.cpp:2538` | `running_` atomic flag + `WaitForSingleObject(5000ms)` | Application duration (started at line 2069) |
| T3 | **EventBus Dispatcher(s)** | `std::thread` (N threads, configurable) | `EventBus.cpp:27` | `stopFlag_` atomic + `queue_.close()` + `join()` | Per EventBus start/stop |
| T4 | **Filesystem Watcher** | `std::thread` | `FilesystemWatcher.cpp:170` | `stopSignal_` condition variable + `running_` atomic + `join()` | Per watcher start/stop |
| T5 | **Process Collector Poll** | `std::thread` | `ProcessCollector.cpp:47` | `running_` atomic + condition variable `cv_` + `join()` | Per collector start/stop |
| T6 | **User Session Collector Poll** | `std::thread` | `UserSessionCollector.cpp:51` | `running_` atomic + condition variable + `join()` | Per collector start/stop |
| T7 | **Shader Hot-Reload Watcher** | `std::thread` | `ShaderHotReload.cpp:51` | `running_` atomic + `join()` | Per hot-reload start/stop |
| T8 | **Shader Module Creation Worker** | `std::thread` (short-lived) | `vulkan_renderer.cpp:1581` | `done` atomic + `join()` or `detach()` on 5s timeout | Short-lived (up to 5s) |

### 1.2 Thread Notes

- **T2 (Telemetry):** Created with 8 MB stack (`_beginthreadex` at `main.cpp:2538`). `StopTelemetry()` sets `running_ = false` and calls `WaitForSingleObject(telemetryHandle_, 5000)`. If the thread does not exit within 5 seconds, the handle is closed and the thread is leaked (no forced termination).

- **T8 (Shader Worker):** A fire-and-forget background thread for `vkCreateShaderModule` with a 5-second timeout. If the timeout fires, the thread is **detached** (`vulkan_renderer.cpp:1597`), meaning the thread continues running unmonitored. This is intentional to work around a known driver hang bug, but means the detached thread may access dangling captures (`device_`, `createInfo`, `mod`) if the `VulkanRenderer` is destroyed while the thread runs.

---

## 2. Mutex / Synchronization Primitive Inventory

### 2.1 Application-Level Synchronization

| Variable | Type | Location (file:line) | Protects | Acquired By |
|----------|------|---------------------|----------|-------------|
| `stateMutex_` | `std::recursive_mutex` | `main.cpp:1110` | `state_` (AppState), `config_` (Config) | UI thread (T1), Telemetry thread (T2) |
| `validationMutex_` | `std::mutex` | `vulkan_renderer.h:446` | `validationMessages_` vector | Vulkan debug callback thread (driver internal), UI thread via `takeValidationMessages()` |
| `running_` | `std::atomic<bool>` | `main.cpp:878` | Telemetry loop termination signal | T1 (writes), T2 (reads) |
| `refreshRequested_` | `std::atomic<bool>` | `main.cpp:879` | Telemetry immediate-refresh signal | T1 (writes), T2 (reads) |
| `g_phase` | `std::atomic<const char*>` | `main.cpp:2013` | Current execution phase (debug) | T2 (writes), crash handler (reads) |
| `g_telTid` | `std::atomic<DWORD>` | `main.cpp:2014` | Telemetry thread ID | T2 (writes), crash handler (reads) |

### 2.2 Core Collector Synchronization (all `std::mutex mu_`)

Each collector module uses its own `mutable std::mutex mu_` to protect internal state. These are isolated per-collector and do not participate in cross-module locking:

| Module | Mutex Location | Notes |
|--------|---------------|-------|
| `EventBus` | `subMu_` at `EventBus.hpp:71` | Protects `subscriptions_` vector |
| `EventQueue` | `mu_` at `EventQueue.hpp:44` | Producer-consumer queue with `notEmpty_`/`notFull_` condition variables |
| `FilesystemWatcher` | `mu_` at `FilesystemWatcher.hpp:51` | Protects `watchPaths_`, `dirHandles_` |
| `FilesystemCoalescer` | `mu_` at `FilesystemCoalescer.hpp:25` | Protects coalesced event map |
| `FilesystemDeduplicator` | `mu_` at `FilesystemDeduplicator.hpp:28` | Protects dedup set |
| `ProcessCollector` | `mu_` at `ProcessCollector.hpp:66` | Protects process sample data |
| `UserSessionCollector` | `mu_` + `callback_mu_` at `UserSessionCollector.hpp:70-71` | Two-phase lock (data + callback) |
| `NetworkCollector` | `mu_` at `NetworkCollector.hpp:84` | Protects network flow data |
| `StorageCollector` | `mu_` at `StorageCollector.hpp:82` | Protects disk I/O stats |
| `SensorCollector` | `mu_` + `callback_mu_` at `SensorCollector.hpp:64-65` | Two-phase lock (data + callback) |
| `ServiceCollector` | `mu_` at `ServiceCollector.hpp:71` | Protects service enumeration |
| `SelfMonitor` | `mu_` at `SelfMonitor.hpp:113` | Protects self-monitoring counters |
| `SnapshotEngine` | `mu_` at `SnapshotEngine.hpp:51` | Protects aggregated snapshot |
| `EventStorage` | `mu_` at `EventStorage.hpp:133` | Protects event persistence |
| `CollectorConfig` | `mu_` at `CollectorConfig.hpp:115` | Protects config key-value map |
| `CollectorRegistry` | `mu_` at `CollectorRegistry.hpp:26` | Protects collector registry |
| `CollectorManager` | `mu_` at `CollectorManager.hpp:46` | Protects manager state |
| `CollectorMetrics` | `mu_` at `CollectorMetrics.hpp:36` | Protects metrics counters |
| `ShaderLibrary` | `mutex_` at `ShaderLibrary.hpp:70` | Protects shader entry map |
| `ShaderHotReload` | `stateMutex_` + `callbackMutex_` + `pendingMutex_` at `ShaderHotReload.hpp:92-99` | Three separate fine-grained locks |
| `ShaderRenderer` | `loadMutex_` at `ShaderRenderer.hpp:85` | Protects load operations |
| `ShaderDependencyGraph` | `mutex_` at `ShaderDependencyGraph.hpp:81` | Protects dependency graph |
| `KernelEvent` | `mutex_` at `KernelEvent.hpp:109` | Protects kernel event queue |

### 2.3 Global Atomics (Telemetry Counters)

| Variable | Type | Location | Purpose |
|----------|------|----------|---------|
| `g_sehExceptionCount` | `std::atomic<int>` | `Collectors.cpp:52` | SEH exception counter |
| `g_unhandledExceptionCount` | `std::atomic<int>` | `Collectors.cpp:53` | Unhandled exception counter |
| `g_accessViolationCount` | `std::atomic<int>` | `Collectors.cpp:54` | Access violation counter |
| `g_heapCorruptionDetected` | `std::atomic<int>` | `Collectors.cpp:55` | Heap corruption counter |
| `g_assertionFailureCount` | `std::atomic<int>` | `Collectors.cpp:56` | Assertion failure counter |
| `g_stackOverflowCount` | `std::atomic<int>` | `Collectors.cpp:57` | Stack overflow counter |

---

## 3. Shared State Analysis

### 3.1 The `state_` / `config_` / `stateMutex_` System

The `MonixApp` class holds:
- `AppState state_` — all UI state (snapshot, logs, notifications, history buffers, process maps, counters)
- `Config config_` — all runtime configuration
- `mutable std::recursive_mutex stateMutex_` — single lock for both

#### Access Pattern Matrix

| Thread | Operation | Lock Held? | Location |
|--------|-----------|------------|----------|
| **Telemetry (T2)** | Reads `state_.sampleCount` | **NO** | `main.cpp:2674` |
| **Telemetry (T2)** | Reads `state_.hasPreviousSnapshot`, `state_.previousSnapshot` | **NO** | `main.cpp:2677-2678` |
| **Telemetry (T2)** | Reads/writes `prevDriverNames_`, `prevTotalHandles_`, etc. (40+ prev* fields) | **NO** | `main.cpp:3145-3196` |
| **Telemetry (T2)** | Writes `previousProcessSamples_`, `lastNativeSampleQpc_`, `nativeBaselineReady_` | **NO** | `main.cpp:3220-3222` |
| **Telemetry (T2)** | Reads `state_.previousSnapshot->threadCount` | **NO** | `main.cpp:3086` |
| **Telemetry (T2)** | Calls `ConsumeSnapshot()` (writes `state_.*`) | **YES** | `main.cpp:2568` |
| **Telemetry (T2)** | Reads `config_.telemetryIntervalMs` | **YES** | `main.cpp:2579-2580` |
| **UI (T1)** | `PushLog()` reads/writes `state_.logs`, `state_.counters`, `config_*` | **MUST BE YES** (see §4) | `main.cpp:3597+` |
| **UI (T1)** | `FlushLogQueues()` reads `config_*` | **NO** | `main.cpp:3370+` |
| **UI (T1)** | `QueueNotification()` reads/writes `config_*`, `state_.notifications` | **NO** | `main.cpp:3436+` |
| **UI (T1)** | `Render()` reads `state_.*` | **YES** (some paths) | `main.cpp:8722` |
| **UI (T1)** | `TickAnimations()` | **YES** | `main.cpp:8692` |
| **UI (T1)** | WndProc various handlers read `state_.*` | **Mixed** | Various |

### 3.2 The `validationMessages_` / `validationMutex_` System

| Thread | Operation | Lock Held? | Location |
|--------|-----------|------------|----------|
| **Vulkan driver callback** | `addValidationMessage()` — push to vector | **YES** (`validationMutex_`) | `vulkan_renderer.cpp:173` |
| **UI (T1)** | `takeValidationMessages()` — drain vector | **YES** (`validationMutex_`) | `vulkan_renderer.cpp:181` |
| **UI (T1)** | `clearValidationMessages()` | **YES** (`validationMutex_`) | `vulkan_renderer.cpp:189` |
| **UI (T1)** | `validationErrorCount()` — iterates `validationMessages_` | **NO** | `vulkan_renderer.cpp:196-201` |
| **UI (T1)** | `validationWarningCount()` — iterates `validationMessages_` | **NO** | `vulkan_renderer.cpp:204-209` |
| **UI (T1)** | `validationCriticalCount()` — iterates `validationMessages_` | **NO** | `vulkan_renderer.cpp:212-217` |

---

## 4. Data Races Found

### 4.1 CRITICAL: `validationErrorCount()` / `validationWarningCount()` / `validationCriticalCount()` — Data Race

**Severity: HIGH**
**Location:** `vulkan_renderer.cpp:196-218`

**Problem:** These three functions iterate over `validationMessages_` **without acquiring `validationMutex_`**. Meanwhile, the Vulkan debug callback (running on an internal driver thread) calls `addValidationMessage()` which acquires the mutex and modifies the vector via `push_back`.

This is a textbook data race: one thread reads/iterates the vector while another thread mutates it. On MSVC/x86-64 this may manifest as:
- Iterating over a partially-constructed `ValidationMessage` (torn read of `std::string`)
- Reading stale/partial `size()` during vector reallocation
- Crash if `push_back` triggers reallocation mid-iteration

**Fix:** Acquire `validationMutex_` in the count functions, or use atomic counters that are incremented by `addValidationMessage()`.

### 4.2 CRITICAL: `PollNativeSnapshot()` Reads `state_` Without Lock

**Severity: HIGH**
**Location:** `main.cpp:2674, 2677-2678, 3086`

**Problem:** `PollNativeSnapshot()` runs on the **telemetry thread (T2)** and reads `state_.sampleCount`, `state_.hasPreviousSnapshot`, `state_.previousSnapshot` *before* `stateMutex_` is acquired at line 2568. The UI thread (T1) can simultaneously modify these fields (e.g., `ConsumeSnapshot` writes `state_.sampleCount` at line 6850, `state_.hasPreviousSnapshot` at line 6848).

Additionally, `PollNativeSnapshot()` reads `state_.previousSnapshot->threadCount` at line 3086 without any lock.

**Impact:** Torn reads of `sampleCount` (unlikely to corrupt but may produce stale data), or undefined behavior from accessing `previousSnapshot` while it's being replaced.

### 4.3 HIGH: `PushLog()` Called Without Guaranteed Lock

**Severity: HIGH**
**Location:** `main.cpp:3597-3676`

**Problem:** `PushLog()` directly accesses `config_.logLevel`, `config_.logDeduplicate`, `config_.logMilliseconds`, `config_.logBufferSize`, `config_.historyCapacity`, `state_.logs`, `state_.counters`, `state_.nextEventId`, `state_.sessionId`, and multiple `state_.*History` vectors — all without acquiring `stateMutex_`.

`PushLog()` is called from:
1. `ConsumeSnapshot()` (T2, under lock at line 2568) — **SAFE**
2. Various UI thread handlers (T1) — **UNSAFE unless the caller holds the lock**

Callers on the UI thread include WndProc handlers at lines 1767, 1947, 2062-2065 (which do hold the lock at line 2060). However, `QueueNotification()` (line 3436) calls `PushLog()`-adjacent code that modifies `state_.notifications` and reads `config_` without any lock.

### 4.4 HIGH: `FlushLogQueues()` and `QueueNotification()` — Unprotected `config_` Access

**Severity: HIGH**
**Location:** `main.cpp:3370-3434, 3436-3457`

**Problem:**
- `FlushLogQueues()` reads `config_.logFlushIntervalMs`, `config_.logPlainEnabled`, `config_.logJsonEnabled` at lines 3372, 3391, 3410 **without** `stateMutex_`. It is called from the UI thread timer handler.
- `QueueNotification()` reads `config_.notificationsEnabled`, `config_.notificationDurationMs`, `config_.notificationMaxStack` and writes `state_.notifications` at lines 3437, 3451-3454 **without** `stateMutex_`.

If the telemetry thread is simultaneously reading config (it reads `config_.telemetryIntervalMs` under lock), and the UI thread reloads config via `LoadConfig()` (which writes `config_` under lock), the unprotected reads in `FlushLogQueues`/`QueueNotification` are data races on the `Config` struct.

### 4.5 MEDIUM: `config_` — Return-by-Reference Without Lock Lifetime

**Severity: MEDIUM**
**Location:** `main.cpp:1527` (`SettingValueText`)

**Problem:** `SettingValueText()` reads `config_` fields directly. If called from the UI thread without holding the lock, and the config is simultaneously being reloaded, this is a data race. The `Config` struct contains `std::wstring` members (e.g., `userId`, `borderImage`) which are not atomic — a torn read could access freed memory.

### 4.6 MEDIUM: Telemetry Thread `prev*` Fields — No Synchronization

**Severity: MEDIUM**
**Location:** `main.cpp:3145-3196, 3220-3222`

**Problem:** The 40+ `prev*` member variables (e.g., `prevDriverNames_`, `prevTotalHandles_`, etc.) are written by the telemetry thread in `PollNativeSnapshot()` at lines 3145-3196 **without any lock**. These are read in the same function at earlier points. While they are currently only accessed from T2, the pattern is fragile — if any UI code ever reads these, it would be a data race.

### 4.7 LOW: `LoadCommonModules()` Static Cache

**Severity: LOW**
**Location:** `main.cpp:371-401`

**Problem:** Uses `static std::string cached` + `static std::atomic<bool> loaded` with acquire/release semantics. This is a correct double-checked locking pattern. **No data race**, but worth noting as a shared static.

---

## 5. Deadlock Risk Analysis

### 5.1 `recursive_mutex` Masking Lock-Order Issues

**Severity: MEDIUM**
**Location:** `main.cpp:1110`

**Problem:** `stateMutex_` is a `std::recursive_mutex`. This allows the same thread to re-acquire the lock without deadlocking, which hides potential lock-order bugs:

- `ConsumeSnapshot()` (called under lock at line 2568) calls `PushLog()` which does NOT acquire the lock. If `PushLog()` were ever refactored to acquire the lock, the recursive_mutex would silently allow it — but this would mask a real bug if the lock were held in a different order elsewhere.

- `LoadConfig()` (line 1454) acquires `stateMutex_` and then calls `PushLog()` (line 1466) and `TrimHistoryBuffers()` (line 1458). If any code path within those functions tries to acquire `stateMutex_` again (currently it doesn't), the recursive mutex would hide the issue.

**Recommendation:** Replace `std::recursive_mutex` with `std::mutex` and ensure each code path acquires the lock exactly once. This makes lock-ordering violations immediately visible as deadlocks during testing.

### 5.2 `CollectorConfig::merge()` — Nested Lock Acquisition

**Severity: LOW**
**Location:** `CollectorConfig.cpp:304-309`

**Problem:** `merge()` acquires `this->mu_` then `other.mu_`. If two threads call `merge(A, B)` and `merge(B, A)` concurrently, this is a classic ABBA deadlock. However, the mutex is `std::mutex` (not recursive), so this would manifest as an immediate deadlock.

**Mitigation:** This pattern appears safe in practice because `merge()` is likely only called during initialization, not at runtime.

### 5.3 UserSessionCollector / SensorCollector — Two-Phase Locking

**Severity: LOW**
**Location:** `UserSessionCollector.hpp:70-71`, `SensorCollector.hpp:64-65`

**Problem:** These classes use two separate mutexes (`mu_` for data, `callback_mu_` for callbacks). The lock ordering appears consistent (always `mu_` first, then `callback_mu_`), so this is likely safe. But the pattern is inherently risky — a single reversal would deadlock.

### 5.4 Shader Hot-Reload — Three Locks

**Severity: LOW**
**Location:** `ShaderHotReload.hpp:92-99`

**Problem:** Three separate mutexes (`stateMutex_`, `callbackMutex_`, `pendingMutex_`). If any code path acquires them in different orders, deadlock occurs. This requires careful lock-order documentation.

---

## 6. Telemetry Thread Shutdown Analysis

### 6.1 Stop Mechanism

```
main.cpp:2541-2548:
void MonixApp::StopTelemetry() {
  running_ = false;                    // (1) Signal termination
  if (telemetryHandle_) {
    WaitForSingleObject(telemetryHandle_, 5000);  // (2) Wait up to 5s
    CloseHandle(telemetryHandle_);     // (3) Close handle
    telemetryHandle_ = nullptr;
  }
}
```

### 6.2 Issues

1. **5-second timeout without forced termination:** If `TelemetryLoop()` is blocked in a long-running collector (e.g., `CollectSecurityData` which calls `WinVerifyTrust` for every driver), the 5-second timeout will fire, the handle will be closed, and the thread will continue running as a leaked thread. There is no `TerminateThread()` call.

2. **`PollNativeSnapshot()` without lock (§4.2):** During shutdown, the telemetry thread reads `state_` without the lock. If the destructor runs `StopTelemetry()` and then immediately destroys `state_`, the telemetry thread may access destroyed memory.

3. **Destructor calls `StopTelemetry()`:** `~MonixApp()` at line 1151 calls `StopTelemetry()`. If the destructor is called while the message loop is still running (unlikely but possible during abnormal shutdown), there could be a race.

4. **PostMessage after shutdown:** At `main.cpp:2573`, after `ConsumeSnapshot()` completes, the thread posts `WM_MONIX_UPDATE` to the UI window. If the window is being destroyed during shutdown, this could post to an invalid HWND. However, `PostMessageW` is generally safe with invalid HWNDs (it just returns FALSE).

---

## 7. Shared State Matrix

| Shared Object | Telemetry (T2) | UI (T1) | Vulkan Driver | EventBus Workers | Sync Mechanism |
|--------------|----------------|---------|---------------|------------------|----------------|
| `state_.snapshot` | **Writes** (in ConsumeSnapshot) | **Reads** (in Render, Draw*) | — | — | `stateMutex_` (but see §4.2) |
| `state_.logs` | **Writes** (via PushLog in ConsumeSnapshot) | **Reads + Writes** (PushLog, Render) | — | — | `stateMutex_` (but see §4.3) |
| `state_.counters` | **Writes** (via PushLog) | **Reads + Writes** (via PushLog, IncrementCounter) | — | — | `stateMutex_` (but see §4.3) |
| `state_.notifications` | — | **Reads + Writes** (QueueNotification, Render) | — | — | **NONE** (§4.4) |
| `state_.knownProcesses` | **Writes** (ConsumeSnapshot) | **Reads** (DrawTasksView) | — | — | `stateMutex_` |
| `state_.previousSnapshot` | **Writes** (ConsumeSnapshot) | **Reads** (Render) | — | — | `stateMutex_` (but see §4.2) |
| `config_` | **Reads** (under lock) | **Reads + Writes** (LoadConfig, SettingValueText) | — | — | `stateMutex_` (but see §4.4) |
| `validationMessages_` | — | **Reads** (count functions) | **Writes** (debug callback) | — | `validationMutex_` (but see §4.1) |
| `pendingPlainLogs_` | — | **Reads + Writes** (FlushLogQueues) | — | — | **NONE** (single-threaded UI) |
| `pendingJsonLogs_` | — | **Reads + Writes** (FlushLogQueues) | — | — | **NONE** (single-threaded UI) |
| `prev*` fields (40+) | **Reads + Writes** | — | — | — | **NONE** (T2 only, §4.6) |
| `g_sehExceptionCount` etc. | — | **Reads** | — | — | `std::atomic` (safe) |
| `running_` | **Reads** | **Writes** | — | — | `std::atomic` (safe) |
| `refreshRequested_` | **Reads** | **Writes** | — | — | `std::atomic` (safe) |

---

## 8. Recommended Fixes

### 8.1 CRITICAL — Fix Validation Count Data Race

**Severity:** CRITICAL
**File:** `vulkan_renderer.cpp:196-218`

**Option A (preferred):** Add atomic counters to `VulkanRenderer` that are incremented by `addValidationMessage()` and read by the count functions. No mutex needed for reads.

```cpp
// In vulkan_renderer.h, add:
std::atomic<uint32_t> validationErrorCount_{0};
std::atomic<uint32_t> validationWarningCount_{0};
std::atomic<uint32_t> validationCriticalCount_{0};

// In addValidationMessage(), after push_back:
if (msg.severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    validationErrorCount_.fetch_add(1, std::memory_order_relaxed);
// ... etc
```

**Option B:** Acquire `validationMutex_` in the count functions. Simpler but adds contention on the debug callback path.

### 8.2 CRITICAL — Fix `PollNativeSnapshot()` Unguarded `state_` Access

**Severity:** CRITICAL
**File:** `main.cpp:2671-3223`

**Fix:** Move the lock acquisition to before `PollNativeSnapshot()` begins, or refactor `PollNativeSnapshot()` to take a snapshot of the needed `state_` fields under the lock before proceeding with OS queries.

```cpp
while (running_) {
    Snapshot snapshot;
    {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        snapshot = PollSnapshot();  // Now runs under lock
    }
    // ... rest of loop
}
```

Note: This increases lock hold time during OS queries. Consider copying only the needed fields under the lock, then releasing.

### 8.3 HIGH — Add Lock to `PushLog()` Callers

**Severity:** HIGH
**File:** `main.cpp:3597`

**Fix:** Either:
1. Make `PushLog()` acquire `stateMutex_` internally (simplest), or
2. Ensure every caller holds the lock (audit all call sites)

Option 1 is safer. Change `PushLog()` to:
```cpp
void MonixApp::PushLog(...) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    // ... existing body
}
```

This works because `ConsumeSnapshot()` already holds the lock, and `recursive_mutex` allows re-entry.

### 8.4 HIGH — Protect `config_` Reads in `FlushLogQueues` / `QueueNotification`

**Severity:** HIGH
**File:** `main.cpp:3370-3457`

**Fix:** Acquire `stateMutex_` at the start of both functions, or snapshot the needed config values under the lock before proceeding.

### 8.5 MEDIUM — Replace `recursive_mutex` with `mutex`

**Severity:** MEDIUM
**File:** `main.cpp:1110`

**Fix:** Replace `std::recursive_mutex stateMutex_` with `std::mutex stateMutex_`. Audit every call site to ensure the lock is acquired at most once per call chain. This makes lock-ordering bugs immediately visible.

### 8.6 MEDIUM — Document Lock Ordering

**Severity:** MEDIUM

**Fix:** Establish and document a global lock ordering:
1. `stateMutex_` (outermost)
2. `validationMutex_`
3. Collector `mu_` locks (each collector is independent, never held across collectors)
4. `callback_mu_` (always acquired after `mu_`)

### 8.7 LOW — Shader Module Detached Thread Safety

**Severity:** LOW
**File:** `vulkan_renderer.cpp:1597`

**Fix:** When the 5-second timeout fires and the thread is detached, ensure the captures outlive the thread. Currently `device_`, `createInfo`, and `mod` are captured by reference in the lambda. If `VulkanRenderer` is destroyed while the detached thread runs, this is use-after-free.

**Mitigation:** Use a `std::shared_ptr<VkDevice>` or ensure `VulkanRenderer::shutdown()` calls `vkDeviceWaitIdle()` before destruction (which it does at line 326).

### 8.8 LOW — Telemetry Thread Shutdown Robustness

**Severity:** LOW
**File:** `main.cpp:2541-2548`

**Fix:** Consider using an Event object (Win32 `HANDLE`) for cleaner shutdown signaling, and add a second `WaitForSingleObject` with `TerminateThread` as a last resort (though `TerminateThread` is generally discouraged).

---

## 9. Summary

| Category | Count | Severity |
|----------|-------|----------|
| Data Races Found | 7 | 2 CRITICAL, 2 HIGH, 2 MEDIUM, 1 LOW |
| Deadlock Risks | 4 | 0 HIGH, 1 MEDIUM, 3 LOW |
| Thread Shutdown Issues | 2 | 1 HIGH, 1 LOW |
| **Total Issues** | **13** | |

The most dangerous issues are:
1. **§4.1** — `validation*Count()` iterating without the mutex (guaranteed data race with Vulkan driver callback)
2. **§4.2** — `PollNativeSnapshot()` reading `state_` before acquiring the lock (data race with UI thread)
3. **§4.3/4.4** — `PushLog()` and `QueueNotification()` accessing shared state without guaranteed lock coverage

The codebase generally follows good practices: each core collector has its own mutex, atomics are used for simple cross-thread flags, and the telemetry thread properly signals shutdown. The main weakness is the `stateMutex_` coverage gap in the UI thread's log/notification paths.
