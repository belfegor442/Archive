# REPOSITORY-INVENTORY.md

**Repository:** belfegor442/Monix-31ago-stable  
**Date:** 2026-09-14  
**Commit:** `316ea2d` (HEAD)  
**Method:** Full tree scan + build system analysis + source file cross-reference

---

## 1. Árbol Actual del Repositorio

```
Monix-31ago-stable-repo/
├── CMakeLists.txt                  ← Build system (BROKEN — see §5)
├── build_tests.bat                 ← Builds 9 test executables
├── AUDIT-REPORT.md                 ← Previous audit (45 findings, 13 fixed)
├── CHANGELOG-AUDIT.md              ← 12 fixes documented
├── README.md                       ← Brief project overview
├── .gitignore
│
├── src/                            ← REFACTORED tree (partial)
│   ├── core/                       ← 107 .cpp files (events, eventbus, validation, collectors, etc.)
│   ├── native/                     ← 18 .cpp + 4 .h (INCOMPLETE — see §3)
│   │   ├── main.cpp
│   │   ├── vulkan_renderer.cpp/h
│   │   ├── renderer_vk_unity.cpp
│   │   ├── TelemetryInternal.hpp
│   │   ├── render_backend.h, runtime_state.h
│   │   ├── config/
│   │   ├── core/ (TextUtils)
│   │   ├── GL/ (glext.h)
│   │   ├── KHR/ (khrplatform.h)
│   │   ├── kernel/ (KernelDiagnostics, KernelSelfTest)
│   │   ├── logging/ (LogEntry.hpp ONLY)
│   │   ├── renderer_vk/ (full Vulkan backend — DUPLICATED)
│   │   ├── settings/ (SettingsRegistry + MonixConfigTypes)
│   │   ├── telemetry/ (Collectors.cpp + Snapshot.hpp ONLY)
│   │   ├── transitions/ (4 .cpp files)
│   │   └── ui/ (AppState.hpp, CoreMonitorTheme.hpp ONLY)
│   └── build_tests.bat
│
├── Monix/Monix/                    ← MONOLITHIC application (THE REAL BUILD)
│   ├── build.ps1                   ← AUTHORITATIVE build system (469 lines)
│   ├── build_run.bat               ← Wrapper that calls build.ps1
│   ├── build_tests.bat             ← Builds 10 test executables
│   ├── publish.ps1                 ← GitHub release script
│   ├── monix.ini                   ← Runtime config
│   ├── src/native/                 ← FULL application source (200+ .cpp)
│   │   ├── main.cpp, MonixApp.cpp, AppWindowProc.cpp, AppRenderer.cpp
│   │   ├── TelemetryThread.cpp
│   │   ├── renderer_vk/            ← DUPLICATE of src/native/renderer_vk/
│   │   ├── crash/, logging/, ui/, scram/, settings/, updater/
│   │   ├── telemetry/ (full collector tree)
│   │   ├── shader/, platform/, app/, core/
│   │   └── tests/ (test stubs + unity files)
│   ├── login/ (kernel boot + auth)
│   ├── sensors/ (hardware.c + cpu.asm)
│   ├── Shaders/ (GLSL, Slang presets, tests)
│   ├── themes/
│   ├── installer/ (install.bat + uninstall.bat)
│   ├── tests/ (SCRAM tests + shader test data)
│   ├── docs/
│   └── cache_test/
│
├── original/                       ← PRE-REFACTOR originals (16 files)
│   ├── main_original.cpp
│   ├── login/ (BootUp, KernelDisplay, etc.)
│   ├── settings/, shader_panel/, transitions/, types/
│
├── tests/                          ← Core unit tests (31 files)
│   └── core/ (collectors, eventbus, events, validation)
│
├── docs/                           ← Documentation
│
└── Monix-2ago-unestable/           ← FULL REPOSITORY SNAPSHOT (482 .cpp)
    ├── CMakeLists.txt              ← Older copy (different RC path)
    ├── src/, Monix/, original/, tests/
    └── [NOT referenced by any build system]
```

---

## 2. Clasificación de Componentes

### `src/` — Refactored Tree

| Subárbol | Estado | Notas |
|----------|--------|-------|
| `src/core/` (107 .cpp) | **ACTIVE** | Compilable como `monix_core` estática. Events, eventbus, validation, collectors, security, platform |
| `src/native/main.cpp` | **ACTIVE** | Entry point usado por CMakeLists.txt |
| `src/native/vulkan_renderer.cpp/h` | **ACTIVE** | Vulkan renderer principal (referenciado por CMake) |
| `src/native/renderer_vk/` | **DUPLICATED** | Copia idéntica de `Monix/Monix/src/native/renderer_vk/` |
| `src/native/kernel/` | **ACTIVE** | KernelDiagnostics + KernelSelfTest (compilados por CMake) |
| `src/native/transitions/` | **ACTIVE** | 4 archivos .cpp (compilados por CMake, NO por build.ps1) |
| `src/native/settings/` | **ACTIVE** | SettingsRegistry (compilado por CMake) |
| `src/native/telemetry/` | **STUB** | Solo Collectors.cpp (1 línea `#include`) + Snapshot.hpp |
| `src/native/logging/` | **STUB** | Solo LogEntry.hpp |
| `src/native/ui/` | **STUB** | Solo AppState.hpp + CoreMonitorTheme.hpp |
| `src/native/config/` | **STUB** | Solo MonixConfigTypes.hpp |
| `src/native/core/` | **STUB** | Solo TextUtils.cpp/hpp + Types.hpp |
| `src/native/*.cpp` (test unity) | **DEAD** | 12 test unity files compilados por .bat, no por CMake |
| `src/native/runtime_state.h` | **LEGACY** | Header sin referencia en build |
| `src/native/render_backend.h` | **LEGACY** | Header sin referencia en build |

### `Monix/Monix/` — Monolithic Tree

| Subárbol | Estado | Notas |
|----------|--------|-------|
| `Monix/Monix/src/native/` | **ACTIVE** | Build.ps1 compila ~80+ archivos de aquí |
| `Monix/Monix/login/` | **ACTIVE** | Kernel boot + auth (compilado por build.ps1) |
| `Monix/Monix/sensors/` | **ACTIVE** | hardware.c + cpu.asm |
| `Monix/Monix/Shaders/` | **ACTIVE** | GLSL + Slang presets + tests |
| `Monix/Monix/renderer_vk/` | **DUPLICATED** | Espejo de `src/native/renderer_vk/` |
| `Monix/Monix/tests/` | **ACTIVE** | SCRAM tests + shader test data |
| `Monix/Monix/installer/` | **ACTIVE** | install.bat + uninstall.bat |
| `Monix/Monix/docs/` | **ACTIVE** | Documentation |
| `Monix/Monix/themes/` | **ACTIVE** | Theme files |
| `Monix/Monix/0a6d0857b27dea91.*` | **DEAD** | Archivos sueltos de testing |
| `Monix/Monix/MEGA_BEZEL_*` | **DEAD** | Reportes de fases anteriores |
| `Monix/Monix/pass_captures/` | **DEAD** | Datos de captura |
| `Monix/Monix/PHASE4_VISUAL_REPORT.txt` | **DEAD** | Reporte histórico |

### `original/` — Legacy

| Componente | Estado | Notas |
|-----------|--------|-------|
| `original/main_original.cpp` | **LEGACY** | Original pre-refactor |
| `original/login/` | **LEGACY** | Login originals |
| `original/settings/` | **LEGACY** | Settings originales |
| `original/shader_panel/` | **LEGACY** | ShaderBrowserPanel original |
| `original/transitions/` | **LEGACY** | Transitions originales |
| `original/types/` | **LEGACY** | TextUtils original |

### `tests/` — Core Tests

| Componente | Estado | Notas |
|-----------|--------|-------|
| `tests/core/collectors/` (15 files) | **ACTIVE** | Unit tests para collectors |
| `tests/core/eventbus/` (1 file) | **ACTIVE** | EventBus tests |
| `tests/core/events/` (2 files) | **ACTIVE** | Event system tests |
| `tests/core/validation/` (1 file) | **ACTIVE** | Validation tests |

### `Monix-2ago-unestable/` — Archive

| Componente | Estado | Notas |
|-----------|--------|-------|
| Todo el árbol (482 .cpp) | **ARCHIVE** | Snapshot completo del repo en fecha anterior. NO referenciado por ningún build |

---

## 3. Fuentes No Compiladas (src/native/)

Los siguientes archivos/directorios existen en `src/native/` pero están **referenciados por CMakeLists.txt** bajo rutas que **no existen**:

| Referencia CMake | Ruta Esperada | Estado |
|-----------------|---------------|--------|
| `${MONIX_NATIVE}/Draw.cpp` | `src/native/Draw.cpp` | **MISSING** |
| `${MONIX_NATIVE}/Telemetry.cpp` | `src/native/Telemetry.cpp` | **MISSING** |
| `${MONIX_NATIVE}/Input.cpp` | `src/native/Input.cpp` | **MISSING** |
| `${MONIX_NATIVE}/resources.rc` | `src/native/resources.rc` | **MISSING** (real: `Monix/Monix/src/native/resources.rc`) |
| `src/native/app/` | Directorio completo | **MISSING** |
| `src/native/platform/` | Directorio completo | **MISSING** |
| `src/native/crash/` | Directorio completo | **MISSING** |
| `src/native/scram/` | Directorio completo | **MISSING** |
| `src/native/updater/` | Directorio completo | **MISSING** |

Estos archivos/directorios existen solamente en `Monix/Monix/src/native/`.

---

## 4. Fuentes Compiladas

### build.ps1 (AUTORITATIVO — genera Monix.exe)

Compila **~80+ archivos .cpp** de `Monix/Monix/src/native/`, organizados en:

| Categoría | Archivos | Origen |
|-----------|----------|--------|
| Core (main, app, UI) | 8 | `src/native/*.cpp` |
| Vulkan renderer | ~30+ | `src/native/renderer_vk/**/*.cpp` |
| Settings | 3 | `src/native/settings/` |
| Core (TextUtils) | 1 | `src/native/core/` |
| Telemetry (collectors) | 14 | `src/native/telemetry/collectors/` |
| Telemetry (snapshot) | 4 | `src/native/telemetry/snapshot/` |
| Telemetry (state) | 4 | `src/native/telemetry/state/` |
| Telemetry (runtime) | 1 | `src/native/telemetry/runtime/TelemetryThread.cpp` |
| Telemetry (other) | 3 | ChangeDetector, EnvironmentCollector, etc. |
| SCRAM | 1 + 25 rules | `src/native/scram/` |
| Logging | 6 | `src/native/logging/` |
| Login (kernel) | 6 | `login/` |
| Crash | 2 | `src/native/crash/` |
| Updater | 1 | `src/native/updater/` |
| Sensors | 2 | `sensors/hardware.c` + `sensors/cpu.asm` |
| OpenGL | 1 | `src/native/renderer_vk/opengl/GlBackend.cpp` |
| Shader (GLSL parser) | 1 | `src/native/shader/` |
| Platform | 1 | `src/native/platform/win32/WindowFactory.cpp` |
| App lifecycle | 2 | `src/native/app/lifecycle/` |
| App bootstrap | 1 | `src/native/app/bootstrap/CliParser.cpp` |

### CMakeLists.txt (ROTO — genera targets parciales)

Compila **~30 archivos** de:
- `src/core/**/*.cpp` → `monix_core` static library (~107 files)
- `src/native/` (only files that exist: main.cpp, vulkan_renderer.cpp, kernel/*.cpp)
- `Monix/Monix/login/` (6 files)
- `Monix/Monix/sensors/` (2 files)
- `Monix/Monix/src/native/scram/` + `scram/rules/` (26 files)
- `Monix/Monix/src/native/logging/` (3 files by name)
- `Monix/Monix/src/native/renderer_vk/opengl/GlBackend.cpp`
- `Monix/Monix/src/native/settings/SettingsRegistry.cpp`
- `Monix/Monix/src/native/core/TextUtils.cpp`
- `Monix/Monix/src/native/telemetry/Collectors.cpp`
- `src/native/transitions/` (4 files)
- `tests/core/**/*.cpp` → `monix_tests`

### build_tests.bat (REPO ROOT — genera 9 test executables)

Compila test unity files + test mains para:
1. shader_runtime_tests.exe
2. production_hardening_tests.exe
3. integration_tests.exe
4. shader_library_tests.exe
5. shader_library_compiler_tests.exe
6. shader_browser_panel_tests.exe
7. gpu_validation_tests.exe
8. fase13_validation_tests.exe
9. external_compat_tests.exe

---

## 5. Dependencias Cruzadas

```
CMakeLists.txt
  ├── usa GLOB_RECURSE sobre src/core/           → monix_core (STATIC)
  ├── referencia src/native/Draw.cpp              → NO EXISTE
  ├── referencia src/native/Telemetry.cpp         → NO EXISTE
  ├── referencia src/native/Input.cpp             → NO EXISTE
  ├── referencia src/native/resources.rc          → NO EXISTE (real: Monix/)
  ├── referencia src/native/transitions/*.cpp     → EXISTE (4 archivos)
  ├── referencia Monix/Monix/login/*.cpp          → EXISTE
  ├── referencia Monix/Monix/sensors/*            → EXISTE
  ├── referencia Monix/Monix/src/native/scram/    → EXISTE
  ├── referencia Monix/Monix/src/native/logging/  → EXISTE (parcial)
  └── NO referencia:
      ├── Monix/Monix/src/native/MonixApp.cpp
      ├── Monix/Monix/src/native/AppWindowProc.cpp
      ├── Monix/Monix/src/native/AppRenderer.cpp
      ├── Monix/Monix/src/native/telemetry/ (14 collectors)
      ├── Monix/Monix/src/native/telemetry/snapshot/ (4 files)
      ├── Monix/Monix/src/native/telemetry/state/ (4 files)
      ├── Monix/Monix/src/native/ui/ (FontManager + render)
      ├── Monix/Monix/src/native/settings/ (SettingHelpers, SettingDefs, SettingSerializer)
      ├── Monix/Monix/src/native/updater/AutoUpdater.cpp
      ├── Monix/Monix/src/native/crash/ (2 files)
      ├── Monix/Monix/src/native/renderer_vk/ (25+ VK files)
      ├── Monix/Monix/src/native/app/ (bootstrap + lifecycle)
      ├── Monix/Monix/src/native/platform/ (WindowFactory)
      └── Monix/Monix/src/native/shader/ (GlslpPresetParser)

build.ps1 (AUTORITATIVO)
  ├── compila ~80+ archivos de Monix/Monix/src/native/
  ├── compila Monix/Monix/login/ (6 files)
  ├── compila sensors/ (hardware.c + cpu.asm)
  ├── compila src/core/**/*.cpp via wildcard
  ├── NO compila src/native/ (excepto src/native/main.cpp references)
  └── NO compila src/native/transitions/

src/native/renderer_vk/    ← DUPLICADO
Monix/Monix/src/native/renderer_vk/  ← DUPLICADO (copia idéntica)
```

---

## 6. Riesgos Encontrados

### CRÍTICOS

| # | Riesgo | Impacto |
|---|--------|---------|
| C1 | **CMakeLists.txt no puede compilar el ejecutable completo.** Falta ~50 archivos que solo existen en `Monix/Monix/` | El target `monix` de CMake no linkagea — errores de linker masivos |
| C2 | **build.ps1 apunta a `D:\Monix-10sept-stable\Monix-31ago-stable\Monix-2ago-unestable\` en build_run.bat** | La ruta está hardcoded al directorio del build anterior, no al repo clonado |
| C3 | **Renderer_vk DUPLICADO entre `src/native/renderer_vk/` y `Monix/Monix/src/native/renderer_vk/`** | Cambios en uno no se reflejan en otro; el build usa la copia de Monix/ |
| C4 | **CMakeLists.txt referencia archivos fantasma** (`Draw.cpp`, `Telemetry.cpp`, `Input.cpp`) — aún después del FIX-11/12 del audit anterior | La auditoría anterior dice que los removió, pero siguen listados en el HEAD actual |

### ALTOS

| # | Riesgo | Impacto |
|---|--------|---------|
| A1 | **`/w` (suppress ALL warnings)** en los 3 targets de CMake + build.ps1 | No se detectan bugs en compile-time |
| A2 | **`GLOB_RECURSE` para sources** en CMake | Agregar/quitar archivos no se detecta sin reconfigure |
| A3 | **`monix_tests` no tiene framework de tests** ni `ctest` integration | No hay forma de ejecutar tests desde CMake |
| A4 | **KernelSelfTest referencia símbolos de test** en el target `monix` (no `monix_tests`) | 153 errores de linker si se compila completo |
| A5 | **~90 function pointers Vulkan son `static`** en vulkan_renderer.cpp | No visibles desde renderer_vk_unity.cpp — 8 errores de linker |
| A6 | **`src/core/` tiene 107 archivos .cpp** pero `monix_tests` solo linkea `monix_core` sin Win32 libs | Tests no pueden probar código que depende de Win32 |

### MEDIOS

| # | Riesgo | Impacto |
|---|--------|---------|
| M1 | **`Monix-2ago-unestable/` (482 .cpp) es un snapshot completo no archivado** | Confusión, duplicación de espacio, unclear si es backup o alternativa |
| M2 | **`original/` (16 files) sin referencia** | No se sabe si son alternativas o legacy |
| M3 | **Test unity files en `src/native/`** (12 archivos `*_test_unity.cpp`) mezclados con source principal | No separados de la aplicación |
| M4 | **`Monix/Monix/build_tests.bat`** referencia rutas relativas que asumen `cd` al directorio correcto | Frágil, puede fallar desde otras ubicaciones |
| M5 | **No hay `.github/workflows/`** — sin CI/CD | Sin validación automática |
| M6 | **`/Zm800` inconsistente** — en monix pero no en monix_core | Posible problema con headers grandes |
| M7 | **No hay `CMAKE_BUILD_TYPE` default** | En single-config generators, sin optimización por defecto |
| M8 | **No hay `install()` rules** | No se puede instalar vía CMake |

### BAJO

| # | Riesgo | Impacto |
|---|--------|---------|
| L1 | **`/D` flags redundantes** en `target_compile_options` para monix_core | Code smell, no funcional |
| L2 | **ASM_MASM habilitado dos veces** en CMake | Redundante |
| L3 | **Archivos sueltos en Monix/Monix/** (0a6d0857b27dea91.*, MEGA_BEZEL_*, PHASE4_*) | Clutter |
| L4 | **README.md mínimo** | No documenta setup, build, o arquitectura |
| L5 | **Sin `set(CMAKE_BUILD_TYPE)` default** | En single-config generators, sin optimización por defecto |
| L6 | **winhttp.lib linkado por build.ps1 pero no por CMake** | Diferencia entre sistemas de build |
| L7 | **wbemuuid + shcore linkados por CMake pero no por build.ps1** | Diferencia entre sistemas de build |

---

## 7. Recomendación de Reorganización

### Árbol Oficial: `Monix/Monix/src/native/`

**`build.ps1`** es el sistema de build autoritativo. El ejecutable se genera desde `Monix/Monix/`. CMakeLists.txt es un intento de reestructuración que no se completó.

### Prioridad de acción:

1. **Unificar el árbol de fuentes**: Mover todo el código de `Monix/Monix/src/native/` a `src/native/` (la ubicación que CMake espera)
2. **O eliminar `src/` y reescribir CMakeLists.txt** para apuntar a `Monix/Monix/`
3. **Resolver la duplicación de renderer_vk/**: Decidir cuál es la fuente y eliminar la otra
4. **Archivar `Monix-2ago-unestable/`** al repo `Archive`
5. **Archivar `original/`** al repo `Archive`
6. **Separar test unity files** del source principal
7. **Corregir build_run.bat** para apuntar al directorio correcto

### Mapa de dependencias real (solo build.ps1):

```
build.ps1
├── Monix/Monix/src/native/*.cpp (main, MonixApp, AppWindowProc, ...)
├── Monix/Monix/src/native/app/**/*.cpp
├── Monix/Monix/src/native/core/TextUtils.cpp
├── Monix/Monix/src/native/crash/*.cpp
├── Monix/Monix/src/native/logging/**/*.cpp
├── Monix/Monix/src/native/renderer_vk/**/*.cpp
├── Monix/Monix/src/native/scram/**/*.cpp
├── Monix/Monix/src/native/settings/**/*.cpp
├── Monix/Monix/src/native/shader/**/*.cpp
├── Monix/Monix/src/native/telemetry/**/*.cpp
├── Monix/Monix/src/native/updater/**/*.cpp
├── Monix/Monix/src/native/ui/**/*.cpp
├── Monix/Monix/src/native/platform/**/*.cpp
├── Monix/Monix/login/*.cpp
├── Monix/Monix/sensors/hardware.c
├── Monix/Monix/sensors/cpu.asm
├── src/core/**/*.cpp (via wildcard)
├── tests/core/**/*Tests.cpp (via wildcard)
└── librerías Win32 (23 libs)
```
