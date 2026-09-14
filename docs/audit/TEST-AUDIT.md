# Monix Test Infrastructure Audit

**Date:** 2026-09-14
**Scope:** All test code, build scripts, entry points, and coverage
**Repository:** D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo

---

## Executive Summary

The Monix test infrastructure consists of **three separate build systems** that don't overlap cleanly:

1. **CMake `monix_tests` target** — builds 30 core library unit tests (events, eventbus, validation, collectors)
2. **`build_tests.bat` (repo root)** — builds 8-9 shader/renderer test executables via unity builds
3. **`Monix/Monix/build_tests.bat`** — builds 10+ test executables (superset of #2, includes events and workspace tests)
4. **Inline tests in production binary** — `test_pipeline.cpp` and `test_runtime.cpp` compiled into `Monix.exe`, activated via `--test` and `--test-vulkan` CLI flags

**Critical finding:** The production executable contains embedded test code (`test_pipeline.cpp`, `test_runtime.cpp`) that is **always compiled** into the shipping binary. Tests can be activated via `--test` flag which calls `extern void runCompilationPipelineTests()` and `extern void runMegadrivePresetTests()` — these symbols are defined in test source files that are part of the build. This means **test code is linked into the production executable**.

---

## 1. Extern Test Declarations in main.cpp

Found at `src/native/main.cpp:10401-10411`:

```cpp
if (testMode) {
    app.SetTestMode();
    extern void runCompilationPipelineTests();   // line 10403
    runCompilationPipelineTests();
    extern void runMegadrivePresetTests();       // line 10405
    runMegadrivePresetTests();
    return 0;
}
if (testVulkanMode) {
    extern int runVulkanRuntimeTests(HINSTANCE instance);  // line 10410
    return runVulkanRuntimeTests(instance);
}
```

| extern Declaration | Activated By | Defined In | Linked Into |
|--------------------|-------------|------------|-------------|
| `void runCompilationPipelineTests()` | `--test` flag | `tests/renderer_vk/test_pipeline.cpp` | Production `Monix.exe` |
| `void runMegadrivePresetTests()` | `--test` flag | Likely in test_pipeline.cpp or separate file | Production `Monix.exe` |
| `int runVulkanRuntimeTests(HINSTANCE)` | `--test-vulkan` flag | `tests/renderer_vk/test_runtime.cpp` | Production `Monix.exe` |

**Issue:** These test functions are compiled into the production binary via `build.ps1` which explicitly lists `test_pipeline.cpp` and `test_runtime.cpp` as source files (lines 7-8 of build.ps1). The test code is **always present** in the shipping executable.

---

## 2. Test Entry Points Table

### 2.1 All test files with `main()` functions

| # | File | Framework | Linked Into | Isolation |
|---|------|-----------|-------------|-----------|
| 1 | `tests/core/eventbus/EventBusTests.cpp` | Custom (`KTestEntry`) | `monix_tests` (CMake) OR standalone | Headless, no UI needed |
| 2 | `tests/core/events/EventCoreTests.cpp` | Custom (`KTestEntry`) | `monix_tests` (CMake) OR standalone | Headless |
| 3 | `tests/core/validation/ValidationTests.cpp` | Custom (`KTestEntry`) | `monix_tests` (CMake) OR standalone | Headless |
| 4 | `tests/core/collectors/CollectorTests.cpp` | Custom (`KTestEntry`) | `monix_tests` (CMake) OR standalone | Headless |
| 5 | `tests/core/collectors/CollectorConfigTests.cpp` | Custom (`KTestEntry`) | `monix_tests` (CMake) OR standalone | Headless |
| 6 | `tests/core/collectors/camera/Phases30_33Tests.cpp` | Custom | `monix_tests` | Headless |
| 7 | `tests/core/collectors/circuit/Phases23_26Tests.cpp` | Custom | `monix_tests` | Headless |
| 8 | `tests/core/collectors/config/ConfigCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 9 | `tests/core/collectors/device/DeviceCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 10 | `tests/core/collectors/driver/DriverCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 11 | `tests/core/collectors/failureinject/Phases45_49Tests.cpp` | Custom | `monix_tests` | Headless |
| 12 | `tests/core/collectors/filesystem/FilesystemTests.cpp` | Custom | `monix_tests` | Headless |
| 13 | `tests/core/collectors/filesystem/FilesystemScopeTests.cpp` | Custom | `monix_tests` | Headless |
| 14 | `tests/core/collectors/health/HealthTests.cpp` | Custom | `monix_tests` | Headless |
| 15 | `tests/core/collectors/network/NetworkCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 16 | `tests/core/collectors/process/ProcessCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 17 | `tests/core/collectors/ruleengine/Phases40_44Tests.cpp` | Custom | `monix_tests` | Headless |
| 18 | `tests/core/collectors/script/ScriptCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 19 | `tests/core/collectors/sensor/SensorCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 20 | `tests/core/collectors/service/ServiceCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 21 | `tests/core/collectors/snapshot/SnapshotEngineTests.cpp` | Custom | `monix_tests` | Headless |
| 22 | `tests/core/collectors/storage/StorageCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 23 | `tests/core/collectors/storage/Phases34_39Tests.cpp` | Custom | `monix_tests` | Headless |
| 24 | `tests/core/collectors/storm/Phases27_29Tests.cpp` | Custom | `monix_tests` | Headless |
| 25 | `tests/core/collectors/usersession/UserSessionCollectorTests.cpp` | Custom | `monix_tests` | Headless |
| 26 | `tests/core/collectors/driver/driver_api_timing.cpp` | Standalone | `monix_tests` | Headless |
| 27 | `tests/core/collectors/driver/driver_timing.cpp` | Standalone | `monix_tests` | Headless |
| 28 | `tests/core/collectors/driver/name_check.cpp` | Standalone | `monix_tests` | Headless |
| 29 | `tests/core/collectors/service/diag_test.cpp` | Standalone | `monix_tests` | Headless |
| 30 | `tests/core/collectors/service/enum_test.cpp` | Standalone | `monix_tests` | Headless |
| 31 | `src/native/tests/MinimalTest.cpp` | Custom printf | Standalone .exe | Headless |
| 32 | `src/native/tests/PipelineTest_unity.cpp` | Custom printf | Standalone .exe | Headless |
| 33 | `Monix/Monix/test_translator.cpp` | Custom printf | Standalone .exe | Headless |
| 34 | `Monix/Monix/test_updater.cpp` | Custom printf | Standalone .exe | Requires network |
| 35 | `tests/renderer_vk_library/shader_library_tests_main.cpp` | Custom | `shader_library_tests.exe` | Headless |
| 36 | `tests/renderer_vk_library/shader_library_compiler_tests_main.cpp` | Custom | `shader_library_compiler_tests.exe` | Headless |
| 37 | `tests/renderer_vk_library/fase16_workspace_tests_main.cpp` | Custom | `fase16_workspace_tests.exe` | Headless |
| 38 | `tests/renderer_vk_library/fase18_workspace_tests_main.cpp` | Custom | `fase18_workspace_tests.exe` | Headless |
| 39 | `tests/renderer_vk_library/fase19_shader_workspace_ui_tests_main.cpp` | Custom | `fase19_shader_workspace_ui_tests.exe` | Headless |
| 40 | `tests/renderer_vk_library/fase20_gdi_shader_ui_tests_main.cpp` | Custom | `fase20_gdi_shader_ui_tests.exe` | Headless |
| 41 | `tests/renderer_vk/shader_runtime_tests_main.cpp` | Custom | `shader_runtime_tests.exe` | Headless |
| 42 | `tests/renderer_vk/production_hardening_tests_main.cpp` | Custom | `production_hardening_tests.exe` | Headless |
| 43 | `tests/renderer_vk/integration_tests_main.cpp` | Custom | `integration_tests.exe` | Headless |
| 44 | `tests/renderer_vk/gpu_validation_tests_main.cpp` | Custom | `gpu_validation_tests.exe` | **Needs GPU + Vulkan** |
| 45 | `tests/renderer_vk/fase13_validation_tests_main.cpp` | Custom | `fase13_validation_tests.exe` | **Needs GPU + Vulkan** |
| 46 | `tests/renderer_vk/external_compat_tests_main.cpp` | Custom | `external_compat_tests.exe` | **Needs GPU + Vulkan** |
| 47 | `tests/renderer_vk/transactional_swap_tests_main.cpp` | Custom | `transactional_swap_tests.exe` | Headless |

---

## 3. Test Executables That Can Be Built

### 3.1 From `build_tests.bat` (repo root) — 9 targets

| # | Executable | Unity File | Main File | Exists | Status |
|---|-----------|------------|-----------|--------|--------|
| 1 | `shader_runtime_tests.exe` | `shader_runtime_test_unity.cpp` | `shader_runtime_tests_main.cpp` | ✅ | Buildable |
| 2 | `production_hardening_tests.exe` | `production_hardening_test_unity.cpp` | `production_hardening_tests_main.cpp` | ✅ | Buildable |
| 3 | `integration_tests.exe` | `integration_test_unity.cpp` ❌ | `integration_tests_main.cpp` | ❌ | **BROKEN** — missing unity file |
| 4 | `shader_library_tests.exe` | `shader_library_test_unity.cpp` | `shader_library_tests_main.cpp` | ✅ | Buildable |
| 5 | `shader_library_compiler_tests.exe` | `shader_library_compiler_test_unity.cpp` | `shader_library_compiler_tests_main.cpp` | ✅ | Buildable |
| 6 | `shader_browser_panel_tests.exe` | `shader_browser_panel_test_unity.cpp` | `shader_browser_panel_tests_main.cpp` | ✅ | Buildable |
| 7 | `gpu_validation_tests.exe` | `gpu_validation_test_unity.cpp` | `gpu_validation_tests_main.cpp` | ✅ | Buildable (needs GPU) |
| 8 | `fase13_validation_tests.exe` | `fase13_validation_test_unity.cpp` | `fase13_validation_tests_main.cpp` | ✅ | Buildable (needs GPU) |
| 9 | `external_compat_tests.exe` | `external_compat_test_unity.cpp` | `external_compat_tests_main.cpp` | ✅ | Buildable (needs GPU) |

### 3.2 From `Monix/Monix/build_tests.bat` — 11 targets (adds 2)

| # | Additional Executable | Unity File | Main File | Status |
|---|----------------------|------------|-----------|--------|
| 10 | `events_tests.exe` | N/A (direct compile) | `events/tests/test_main.cpp` | Referenced in `run_all_tests.bat` but **file not found** at expected path |
| 11 | `fase16_workspace_tests.exe` | `fase16_test_unity.cpp` | `fase16_workspace_tests_main.cpp` | Buildable |

### 3.3 Additional standalone test files (no build script)

| File | Purpose | Notes |
|------|---------|-------|
| `MinimalTest.cpp` | Telemetry normalizer/validator/state tests | Standalone, has own main() |
| `PipelineTest_unity.cpp` | Pipeline component tests (CorrelationEngine, StateStore, etc.) | Standalone, has own main() |
| `test_translator.cpp` | Slang-to-GLSL translator tests | Standalone, has own main() |
| `test_updater.cpp` | Auto-updater HTTP check | Standalone, has own main(), needs network |

### 3.4 CMake `monix_tests` target

Builds all `.cpp` files under `tests/core/` (30 files) as a single executable.
Links only `monix_core` static library — **no Win32 libs linked**, so tests calling Win32 APIs will fail.

### 3.5 run_all_tests.bat expected executables

```batch
events_tests.exe shader_runtime_tests.exe production_hardening_tests.exe
integration_tests.exe shader_library_tests.exe shader_library_compiler_tests.exe
gpu_validation_tests.exe shader_browser_panel_tests.exe fase13_validation_tests.exe
fase16_workspace_tests.exe external_compat_tests.exe
```

---

## 4. Test Isolation Analysis

### 4.1 Tests that can run WITHOUT UI (headless)

| Test | Dependencies | Can Run Headless |
|------|-------------|-----------------|
| All `tests/core/` tests | monix_core only | ✅ Yes |
| `shader_runtime_tests.exe` | renderer_vk (no GPU init) | ✅ Yes |
| `production_hardening_tests.exe` | renderer_vk (no GPU init) | ✅ Yes |
| `shader_library_tests.exe` | ShaderLibrary only | ✅ Yes |
| `shader_library_compiler_tests.exe` | ShaderLibrary + Compiler | ✅ Yes |
| `shader_browser_panel_tests.exe` | ShaderBrowserPanel + stubs | ✅ Yes |
| `fase16_workspace_tests.exe` | ShaderWorkspace | ✅ Yes |
| `fase18_workspace_tests.exe` | ShaderWorkspaceConfig | ✅ Yes |
| `fase19_shader_workspace_ui_tests.exe` | ShaderWorkspace + UI | ✅ Yes |
| `fase20_gdi_shader_ui_tests.exe` | ShaderWorkspace + GDI | ✅ Yes |
| `MinimalTest.cpp` | Normalizer, Validator, StateStore | ✅ Yes |
| `PipelineTest_unity.cpp` | Full pipeline components | ✅ Yes |
| `test_translator.cpp` | Standalone parser | ✅ Yes |
| `test_updater.cpp` | AutoUpdater (HTTP) | ⚠️ Needs network |

### 4.2 Tests that REQUIRE GPU/Vulkan/hardware

| Test | Requirement |
|------|------------|
| `gpu_validation_tests.exe` | GPU + Vulkan driver |
| `fase13_validation_tests.exe` | GPU + Vulkan driver |
| `external_compat_tests.exe` | GPU + Vulkan driver |
| `test_runtime.cpp` (in production exe) | Creates hidden HWND + Vulkan context |
| `integration_tests.exe` | Would need real shader compilation |

### 4.3 Tests that require the full application

| Test | Requirement |
|------|------------|
| `--test` flag on Monix.exe | Full MonixApp initialization (auth, kernel, config) |
| `--test-vulkan` flag on Monix.exe | Full MonixApp + HINSTANCE + Vulkan init |

---

## 5. Unity Build Pattern Analysis

All native tests use a "unity build" pattern where `*_test_unity.cpp` files `#include` the full implementation of the system under test, then link against a separate `*_main.cpp` that calls test functions. This means:

**Each test executable is a self-contained compilation unit** — no shared libraries, no test framework dependencies.

Example: `shader_runtime_test_unity.cpp` includes:
- 25+ `.cpp` files from `renderer_vk/` (core, preset, compiler, graph, runtime, shader_runtime, validation)
- Then `shader_runtime_tests_main.cpp` calls all test functions directly

**Issue:** Unity builds create massive object files (16MB-32MB stack allocated per target). They also mean **every test recompiles the entire dependency chain**, leading to slow builds and potential ODR violations.

---

## 6. Coverage Gaps

### 6.1 Critical areas with NO tests

| Area | Files | Risk |
|------|-------|------|
| **SCRAM engine rules** | `scram/rules/*.cpp` (29 files) | High — rule-based risk analysis with zero unit tests |
| **Login/Auth system** | `login/LoginOverlay.cpp`, `MonixKernel.cpp`, `KernelDisplay.cpp`, `BootUp.cpp`, `BootUpDisplay.cpp` | High — security-critical authentication |
| **Settings system** | `settings/SettingsRegistry.cpp`, `SettingDefs.cpp`, `SettingSerializer.cpp` | Medium — 71 configurable settings, none tested |
| **Logging subsystem** | `logging/LogManager.cpp`, `SoundPlayer.cpp`, `NotificationQueue.cpp`, + 6 more | Medium — logging is core functionality |
| **UI rendering** | `ui/render/*.cpp` (10 files), `ui/*.cpp` | Medium — all GDI drawing code |
| **Telemetry collectors** | 17 collector files (network, power, thermal, audio, GPU, etc.) | High — core data collection, no tests |
| **Crash handler** | `crash/CrashHandler.cpp`, `CrashHandling.cpp` | High — reliability-critical |
| **Process capture** | `telemetry/snapshot/SnapshotPoller.cpp`, `SnapshotConsumer.cpp` | High — data acquisition |
| **Font system** | Font loading, switching, metrics | Low |
| **CRT shader pipeline** | Shader compilation + GL backend | Low (partially covered by renderer tests) |
| **Border rendering** | Border image loading | Low |
| **Notification system** | `NotificationQueue`, toast rendering | Low |
| **Keyboard shortcuts** | Hotkey handling in WndProc | Low |
| **Window management** | DPI scaling, viewport computation, tab switching | Low |

### 6.2 Areas with partial test coverage

| Area | What's Tested | What's Missing |
|------|--------------|----------------|
| Shader runtime | Core runtime, adapters, cache, hot reload | Edge cases, large presets |
| Shader library | Library scanning, compilation | Concurrent compilation, corrupted files |
| Shader compiler | Slang compilation pipeline | Error recovery, timeout handling |
| GPU validation | Pixel analysis, validation profiles | Real GPU output validation |
| Vulkan backend | Stubs only (`vulkan_backend_test_stub.cpp`) | Actual Vulkan API calls |
| Events system | EventFactory, EventBus | Event serialization edge cases |
| Collectors | Individual collector logic | Collector integration, data flow |
| Validation | Schema, structural, semantic, integrity | Cross-field validation |

### 6.3 Missing test framework

There is **no test framework** (no Google Test, no Catch2, no Unity). All tests use a hand-rolled `KTestEntry` pattern with manual `printf("[PASS]")` / `printf("[FAIL]")` output. This means:
- No test discovery
- No automatic test listing
- No structured output (TAP, JUnit XML)
- No CI/CD integration possible without custom parsing

---

## 7. Issues Found

### CRITICAL

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| T1 | **Test code compiled into production binary** — `test_pipeline.cpp` and `test_runtime.cpp` are always compiled into `Monix.exe` by `build.ps1`. Test functions `runCompilationPipelineTests()`, `runMegadrivePresetTests()`, and `runVulkanRuntimeTests()` are linked into shipping executable. | `build.ps1:7-8`, `main.cpp:10401-10411` | Test code bloats production binary, potential attack surface, dead code in shipping builds |
| T2 | **`integration_tests.exe` cannot be built** — `integration_test_unity.cpp` is referenced by `build_tests.bat` but **does not exist** anywhere in the repository. | `build_tests.bat:58` | One of the 9 test targets is permanently broken |
| T3 | **Hardcoded stale paths in all build_tests.bat** — `cd /d D:\Monix-2ago-unestable\Monix\Monix` in both `build_tests.bat` files. Points to wrong directory. | `build_tests.bat:4` | All test builds will fail unless run from correct directory |

### HIGH

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| T4 | **CMake `monix_tests` links no Win32 libs** — only links `monix_core`. Any test using Win32 APIs (ProcessCollector, DeviceCollector, etc.) will fail to link. | `CMakeLists.txt:165` | Core tests that depend on Win32 will fail in CMake build |
| T5 | **`events_tests.exe` target missing** — `run_all_tests.bat` expects it, `Monix/Monix/build_tests.bat` builds it, but the events test main file doesn't exist at the expected path. | `run_all_tests.bat:4` | Test runner will report FAIL for missing executable |
| T6 | **Duplicate `build_tests.bat`** — `repo/build_tests.bat` (209 lines, 9 targets) vs `src/build_tests.bat` (154 lines, 7 targets). Different target sets, same stale paths. | Multiple | Confusion about which script to run |
| T7 | **Vulkan tests require GPU** — `gpu_validation_tests.exe`, `fase13_validation_tests.exe`, `external_compat_tests.exe` require active GPU + Vulkan driver. Cannot run in CI/headless environments. | Various | No automated testing possible in pipelines |

### MEDIUM

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| T8 | **No test framework** — all tests use manual printf-based pass/fail counting. | All test files | No structured output, no CI integration, no test filtering |
| T9 | **Unity build pattern** — each test recompiles 15-25 source files. No shared test library. | `*_test_unity.cpp` | Slow builds, ODR violation risk, massive object files |
| T10 | **`MinimalTest.cpp` and `PipelineTest_unity.cpp` not in any build script** — standalone tests that must be compiled manually. | `Monix/Monix/src/native/tests/` | Tests exist but are not part of any automated build |
| T11 | **No test for SCRAM rules** — 29 rule files with zero test coverage. | `scram/rules/*.cpp` | High-risk analysis engine untested |
| T12 | **No test for login/auth** — 6 source files with zero test coverage. | `login/*.cpp` | Security-critical code untested |
| T13 | **No test for 17 telemetry collectors** — network, power, thermal, audio, GPU, filesystem, registry, security, etc. | `telemetry/collectors/` | Core data pipeline untested |

### LOW

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| T14 | **Test naming inconsistency** — mix of `*Tests.cpp`, `*_main.cpp`, `*_unity.cpp`, `test_*.cpp` | Various | No standard convention |
| T15 | **`test_updater.cpp` not in any build script** — standalone test for auto-updater. | `Monix/Monix/test_updater.cpp` | Not built or run automatically |
| T16 | **`test_translator.cpp` not in any build script** — standalone test for shader translator. | `Monix/Monix/test_translator.cpp` | Not built or run automatically |
| T17 | **Conditional `#ifdef MONIX_KERNEL_BUILD` in core tests** — tests switch between standalone `main()` and library-export mode depending on compile definition. | `tests/core/**/*.cpp` | Confusing dual-mode files |

---

## 8. Recommended Fixes

### Immediate (P0)

1. **Gate test code compilation with `#ifdef MONIX_TEST_BUILD`** in `build.ps1` — don't compile `test_pipeline.cpp` and `test_runtime.cpp` into production builds. Add `#ifdef` guards in `main.cpp` around the `--test` and `--test-vulkan` branches.
2. **Create `integration_test_unity.cpp`** or remove the reference from `build_tests.bat`.
3. **Fix hardcoded paths** in all `build_tests.bat` — use `%~dp0` or relative paths.

### Short-term (P1)

4. **Add Win32 link libraries to CMake `monix_tests` target** — at minimum: `kernel32`, `advapi32`, `psapi`, `user32`.
5. **Add SCRAM rule tests** — 29 rule files need at least basic input/output validation.
6. **Add login/auth tests** — mock-based tests for AuthManager, MonixKernel.
7. **Consolidate `build_tests.bat`** into a single script at repo root.

### Medium-term (P2)

8. **Adopt a test framework** — recommend Google Test or Catch2 for structured output and CI integration.
9. **Add telemetry collector tests** — at minimum for NetworkCollector, PowerCollector, SecurityCollector.
10. **Separate test and production builds** — use CMake presets or build configurations to keep test code out of production.

---

## 9. Build Commands Reference

### Build all tests (repo root)

```cmd
cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo
build_tests.bat
```

**Produces:** 8 test executables in `build/` (integration_tests.exe will fail)

### Build all tests (Monix/Monix)

```cmd
cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo\Monix\Monix
build_tests.bat
```

**Produces:** 10 test executables in `build/`

### Run all tests

```cmd
cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo\Monix\Monix
run_all_tests.bat
```

### Build CMake tests

```cmd
cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-31ago-stable-repo
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --target monix_tests --config Release
```

### Run production test mode

```cmd
Monix.exe --test          # Runs compilation pipeline + Megadrive preset tests
Monix.exe --test-vulkan   # Runs Vulkan runtime tests (needs GPU)
```

---

*End of audit. Generated from filesystem analysis of the repository.*
