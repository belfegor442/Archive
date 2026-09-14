# Monix Build System Audit

**Date:** 2026-09-14
**Scope:** CMakeLists.txt, build.ps1, build_run.bat, build_tests.bat (repo root)
**Repository:** D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo

---

## Executive Summary

The Monix build system has **two parallel build paths** that are not in sync:

1. **CMake (CMakeLists.txt)** — intended to be the modern, unified build but has missing source references and diverges significantly from the actual build
2. **build.ps1** — the real production build that compiles ~250 files via a generated batch script, but uses ad-hoc cl.exe invocations instead of a proper build system

**Critical finding:** The CMakeLists.txt references **3 source files that do not exist on disk** (`Draw.cpp`, `Telemetry.cpp`, `Input.cpp` in `src/native/`). It also references `resources.rc` at `src/native/resources.rc` when the actual file is at `Monix/Monix/src/native/resources.rc`. The CMake build will fail at configure time or produce a broken executable.

Additionally, the build.ps1 compiles **~100+ files** that CMakeLists.txt completely omits (scattered MonixApp methods, telemetry collectors, UI render, logging subsystems, platform/win32, updater, etc.), meaning **CMake produces a different binary than build.ps1**.

---

## 1. CMakeLists.txt Analysis

### 1.1 Targets

| Target | Type | Sources | Purpose |
|--------|------|---------|---------|
| `monix_core` | STATIC library | GLOB_RECURSE from `src/core/*` (102 .cpp files) | Ultra Logger core library |
| `monix` | WIN32 executable | Explicit + GLOB + GLOB_RECURSE (~160 files) | Main application |
| `monix_tests` | Console executable | GLOB_RECURSE `tests/core/**/*.cpp` (30 files) | Unit tests |

### 1.2 Source Collection Strategy

**monix_core sources** (GLOB_RECURSE):
- `src/core/events/*.cpp` → 13 files
- `src/core/eventbus/*.cpp` → 8 files
- `src/core/validation/*.cpp` → 13 files
- `src/core/collectors/*.cpp` → 62 files (47 subdirectories)
- `src/core/security/*.cpp` → 1 file
- `src/core/integration/*.cpp` → 1 file
- `src/core/platform/*.cpp` → 4 files (incl. `windows/WindowsPlatform.cpp`)

**monix executable sources** (mixed explicit + GLOB):
- **Explicit MAIN_SOURCES** (12 files):
  - `src/native/main.cpp` ✅
  - `src/native/Draw.cpp` ❌ **MISSING**
  - `src/native/Telemetry.cpp` ❌ **MISSING**
  - `src/native/Input.cpp` ❌ **MISSING**
  - `Monix/Monix/src/native/vulkan_renderer.cpp` ✅
  - `Monix/Monix/src/native/renderer_vk_unity.cpp` ✅
  - `Monix/Monix/src/native/settings/SettingsRegistry.cpp` ✅
  - `Monix/Monix/src/native/core/TextUtils.cpp` ✅
  - `Monix/Monix/src/native/telemetry/Collectors.cpp` ✅
  - `src/native/kernel/KernelDiagnostics.cpp` ✅
  - `src/native/kernel/KernelSelfTest.cpp` ✅
  - `Monix/Monix/src/native/renderer_vk/opengl/GlBackend.cpp` ✅

- **SCRAM sources** (GLOB): `Monix/Monix/src/native/scram/*.cpp` → 1 file (ScramEngine.cpp)
- **SCRAM rules** (GLOB): `Monix/Monix/src/native/scram/rules/*.cpp` → 29 files
- **Logging** (explicit, 3 files): LogManager.cpp, SoundPlayer.cpp, NotificationQueue.cpp
- **Login** (explicit, 6 files): all 6 exist ✅
- **Transitions** (GLOB): `src/native/transitions/*.cpp` → 4 files (excl. transition_tests.cpp)
- **Hardware sensors** (explicit): `Monix/Monix/sensors/hardware.c` ✅
- **MASM assembly**: `Monix/Monix/sensors/cpu.asm` ✅
- **Resource file**: `src/native/resources.rc` ❌ **MISSING** (actual: `Monix/Monix/src/native/resources.rc`)

**monix_tests sources** (GLOB_RECURSE): `tests/core/**/*.cpp` → 30 files

### 1.3 Include Directories

**monix_core** (PUBLIC):
```
src/core
src/core/events
src/core/eventbus
src/core/validation
src/core/collectors
src/core/security
src/core/integration
src/core/platform
src/core/platform/windows
src/native/kernel
```

**monix** (PRIVATE):
```
Monix/Monix/login
Monix/Monix/sensors
src/native
src/native/core
src/native/kernel
src/native/settings
src/native/telemetry
src/native/transitions
src/native/renderer_vk
src/native/scram
Monix/Monix/src/native
Monix/Monix/src/native/renderer_vk
```

### 1.4 Compile Flags

**monix:**
| Flag | Meaning | Issue |
|------|---------|-------|
| `/MT` | Static CRT (MultiThreaded) | Matches build.ps1 ✅ |
| `/Zm800` | PCH memory limit 800MB | Matches build.ps1 (some invocations) |
| `/bigobj` | Increase object file section limit | Matches build.ps1 ✅ |
| `/EHsc` | C++ exception handling | Matches build.ps1 ✅ |
| `/utf-8` | Source + execution charset UTF-8 | Matches build.ps1 ✅ |
| `/w` | Suppress ALL warnings | **See Issue #4** |

**monix_core:**
| Flag | Meaning |
|------|---------|
| `/MT` | Static CRT |
| `/EHsc` | C++ exception handling |
| `/utf-8` | UTF-8 charset |
| `/w` | Suppress ALL warnings |
| `/DWIN32_LEAN_AND_MEAN` | Preprocessor definition |
| `/DMONIX_KERNEL_BUILD` | Preprocessor definition |

**monix_tests:**
| Flag | Meaning |
|------|---------|
| `/MT` | Static CRT |
| `/EHsc` | C++ exception handling |
| `/utf-8` | UTF-8 charset |
| `/w` | Suppress ALL warnings |

### 1.5 Linked Libraries (24 total — monix only)

```
user32 gdi32 psapi comdlg32 shell32 shlwapi
iphlpapi ws2_32 ole32 opengl32 uuid winmm
kernel32 ntdll setupapi bcrypt wintrust crypt32
advapi32 powrprof gdiplus pdh wbemuuid shcore
```

### 1.6 Compile Definitions

| Target | Definitions |
|--------|-------------|
| `monix_core` | `WIN32_LEAN_AND_MEAN`, `MONIX_KERNEL_BUILD`, `UNICODE`, `_UNICODE` |
| `monix` | `UNICODE`, `_UNICODE`, `MONIX_ARCH_X64`, `WIN32_LEAN_AND_MEAN` |
| `monix_tests` | `WIN32_LEAN_AND_MEAN`, `MONIX_KERNEL_BUILD`, `UNICODE`, `_UNICODE` |

### 1.7 MASM Assembly

- Enables `ASM_MASM` language at project level and re-enabled for the target
- Single file: `Monix/Monix/sensors/cpu.asm`
- Uses `set_source_files_properties` to force MASM language

### 1.8 Global Settings

```cmake
cmake_minimum_required(VERSION 3.20)
project(Monix LANGUAGES C CXX ASM_MASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
```

---

## 2. build.ps1 Analysis

### 2.1 Overview

build.ps1 is a PowerShell script that generates a temporary batch file (`build/build-temp.bat`) which compiles every file individually via `cl.exe`, then links everything. It is the **authoritative production build**.

### 2.2 Explicitly Compiled Files (47 individual cl.exe calls)

| # | File | Exists | Notes |
|---|------|--------|-------|
| 1 | `src/native/main.cpp` | ✅ | |
| 2 | `src/native/MonixApp.cpp` | ✅ | **Not in CMake** |
| 3 | `src/native/app/bootstrap/CliParser.cpp` | ✅ | **Not in CMake** |
| 4 | `src/native/tests/renderer_vk/test_pipeline.cpp` | ✅ | **Not in CMake** |
| 5 | `src/native/tests/renderer_vk/test_runtime.cpp` | ✅ | **Not in CMake** |
| 6 | `src/native/crash/CrashHandler.cpp` | ✅ | **Not in CMake** |
| 7 | `src/native/vulkan_renderer.cpp` | ✅ | In CMake ✅ |
| 8 | `src/native/renderer_vk_unity.cpp` | ✅ | In CMake ✅ |
| 9 | `src/native/settings/SettingsRegistry.cpp` | ✅ | In CMake ✅ |
| 10 | `src/native/settings/registry/SettingDefs.cpp` | ✅ | **Not in CMake** |
| 11 | `src/native/settings/serialization/SettingSerializer.cpp` | ✅ | **Not in CMake** |
| 12 | `src/native/core/TextUtils.cpp` | ✅ | In CMake ✅ |
| 13 | `src/native/telemetry/Collectors.cpp` | ✅ | In CMake ✅ |
| 14 | `src/native/kernel/KernelDiagnostics.cpp` | ✅ | In CMake ✅ |
| 15 | `login/LoginOverlay.cpp` | ✅ | In CMake ✅ |
| 16 | `login/MonixKernel.cpp` | ✅ | In CMake ✅ |
| 17 | `login/KernelDisplay.cpp` | ✅ | In CMake ✅ |
| 18 | `login/BootUp.cpp` | ✅ | In CMake ✅ |
| 19 | `login/BootUpDisplay.cpp` | ✅ | In CMake ✅ |
| 20 | `login/GdiPlusLoader.cpp` | ✅ | In CMake ✅ |
| 21 | `renderer_vk/opengl/GlBackend.cpp` | ✅ | In CMake ✅ |
| 22 | `sensors/hardware.c` | ✅ | In CMake ✅ |
| 23 | `sensors/cpu.asm` | ✅ | In CMake ✅ (via ml64.exe) |
| 24 | `src/native/AppWindowProc.cpp` | ✅ | **Not in CMake** |
| 25 | `shader/preset_parser/GlslpPresetParser.cpp` | ✅ | **Not in CMake** |
| 26 | `telemetry/collectors/network/NetworkCollectors.cpp` | ✅ | **Not in CMake** |
| 27 | `telemetry/collectors/sys/SchedulerCollector.cpp` | ✅ | **Not in CMake** |
| 28 | `telemetry/collectors/sys/OsKernelCollector.cpp` | ✅ | **Not in CMake** |
| 29 | `telemetry/collectors/sys/ReliabilityCollector.cpp` | ✅ | **Not in CMake** |
| 30 | `telemetry/collectors/security/SecurityCollector.cpp` | ✅ | **Not in CMake** |
| 31 | `telemetry/collectors/power/PowerCollector.cpp` | ✅ | **Not in CMake** |
| 32 | `telemetry/collectors/thermal/ThermalCollector.cpp` | ✅ | **Not in CMake** |
| 33 | `telemetry/collectors/hw/HardwareBoardCollector.cpp` | ✅ | **Not in CMake** |
| 34 | `telemetry/collectors/fs/FilesystemCollector.cpp` | ✅ | **Not in CMake** |
| 35 | `telemetry/collectors/fs/RegistryCollector.cpp` | ✅ | **Not in CMake** |
| 36 | `telemetry/collectors/audio/AudioCollector.cpp` | ✅ | **Not in CMake** |
| 37 | `telemetry/collectors/gpu/GpuDisplayCollector.cpp` | ✅ | **Not in CMake** |
| 38 | `telemetry/snapshot/ReferenceSeeder.cpp` | ✅ | **Not in CMake** |
| 39 | `telemetry/snapshot/SnapshotPoller.cpp` | ✅ | **Not in CMake** |
| 40 | `telemetry/snapshot/SnapshotConsumer.cpp` | ✅ | **Not in CMake** |
| 41 | `telemetry/state/EntityTracker.cpp` | ✅ | **Not in CMake** |
| 42 | `telemetry/state/SnapshotChangeDetector.cpp` | ✅ | **Not in CMake** |
| 43 | `crash/CrashHandling.cpp` | ✅ | **Not in CMake** |
| 44 | `TelemetryThread.cpp` | ✅ | **Not in CMake** |
| 45 | `scram/ScramEngine.cpp` | ✅ | In CMake (via GLOB) |
| 46 | `updater/AutoUpdater.cpp` | ✅ | **Not in CMake** |
| 47 | (test runners — see below) | | |

### 2.3 Dynamically Compiled Files (for loops — ~205 files)

| Directory | Count | In CMake? |
|-----------|-------|-----------|
| `src/native/app/lifecycle/*.cpp` | 2 | **No** |
| `src/native/app/bootstrap/*.cpp` (excl. CliParser) | 0 | N/A |
| `src/native/platform/win32/**/*.cpp` | 1 | **No** |
| `src/native/logging/*.cpp` | 4 | **Partial** (CMake only lists 3 of 4) |
| `src/native/logging/sound/*.cpp` | 1 | **No** |
| `src/native/logging/notify/*.cpp` | 1 | **No** |
| `src/native/logging/history/*.cpp` | 2 | **No** |
| `src/native/logging/export/*.cpp` | 1 | **No** |
| `src/native/ui/*.cpp` (root) | 1 | **No** |
| `src/native/ui/render/**/*.cpp` | 10 | **No** |
| `src/native/settings/**/*.cpp` (excl. known) | 1 | **No** |
| `src/native/scram/rules/*.cpp` | 29 | In CMake (via GLOB) |
| `src/native/updater/*.cpp` | 1 | **No** |
| `renderer_vk/vk/**/*.cpp` | 17 | **No** |
| `src/core/**/*.cpp` (Ultra Logger) | 102 | In CMake (via GLOB) ✅ |
| `tests/core/**/*Tests.cpp` | 25 | In CMake (via GLOB) ✅ |

### 2.4 Compiler Flags (build.ps1)

| Flag | Frequency | Notes |
|------|-----------|-------|
| `/MT` | Every call | Static CRT |
| `/std:c++20` | Every call | C++20 |
| `/O2` | Most calls | Full optimization (CMake uses none) |
| `/Zm800` | Most calls | PCH limit |
| `/bigobj` | Most calls | Large object sections |
| `/EHsc` | Every call | C++ exceptions |
| `/utf-8` | Every call | UTF-8 charset |
| `/w` | Most calls | Suppress all warnings |
| `/DUNICODE /D_UNICODE` | Most calls | Unicode builds |
| `/DNOMINMAX` | ~20 calls | Prevents min/max macros |
| `/DWIN32_LEAN_AND_MEAN` | Ultra Logger calls | |
| `/DMONIX_KERNEL_BUILD` | Ultra Logger calls | |
| `/D MONIX_ARCH_X64` | LoginOverlay, MonixKernel | Note space after `/D` |
| `/Zm200` | hardware.c, renderer_vk | Different from `/Zm800` |
| `/O1` | hardware.c only | Minimize size |

### 2.5 Linker Configuration

**Output:** `Monix/Monix/build/Monix.exe`

**Linker flags:**
```
/subsystem:windows
/entry:wWinMainCRTStartup
/map:build/Monix.map
```

**Libraries (23):**
```
user32 gdi32 psapi comdlg32 shell32 shlwapi
iphlpapi ws2_32 ole32 opengl32 uuid winmm
kernel32 ntdll setupapi bcrypt wintrust crypt32
advapi32 powrprof gdiplus pdh winhttp
```

### 2.6 Post-Build Steps

1. **rcedit** — Stamps `ico.ico` into the executable via `tools/rcedit/rcedit.exe`
2. **Font copy** — Copies `fonts/*.ttf` to `build/fonts/`
3. **Resource compilation** — Uses `rc.exe` from Windows SDK (graceful fallback if missing)

---

## 3. build_run.bat Analysis

```bat
@echo off
call "...\vcvarsall.bat" x64
cd /d D:\...\Monix\Monix
powershell -ExecutionPolicy Bypass -File build.ps1
echo BUILD_EXIT=%ERRORLEVEL%
```

**Purpose:** Convenience wrapper — sets up MSVC environment and runs build.ps1.
**Issue:** Hardcoded path `D:\Monix-10sept-stable\Monix-31ago-stable\Monix-2ago-unestable\Monix\Monix` — points to a **different directory** (`Monix-2ago-unestable`) than the current repo (`Monix-31ago-stable`). This will fail unless that directory also exists.

---

## 4. build_tests.bat (Repo Root) Analysis

**Location:** `repo/build_tests.bat` (209 lines)

Builds **8 test executables** via direct `cl.exe` invocations (no linking to monix_core — each test is a self-contained unity build):

| # | Executable | Source Files | Notes |
|---|-----------|--------------|-------|
| 1 | `shader_runtime_tests.exe` | `shader_runtime_test_unity.cpp` + `shader_runtime_tests_main.cpp` | 16MB stack |
| 2 | `production_hardening_tests.exe` | `production_hardening_test_unity.cpp` + 2 test files | 16MB stack |
| 3 | `integration_tests.exe` | `integration_test_unity.cpp` ❌ + 2 test files | 16MB stack |
| 4 | `shader_library_tests.exe` | `shader_library_test_unity.cpp` + `shader_library_tests_main.cpp` | 16MB stack |
| 5 | `shader_library_compiler_tests.exe` | `shader_library_compiler_test_unity.cpp` + 1 test file | 32MB stack |
| 6 | `shader_browser_panel_tests.exe` | `shader_browser_panel_test_unity.cpp` + 1 test file | 32MB stack |
| 7 | `gpu_validation_tests.exe` | `gpu_validation_test_unity.cpp` + 1 test file | 32MB stack, links opengl32/user32/gdi32/kernel32 |
| 8 | `fase13_validation_tests.exe` | `fase13_validation_test_unity.cpp` + 1 test file | 32MB stack, links opengl32/user32/gdi32/kernel32 |
| 9 | `external_compat_tests.exe` | `external_compat_test_unity.cpp` + 1 test file | 32MB stack |

**Shared flags across all:** `/MT /std:c++20 /Od /EHsc /bigobj` + `/F` (stack size)
**Missing file:** `integration_test_unity.cpp` does not exist anywhere in the repository.

**Issue:** Hardcoded path `cd /d D:\Monix-2ago-unestable\Monix\Monix` — same stale path as build_run.bat.

There is also a **second copy** at `src/build_tests.bat` (154 lines) that only builds the first 6 targets (missing fase13_validation_tests and external_compat_tests).

---

## 5. Cross-Reference: CMake vs build.ps1

### 5.1 Files in CMake but NOT in build.ps1

| File | Status |
|------|--------|
| `src/native/Draw.cpp` | ❌ **DOES NOT EXIST** |
| `src/native/Telemetry.cpp` | ❌ **DOES NOT EXIST** |
| `src/native/Input.cpp` | ❌ **DOES NOT EXIST** |
| `src/native/resources.rc` | ❌ **DOES NOT EXIST** (actual: `Monix/Monix/src/native/resources.rc`) |
| `Monix/Monix/src/native/logging/SoundPlayer.cpp` | In build.ps1 (via logging GLOB) ✅ |
| `src/native/transitions/*.cpp` (4 files) | **Not in build.ps1 at all** |

### 5.2 Files in build.ps1 but NOT in CMake

This is the **major gap** — build.ps1 compiles ~100+ files that CMake omits entirely:

**Application layer (not in CMake):**
- `MonixApp.cpp`
- `AppWindowProc.cpp`
- `CliParser.cpp`
- `CrashHandler.cpp`
- `CrashHandling.cpp`
- `AutoUpdater.cpp`
- `TelemetryThread.cpp`
- `GlslpPresetParser.cpp`

**Logging subsystem (not in CMake):**
- `LogSink.cpp` (root logging dir)
- `SoundEffects.cpp`
- `Notifications.cpp`
- `HistoryAppend.cpp`, `LogHistoryLoader.cpp`
- `LogExport.cpp`

**UI system (not in CMake):**
- `FontManager.cpp`
- `RenderCoreMonitor.cpp`
- `RenderDispatcher.cpp`
- `RenderOverlay.cpp`
- `RenderHardware.cpp`
- `RenderLog.cpp`
- `RenderNetwork.cpp`
- `RenderScram.cpp`
- `RenderSettings.cpp`
- `RenderTasks.cpp`
- `RenderPrimitives.cpp`

**Settings (not in CMake):**
- `SettingDefs.cpp`
- `SettingSerializer.cpp`
- `SettingHelpers.cpp`

**Platform (not in CMake):**
- `WindowFactory.cpp`

**Telemetry (not in CMake):**
- `NetworkCollectors.cpp`
- `SchedulerCollector.cpp`
- `OsKernelCollector.cpp`
- `ReliabilityCollector.cpp`
- `SecurityCollector.cpp`
- `PowerCollector.cpp`
- `ThermalCollector.cpp`
- `HardwareBoardCollector.cpp`
- `FilesystemCollector.cpp`
- `RegistryCollector.cpp`
- `AudioCollector.cpp`
- `GpuDisplayCollector.cpp`
- `ReferenceSeeder.cpp`
- `SnapshotPoller.cpp`
- `SnapshotConsumer.cpp`
- `EntityTracker.cpp`
- `SnapshotChangeDetector.cpp`
- `ProcessCapture.cpp` (telemetry/runtime)

**Vulkan renderer_vk/vk (not in CMake):** 17 files including:
- `vk_globals.cpp`, `VkBuffer.cpp`, `VkCmdRecording.cpp`, `VkCommand.cpp`
- `VkDescriptor.cpp`, `VkDevice.cpp`, `VkImage.cpp`, `VkInstance.cpp`
- `VkMemory.cpp`, `VkPipeline.cpp`, `VkPreset.cpp`, `VkSampler.cpp`
- `VkScreenshot.cpp`, `VkSwapchain.cpp`, `VkSync.cpp`, `VkUploadTexture.cpp`
- `VkValidation.cpp`

**Test runners (not in CMake monix target):**
- `test_pipeline.cpp`, `test_runtime.cpp`

### 5.3 Files Referenced by CMake That Don't Exist on Disk

| CMake Reference | Resolved Path | Exists? |
|-----------------|---------------|---------|
| `src/native/Draw.cpp` | `D:\...\src\native\Draw.cpp` | ❌ **MISSING** |
| `src/native/Telemetry.cpp` | `D:\...\src\native\Telemetry.cpp` | ❌ **MISSING** |
| `src/native/Input.cpp` | `D:\...\src\native\Input.cpp` | ❌ **MISSING** |
| `src/native/resources.rc` | `D:\...\src\native\resources.rc` | ❌ **MISSING** |

The CMake executable will either fail to configure or link with missing symbols.

---

## 6. Library Link Comparison

| Library | CMake | build.ps1 | Notes |
|---------|-------|-----------|-------|
| user32 | ✅ | ✅ | |
| gdi32 | ✅ | ✅ | |
| psapi | ✅ | ✅ | |
| comdlg32 | ✅ | ✅ | |
| shell32 | ✅ | ✅ | |
| shlwapi | ✅ | ✅ | |
| iphlpapi | ✅ | ✅ | |
| ws2_32 | ✅ | ✅ | |
| ole32 | ✅ | ✅ | |
| opengl32 | ✅ | ✅ | |
| uuid | ✅ | ✅ | |
| winmm | ✅ | ✅ | |
| kernel32 | ✅ | ✅ | |
| ntdll | ✅ | ✅ | |
| setupapi | ✅ | ✅ | |
| bcrypt | ✅ | ✅ | |
| wintrust | ✅ | ✅ | |
| crypt32 | ✅ | ✅ | |
| advapi32 | ✅ | ✅ | |
| powrprof | ✅ | ✅ | |
| gdiplus | ✅ | ✅ | |
| pdh | ✅ | ✅ | |
| **wbemuuid** | ✅ | ❌ | CMake only |
| **shcore** | ✅ | ❌ | CMake only |
| **winhttp** | ❌ | ✅ | build.ps1 only |

---

## 7. Compiler Flag Comparison

| Flag | CMake `monix` | build.ps1 | Consistent? |
|------|---------------|-----------|-------------|
| `/MT` | ✅ | ✅ | ✅ |
| `/std:c++20` | via CMake standard | `/std:c++20` | ✅ (different mechanism) |
| `/O2` | ❌ (no optimization set) | ✅ (most files) | ❌ **CMake builds unoptimized** |
| `/Zm800` | ✅ | ✅ (most) | ⚠️ Some files use `/Zm200` |
| `/bigobj` | ✅ | ✅ (most) | ⚠️ Not all build.ps1 calls |
| `/EHsc` | ✅ | ✅ | ✅ |
| `/utf-8` | ✅ | ✅ | ✅ |
| `/w` | ✅ | ✅ (most) | ⚠️ Some build.ps1 calls omit it |
| `/DUNICODE /D_UNICODE` | via target_compile_definitions | via `/D` flags | ✅ |
| `/DNOMINMAX` | ❌ | ✅ (~20 files) | ❌ **Missing in CMake** |
| `/D MONIX_ARCH_X64` | ✅ (CMake def) | ✅ (some login files) | ⚠️ Inconsistent coverage |

---

## 8. Flag Analysis

### 8.1 `/w` vs `/W4`

- **`/w`** = Suppresses ALL compiler warnings. This is used universally across both builds.
- **`/W4`** = Enables all warnings at level 4 (only ignores level 1 "almost never useful" warnings).
- **Impact:** Using `/w` means the build produces **zero diagnostic output** regardless of code quality. No unused variables, no implicit conversions, no potential bugs are flagged.
- **Recommendation:** Switch to `/W3` minimum, `/W4` ideal. Fix existing warnings incrementally.

### 8.2 `/permissive-`

- **Not used anywhere.** This flag enables standards-conformance mode in MSVC.
- **Impact:** The build uses MSVC's default permissive behavior, which allows non-standard extensions. This can mask portability issues.

### 8.3 `/utf-8` Consistency

- **Consistent** across both CMake and build.ps1. All compilation units use `/utf-8`.
- ✅ No issues.

### 8.4 `/EHsc` Consistency

- **Consistent** across both builds. All .cpp compilations use `/EHsc`.
- ✅ No issues.

### 8.5 Warning Level Assessment

- Both builds suppress warnings with `/w`.
- build.ps1 adds `/DNOMINMAX` to many files — a positive practice that prevents Windows header pollution of `std::min`/`std::max`.
- CMake does **not** define `NOMINMAX`, meaning `std::min`/`std::max` may break in headers included from Windows SDK.

---

## 9. Issues Found

### CRITICAL

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| C1 | **3 nonexistent source files in CMake** — `Draw.cpp`, `Telemetry.cpp`, `Input.cpp` | CMakeLists.txt:59-61 | CMake configure or link will **fail** |
| C2 | **Wrong resource.rc path** — references `src/native/resources.rc` (doesn't exist) instead of `Monix/Monix/src/native/resources.rc` | CMakeLists.txt:104 | Resource compilation will fail; icon/manifest missing |
| C3 | **CMake produces incomplete binary** — ~100+ files compiled by build.ps1 are missing from CMake (MonixApp.cpp, all telemetry collectors, all UI render, all logging subsystems, all vk/* files, updater, crash handler, platform/win32, settings) | CMakeLists.txt:57-70 | CMake binary will have **massive linker errors** and be functionally incomplete |

### HIGH

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| H1 | **Stale paths in build_run.bat and build_tests.bat** — hardcoded `D:\Monix-2ago-unestable\Monix\Monix` | build_run.bat:3, build_tests.bat:4 | Scripts will fail or build from wrong source |
| H2 | **No optimization in CMake** — CMake doesn't set `/O1` or `/O2` for the `monix` target | CMakeLists.txt:154-156 | CMake builds will be dramatically slower |
| H3 | **Missing `integration_test_unity.cpp`** — referenced by build_tests.bat but doesn't exist anywhere in the repo | build_tests.bat:58 | integration_tests target will fail to compile |
| H4 | **No `NOMINMAX` in CMake** — build.ps1 defines it for ~20 files, CMake never does | CMakeLists.txt | Potential `std::min`/`std::max` compilation errors in headers |
| H5 | **Missing libraries** — CMake is missing `winhttp.lib` (needed by updater/network code); build.ps1 is missing `wbemuuid.lib` and `shcore.lib` | CMakeLists.txt:147-151, build.ps1:101 | Link failures depending on which symbols are actually referenced |
| H6 | **Duplicate build_tests.bat** — `repo/build_tests.bat` (8 targets) vs `src/build_tests.bat` (6 targets) | Multiple | Confusion about which to run; stale copy |

### MEDIUM

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| M1 | **All warnings suppressed** (`/w`) | Both builds | Zero diagnostic output; code quality issues silently ignored |
| M2 | **No `/permissive-`** | Both builds | Non-standard extensions allowed; reduces portability |
| M3 | **Inconsistent `/Zm` values** — CMake uses `/Zm800`, build.ps1 uses `/Zm800` for most but `/Zm200` for hardware.c and renderer_vk | Both | Potential PCH memory issues if PCH is introduced |
| M4 | **Test executables not linked in CMake** — `monix_tests` builds core unit tests; build_tests.bat builds shader/renderer tests separately; neither includes the other's tests | Both | No single unified test build |
| M5 | **CMake test target doesn't link Win32 libs** — `monix_tests` only links `monix_core` | CMakeLists.txt:165 | Tests that call Win32 APIs will fail to link |
| M6 | **Transitions compiled in CMake but not in build.ps1** | CMakeLists.txt:95-96 | CMake binary has transition code; build.ps1 binary doesn't |
| M7 | **`/D MONIX_ARCH_X64` only on some build.ps1 files** — LoginOverlay.cpp and MonixKernel.cpp get it; others don't | build.ps1:255,258 | Conditional compilation may behave inconsistently |

### LOW

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| L1 | **GLOB usage in CMake** — `file(GLOB_RECURSE)` doesn't detect new files until reconfigure | CMakeLists.txt:18-24 | New .cpp files won't be compiled until CMake is re-run |
| L2 | **Two separate build trees** — `repo/` has CMakeLists.txt, `Monix/Monix/` has build.ps1; different source roots | Both | Confusion; maintenance burden |
| L3 | **Test file naming inconsistency** — some use `*Tests.cpp`, some `*_main.cpp`, some `*_unity.cpp` | Various | No standard convention for test organization |
| L4 | **`/bigobj` missing from some build.ps1 calls** — CrashHandler.cpp, SettingDefs.cpp, etc. omit it | build.ps1 | May hit section limit on very large translation units |

---

## 10. Recommended Staged Cleanup Plan

### Stage 1: Fix Critical Failures (Immediate)

1. **Remove or replace the 3 nonexistent CMake sources** (`Draw.cpp`, `Telemetry.cpp`, `Input.cpp`) — find what they were supposed to be or delete the references
2. **Fix resource.rc path** in CMakeLists.txt: change `src/native/resources.rc` → `${MONIX_LEAF}/src/native/resources.rc`
3. **Add all missing source files to CMakeLists.txt** — sync CMake with build.ps1's full file list

### Stage 2: Align CMake with build.ps1 (Week 1)

4. Add explicit source lists matching build.ps1's compilation (stop relying on GLOB for the main executable):
   - All telemetry collectors (17 files)
   - All UI render files (11 files)
   - All logging subsystem files (9 files)
   - Platform/win32, AppWindowProc, MonixApp, CliParser, etc.
   - All vk/*.cpp files (17 files)
   - Updater, crash handling, TelemetryThread
5. Add `/O2` optimization flag to CMake `monix` target
6. Add `/DNOMINMAX` to CMake `monix` target
7. Add `winhttp.lib` to CMake link list

### Stage 3: Fix Shared Scripts (Week 1-2)

8. Remove or update stale hardcoded paths in `build_run.bat` and `build_tests.bat` — use `%~dp0` or relative paths
9. Delete duplicate `src/build_tests.bat` or merge into single `build_tests.bat`
10. Add missing `integration_test_unity.cpp` or remove the reference from build_tests.bat

### Stage 4: Improve Code Quality (Week 2-3)

11. Replace `/w` with `/W3` across both builds; fix resulting warnings incrementally
12. Add `/permissive-` for standards conformance
13. Add `/DNOMINMAX` globally (not just per-file in build.ps1)
14. Consider replacing GLOB with explicit file lists in CMake for determinism

### Stage 5: Unify Build Systems (Week 3-4)

15. Decide on a single build system (recommend CMake) and deprecate the other
16. Merge test targets into CMake: add shader_runtime_tests, gpu_validation_tests, etc.
17. Create a CMake Preset (`CMakePresets.json`) for Release/Debug configurations
18. Document build commands in a single README section

---

## 11. Build Commands Reference

### CMake (current state — will fail)

```powershell
# Configure (will error due to missing Draw.cpp, Telemetry.cpp, Input.cpp)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release

# Tests
cmake --build build --target monix_tests --config Release
```

### build.ps1 (production — works)

```powershell
# Full build
cd D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo\Monix\Monix
powershell -ExecutionPolicy Bypass -File build.ps1

# Output: build/Monix.exe
```

### build_tests.bat (repo root)

```cmd
# Builds 8+ test executables (requires integration_test_unity.cpp which is missing)
cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo
build_tests.bat

# Output: Various test .exe files in build/
```

---

*End of audit. Generated from filesystem analysis of the repository at commit current state.*
