# BUILD-AUDIT.md — Monix Build System

**Date:** 2025-09-14
**Commit:** `44e9941` (after Phase 2 fixes)
**Build system:** CMake 3.20+ / MSVC 19.44 / Windows SDK 10.0.26100.0

---

## 1. Build Configuration

### Configure Command
```powershell
cd Monix-2ago-unestable
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### Build Command
```powershell
cmake --build build --config Release --target monix
```

### Compiler Settings
| Setting | Value |
|---------|-------|
| C++ Standard | C++20 (`/std:c++20`) |
| CRT | Static (`/MT`) |
| Warnings | `/W4 /permissive-` |
| Encoding | `/utf-8` |
| Other | `/Zm800 /bigobj /EHsc` |
| ASM | MASM x64 (`cpu.asm`) |

---

## 2. Targets

| Target | Type | Source Files | Status |
|--------|------|-------------|--------|
| `monix_core` | STATIC lib | 101 .cpp (core library) | ✅ Compiles, 0 errors |
| `monix` | WIN32 exe | ~152 sources (app + VK + SCRAM + login + transitions + sensors) | ✅ Compiles, links, 0 errors |
| 30 test executables | Console exe | 1 .cpp each (standalone tests) | ⚠️ Pre-existing API mismatches |

---

## 3. Build Results

### monix target (production executable)
```
0 compilation errors
113 warnings (/W4)
0 linker errors
→ build/Release/monix.exe (2.0 MB)
```

### Warning Breakdown (/W4)
| Code | Count | Description |
|------|-------|-------------|
| C4100 | 46 | Unreferenced formal parameter |
| C4005 | 29 | Macro redefinition (`WIN32_LEAN_AND_MEAN`) |
| C4189 | 14 | Local variable initialized but not referenced |
| C4505 | 7 | Unreferenced local function removed |
| C4244 | 5 | Conversion loss of data (int→char) |
| C4267 | 4 | Conversion loss of data (size_t→uint32_t) |
| C4477 | 3 | Format string mismatches |
| C4018 | 2 | Signed/unsigned mismatch |
| C4456 | 2 | Declaration hides previous local |
| C4702 | 1 | Unreachable code |

### monix_tests (test executables)
```
26 compilation errors (pre-existing API mismatches in test code)
```
Tests use old API signatures (e.g., `config.set("key", value)` instead of `config.set(Field::Key, value)`). These are pre-existing issues in test code, not caused by build system changes.

---

## 4. Issues Found and Fixed

### FIX-B01: Missing include path for RetentionPolicy.hpp
- **File:** `src/core/collectors/retention/RetentionPolicy.hpp:7`
- **Before:** `#include "EventStorage.hpp"` — file is at `storage/EventStorage.hpp`
- **After:** `#include "../storage/EventStorage.hpp"`
- **Status:** ✅ Fixed

### FIX-B02: Test target missing collector subdirectory includes
- **File:** `CMakeLists.txt` (test target)
- **Before:** Only 10 include directories for monix_tests
- **After:** Added all 25+ collector subdirectory includes
- **Status:** ✅ Fixed

### FIX-B03: Test target compiled all tests into one binary (multiple main())
- **File:** `CMakeLists.txt` (test target)
- **Before:** Single `monix_tests` executable linking all 30 test .cpp → 5 linker errors (multiple `main()`)
- **After:** Each test .cpp becomes a separate executable via `foreach()`
- **Status:** ✅ Fixed

### FIX-B04: Test target defined MONIX_KERNEL_BUILD (hid main())
- **File:** `CMakeLists.txt` (test target)
- **Before:** `target_compile_definitions` included `MONIX_KERNEL_BUILD`
- **After:** Removed `MONIX_KERNEL_BUILD` (tests use `#ifndef MONIX_KERNEL_BUILD` to gate their `main()`)
- **Status:** ✅ Fixed

### FIX-B05: Warning level `/w` suppressed all warnings
- **File:** `CMakeLists.txt`
- **Before:** `/w` (suppress all warnings)
- **After:** `/W4 /permissive-` (high warning level + strict conformance)
- **Status:** ✅ Fixed

---

## 5. Build System Comparison

| Aspect | build.ps1 (Legacy) | CMakeLists.txt (Active) |
|--------|-------------------|------------------------|
| Source files | All explicit (cl.exe direct) | All explicit (no GLOB for production) |
| renderer_vk/ | All ~90 files compiled | Only 17 VK decomposition + GlBackend |
| Icon stamping | rcedit.exe post-build | Not implemented |
| Font copying | build.ps1 copies TTFs | Not implemented |
| `/W4` | Not set (uses `/w`-equivalent) | `/W4 /permissive-` (after fix) |
| `/MT` | Yes | Yes |
| C++20 | Yes | Yes |

---

## 6. Reproducibility

The build is reproducible from a clean `build/` directory:
```powershell
cd Monix-2ago-unestable
Remove-Item -Recurse -Force build
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target monix
```

**Dependencies:** Visual Studio 2022 Build Tools + Windows SDK 10.0.26100.0 + MASM x64

---

## 7. Remaining Warnings (Categorized by Priority)

### High Priority (fix in Phase 3+)
| Code | File | Issue |
|------|------|-------|
| C4244 | StorageCollector.cpp:276,320 | int→char truncation in collector logic |
| C4267 | MonixKernel.hpp:61 | size_t→uint32_t in login subsystem |
| C4477 | main.cpp (3 locations) | Format string mismatches |

### Medium Priority (fix in Phase 4+)
| Code | File | Issue |
|------|------|-------|
| C4100 | 46 locations | Unreferenced parameters (stub implementations) |
| C4189 | main.cpp (7 locations) | Unused local variables |
| C4456 | main.cpp (2 locations) | Variable shadowing |

### Low Priority (informational)
| Code | File | Issue |
|------|------|-------|
| C4005 | main.cpp | WIN32_LEAN_AND_MEAN macro redefinition (benign) |
| C4505 | FilesystemScope/Watcher | Unreferenced static functions (ODR-safe) |
| C4702 | ValidationTypes.cpp | Unreachable code |
| C4018 | main.cpp (2) | Signed/unsigned comparison |
