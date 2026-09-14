# THREADING-MODEL.md — Monix Concurrency Model

**Date:** 2025-09-14
**Commit:** `5802df5`

---

## Threads

| Thread | Creation | Stack | Purpose |
|--------|----------|-------|---------|
| **Main/UI** | `WinMain` → `GetMessageW` loop | Default | Win32 message pump, all rendering, all input handling |
| **Telemetry** | `_beginthreadex` (line 2531) | 8 MB | `TelemetryLoop()` — polls OS/CPU/RAM/GPU/network/disk/thermal sensors |

---

## Synchronization Primitives

| Primitive | Location | Protects |
|-----------|----------|----------|
| `std::mutex stateMutex_` | `main.cpp:1113` | `state_` (AppState) + `config_` |
| `std::atomic<bool> running_` | `main.cpp:881` | Shutdown flag |
| `std::atomic<bool> refreshRequested_` | `main.cpp:882` | Early-wake signal for telemetry sleep |
| `winraii::ThreadHandle telemetryHandle_` | `main.cpp:892` | Thread lifecycle (WaitForSingleObject + CloseHandle) |

---

## Lock Sites

### stateMutex_ (13 acquisition points)

| Line | Function | Thread | Duration | Risk |
|------|----------|--------|----------|------|
| 1453 | `LoadConfig()` | UI | Short | LOW |
| 2053 | `Run()` | UI | Short | LOW |
| 2557 | `TelemetryLoop()` → `ConsumeSnapshot()` | Telemetry | **~1500 lines** | **HIGH** |
| 2568 | `TelemetryLoop()` (config read) | Telemetry | Short | LOW |
| 8265 | `WndProc(WM_LBUTTONDOWN)` | UI | Short | LOW |
| 8340 | `WndProc(WM_RBUTTONDOWN)` | UI | Short | LOW |
| 8360 | `WndProc(WM_MOUSEMOVE)` | UI | Short | LOW |
| 8377 | `WndProc(WM_MOUSEWHEEL)` | UI | Short | LOW |
| 8416 | `WndProc(WM_CHAR)` | UI | Short | LOW |
| 8445 | `WndProc(WM_KEYDOWN)` | UI | Short | LOW |
| 8707 | `WndProc(WM_TIMER)` | UI | Short | LOW |
| 8737 | `WndProc(WM_PAINT)` | UI | Short | LOW |
| 9103 | `Render()` | UI | **Entire frame** | **HIGH** |

---

## Critical Issues

### 1. Render() holds lock for entire frame (HIGH)

`Render()` acquires `stateMutex_` at line 9103 and holds it for the entire render pass (~4-16ms). This blocks the telemetry thread from writing new snapshots for the full frame duration.

**Impact:** Telemetry thread stalls during rendering, increasing effective telemetry latency.

**Fix:** Use a double-buffered snapshot approach — telemetry writes to a staging buffer, swap atomically at frame boundaries.

### 2. ConsumeSnapshot() holds lock for ~1500 lines (HIGH)

`ConsumeSnapshot()` is called under `stateMutex_` from the telemetry thread. It performs heavy processing (CPU/GPU/RAM analysis, delta detection, event correlation) while holding the lock.

**Impact:** Priority inversion — telemetry processing blocks UI from reading state.

**Fix:** Process snapshot data outside the lock, then acquire lock only for the final state write.

### 3. FlushLogQueues() data race (HIGH)

`FlushLogQueues()` at line 3359 accesses `pendingPlainLogs_` and `pendingJsonLogs_` **without** holding `stateMutex_`. These vectors are written by `PushLog()` (called from telemetry thread under `stateMutex_`).

**Impact:** Use-after-free / data race if `FlushLogQueues` runs concurrently with telemetry.

**Fix:** Acquire `stateMutex_` in `FlushLogQueues()` when accessing pending log vectors.

### 4. Thread shutdown has 3-second timeout (MEDIUM)

`ThreadHandle::reset()` calls `WaitForSingleObject(h, 3000)`. If telemetry is stuck in a long `PollNativeSnapshot()` call (heavy Win32 API enumeration), the timeout expires and the handle is closed without joining.

**Impact:** Thread may still be running, accessing `MonixApp` members that are being destroyed.

**Fix:** Use cooperative shutdown with shorter polling intervals, or increase timeout.

### 5. No lock-free producer/consumer pattern (MEDIUM)

Both threads contend on one coarse mutex (`stateMutex_`). The telemetry thread produces data, the UI thread consumes it for rendering.

**Fix:** Use a lock-free ring buffer or double-buffered snapshot exchange.

---

## Shutdown Protocol

```
1. WM_DESTROY → KillTimer, PostQuitMessage(0)
2. GetMessageW returns 0 → Run() returns
3. ~MonixApp():
   a. StopTelemetry() → running_ = false, telemetryHandle_.reset()
      → WaitForSingleObject(h, 3000) — waits up to 3 seconds
   b. FlushLogQueues(true) — flush pending logs to disk
   c. ShutdownOpenGlBootstrap() — tear down Vulkan/GL
   d. DestroyUiFonts() — release GDI fonts
   e. largeIcon_.reset(), smallIcon_.reset() — release icons
```

**Assessment:** Mostly clean. Weakness: 3-second timeout may orphan thread if telemetry is stuck.

---

## Recommendations

1. **Double-buffered snapshots** — Eliminate lock contention between telemetry and rendering
2. **Protect FlushLogQueues** — Add `stateMutex_` lock when accessing pending log vectors
3. **Lock-free log queue** — Use atomic swap for `pendingPlainLogs_`/`pendingJsonLogs_`
4. **Shorter telemetry polling** — Reduce maximum blocking time in `PollNativeSnapshot()`
5. **Cooperative cancellation** — Add cancellation tokens for heavy Win32 API calls
