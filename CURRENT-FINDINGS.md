# CURRENT-FINDINGS.md — Monix Audit Findings

**Date:** 2025-09-14
**Commit:** `1b13047`

---

## CRITICAL

| # | File | Finding | Status |
|---|------|---------|--------|
| C1 | `vulkan_renderer.cpp` | `syncCreated_` never set → semaphore/fence leak on every shutdown | ✅ FIXED (`5802df5`) |
| C2 | `vulkan_renderer.cpp` | `vkQueueSubmit` return value ignored → fence stall on next frame | ✅ FIXED (`5802df5`) |
| C3 | `vulkan_renderer.cpp` | `vkQueuePresentKHR` return value ignored → stale image index | ✅ FIXED (`5802df5`) |
| C4 | `main.cpp` | `FlushLogQueues()` data race on `pendingPlainLogs_`/`pendingJsonLogs_` | ✅ FIXED (`1b13047`) |

---

## HIGH

| # | File | Finding | Status |
|---|------|---------|--------|
| H1 | `main.cpp:9103` | `Render()` holds `stateMutex_` for entire frame, blocks telemetry | OPEN |
| H2 | `main.cpp:2557` | `ConsumeSnapshot()` holds `stateMutex_` for ~1500 lines of processing | OPEN |
| H3 | `main.cpp:2543` | No exception handling in telemetry thread → crash on any collector failure | OPEN |
| H4 | `main.cpp:8054` | TOCTOU race in `TerminateProcessById()` — PID can be recycled between query and action | OPEN |
| H5 | `main.cpp:8158` | Potential command injection in Explorer launch via unsanitized process name | OPEN |
| H6 | `vulkan_renderer.cpp:962` | `vkBindBufferMemory` return value ignored → GPU crash if fails | ✅ FIXED (`5802df5`) |

---

## MEDIUM

| # | File | Finding | Status |
|---|------|---------|--------|
| M1 | `main.cpp:3791-6762` | `UpdateScramSummary()` is 2971 lines — all rules inline, not using ScramEngine | OPEN |
| M2 | `main.cpp:5692` | Thermal hysteresis rule has mathematically impossible condition (`>X+5 && <X-5`) | OPEN |
| M3 | `main.cpp:8031` | TOCTOU race in `ApplyPriorityToProcess()` — same pattern as H4 | OPEN |
| M4 | `main.cpp:2609` | `RunProcessCapture()` inherits all parent handles — no restriction to needed handles | OPEN |
| M5 | `main.cpp:2543` | Thread shutdown has only 3-second timeout; stuck sensors can orphan thread | OPEN |
| M6 | `main.cpp:3650` | `PushLog()` does O(n) insert at front of vector — inefficient at high log volume | OPEN |
| M7 | `vulkan_renderer.cpp:147-150` | `enumInstExt`/`enumInstLayer` loaded but never used (dead code) | OPEN |

---

## LOW

| # | File | Finding | Status |
|---|------|---------|--------|
| L1 | ScramEngine rules | 12 rule redundancies identified (CPU×5, GPU×4, reboot×2, etc.) | OPEN |
| L2 | ScramEngine rules | 3 definitively dead rules (SecurityIntegrityRule 18, 12, HardwareSensorsRule 18) | OPEN |
| L3 | `main.cpp:1190` | Default `logFlushIntervalMs` = 1200ms — may lose logs on crash | OPEN |
| L4 | `main.cpp:1191` | Default `logMaxFileBytes` = 1MB — no total size cap across all log files | OPEN |

---

## ARCHITECTURAL

| # | File | Finding | Status |
|---|------|---------|--------|
| A1 | `main.cpp` | MonixApp has 265 members, 105 methods, 13 subsystems — needs decomposition | OPEN |
| A2 | `main.cpp` | ~215 of 265 members (81%) are telemetry `prev*` delta tracking fields | OPEN |
| A3 | `main.cpp:3791-6762` | SCRAM rules should use ScramEngine (30 rules) instead of inline 2971-line function | OPEN |
| A4 | `CMakeLists.txt` | Root CMakeLists.txt (183 lines, GLOB-based) is dead — never built | OPEN |
| A5 | Root `src/`, `Monix/Monix/` | Duplicate directory trees — confusing, waste disk, risk of editing wrong copy | OPEN |

---

## CLEANUP

| # | File | Finding | Status |
|---|------|---------|--------|
| CL1 | `original/` | 27 legacy files superseded by src/ refactoring | OPEN |
| CL2 | Root `*.ps1` | 3 temp refactoring scripts (temp_split, temp_fix_headers, temp_fix_offbyone) | OPEN |
| CL3 | Root `build_tests.bat`, `src/build_tests.bat` | Duplicates of Monix/Monix/build_tests.bat | OPEN |
| CL4 | `Monix-2ago-unestable/` | Full repo snapshot — misleading name, should be renamed or removed | OPEN |

---

## Fixes Applied This Session

| Commit | Description |
|--------|-------------|
| `a343342` | Build fixes: RetentionPolicy include, per-test executables, /permissive-, collector includes |
| `5802df5` | Vulkan: syncCreated_ flag, VkResult checking on 6 critical paths |
| `1b13047` | Logging: FlushLogQueues data race fix (swap-under-lock pattern) |
