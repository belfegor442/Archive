# CURRENT-FINDINGS.md

**Repository:** belfegor442/Monix-31ago-stable  
**Commit:** `316ea2d` (HEAD)  
**Date:** 2026-09-14  
**Method:** Direct code analysis — not relying on previous AUDIT-REPORT.md

---

## CRITICAL

### CF-001: `initialize()` leaks all resources on any failure
- **File:** `src/native/vulkan_renderer.cpp:223-317`
- **Evidencia:** `initialized_ = true` solo se ejecuta en línea 314 (el último paso). Los 12 paths de error antes de esa línea retornan `false` sin limpiar nada. El destructor llama `shutdown()` pero `shutdown()` tiene `if (!initialized_) return;` en línea 323.
- **Impacto:** Cualquier fallo en `vkCreateInstance`, `createSurface`, `pickPhysicalDevice`, `createLogicalDevice`, `createSwapchain`, `createSyncObjects`, `createCommandPool`, `allocateCommandBuffers`, `createDescriptorPool`, o `createQuadBuffer` deja todos los recursos previamente creados permanentemente leakados (DLL, instance, surface, device, swapchain, semaphores, fences, command pool, descriptor pool, buffers).
- **Corrección:** Reemplazar `if (!initialized_) return;` con flags individuales por recurso, o usar cleanup incremental. Cada step de `initialize()` debe registrar qué creó para que `shutdown()` lo libere.
- **Estado:** OPEN

### CF-002: `FreeLibrary(g_vkModule)` nunca se ejecuta
- **File:** `src/native/vulkan_renderer.cpp:229`
- **Evidencia:** `LoadLibraryA("vulkan-1.dll")` en línea 229. No existe ningún `FreeLibrary()` en todo el archivo (ni en `initialize()`, ni en `shutdown()`, ni en el destructor).
- **Impacto:** La DLL de Vulkan permanece cargada en el proceso indefinidamente. Los punteros `PFN_vk*` son `static` y nunca se resetean a `nullptr`, así que después de un shutdown parcial, el código puede llamar a funciones a través de punteros que apuntan a memoria de la DLL potencialmente inválida.
- **Corrección:** Agregar `FreeLibrary(vulkanDll_)` al final de `shutdown()`, después de destruir instance y device. Resetear todos los punteros a `nullptr`.
- **Estado:** OPEN

### CF-003: `validationErrorCount()`, `validationWarningCount()`, `validationCriticalCount()` son data races
- **File:** `src/native/vulkan_renderer.cpp:196-217`
- **Evidencia:** Estos 3 métodos recorren `validationMessages_` sin adquirir `validationMutex_`. Mientras tanto, `addValidationMessage()` (línea 172-175) sí adquiere el mutex para escribir. Si el callback de Vulkan se ejecuta desde el driver thread mientras la UI lee los contadores, es undefined behavior.
- **Impacto:** Crash o corrupción de datos al leer el vector mientras se modifica. Ocurre bajo carga de validación con múltiples threads.
- **Corrección:** Agregar `std::lock_guard<std::mutex> lock(validationMutex_);` a los 3 métodos de conteo.
- **Estado:** OPEN

### CF-004: `src/native/main.cpp` contiene un MonixApp de 10,423 líneas (god object legacy)
- **File:** `src/native/main.cpp:709-1112` (clase), total 10,423 líneas
- **Evidencia:** La clase MonixApp está definida completamente inline en este archivo con ~250+ miembros y ~100+ métodos. Contiene: Win32 window, Vulkan, OpenGL, telemetry, processes, network, WMI, PDH, logging, sound, notifications, SCRAM, config, shaders, tests, kernel, auth. La versión en `Monix/Monix/src/native/` está correctamente dividida en MonixApp.hpp (322 líneas), MonixApp.cpp (281), AppWindowProc.cpp (1835), AppRenderer.cpp (12), TelemetryThread.cpp (35).
- **Impacto:** CMakeLists.txt apunta a `src/native/` (vía `MONIX_NATIVE`), pero este árbol tiene Draw.cpp, Telemetry.cpp, Input.cpp como archivos FANTASMA (no existen). El build real usa `Monix/Monix/` via build.ps1.
- **Corrección:** Decidir: (A) Mover todo de `Monix/Monix/` a `src/native/` y reescribir CMakeLists.txt, o (B) Eliminar `src/native/main.cpp` legacy y reescribir CMakeLists.txt para apuntar a `Monix/Monix/`.
- **Estado:** OPEN

---

## HIGH

### CF-005: 16 VkResult return values no verificados
- **File:** `src/native/vulkan_renderer.cpp`
- **Evidencia:**
  - `vkResetFences` (línea 888) — ignorado
  - `vkBeginCommandBuffer` (línea 909) — ignorado
  - `vkEndCommandBuffer` (línea 939) — ignorado
  - `vkQueueSubmit` (línea 959) — ignorado
  - `vkQueuePresentKHR` (línea 969) — ignorado
  - `vkBindImageMemory` (línea 1013) — ignorado
  - `vkAllocateCommandBuffers` (línea 1028) — ignorado
  - `vkBeginCommandBuffer` one-shot (línea 1033) — ignorado
  - `vkEndCommandBuffer` one-shot (línea 1085) — ignorado
  - `vkQueueSubmit` one-shot (línea 1092) — ignorado
  - `vkBindBufferMemory` (línea 1262) — ignorado
  - `vkMapMemory` (línea 1267) — ignorado
  - `vkCreateDescriptorSetLayout` (línea 1305) — ignorado
  - `vkAllocateDescriptorSets` (línea 1327) — ignorado
  - `vkCreatePipelineLayout` (línea 1390) — ignorado
- **Impacto:** La aplicación puede continuar con handles inválidos, causando crashes posteriores o corrupción silenciosa de datos. Especialmente peligroso: `vkBindBufferMemory` fallido deja un buffer sin memoria mapeada → `memcpy` a puntero basura.
- **Corrección:** Cada VkResult debe ser comprobado. Fatal errors deben retornar error. Recoverable errors deben loguear y ejecutar rollback.
- **Estado:** OPEN

### CF-006: `ScramEngine::Evaluate()` no es thread-safe
- **File:** `Monix/Monix/src/native/scram/ScramEngine.cpp:20-130`
- **Evidencia:** No hay mutex, no hay atomic. `Evaluate()` muta `totalEvaluations_`, `phase_`, `calibrationSamples_`, y el mapa `findingState_` sin sincronización.
- **Impacto:** Si `Evaluate()` se llama desde múltiples threads (o si el UI lee el riskScore mientras la telemetría evalúa), los datos son corruptos.
- **Corrección:** Agregar `std::mutex` o `std::shared_mutex` a ScramEngine.
- **Estado:** OPEN (pre-existente, no corregido por auditoría anterior)

### CF-007: ~83 punteros globales `static PFN_vk*` — riesgo de uso post-FreeLibrary
- **File:** `src/native/vulkan_renderer.cpp:22-114`
- **Evidencia:** 83 punteros de función Vulkan declarados como `static` a nivel de archivo. Si la DLL se libera pero los punteros no se resetean, cualquier llamada posterior a través de estos punteros es UB.
- **Impacto:** Si se implementa FreeLibrary sin resetear punteros, o si se intenta una segunda inicialización después de un fallo, los punteros apuntan a memoria inválida.
- **Corrección:** Mover punteros a `VulkanRenderer` como miembros, o al menos resetearlos a `nullptr` en `shutdown()`.
- **Estado:** OPEN

### CF-008: CMakeLists.txt no compila la aplicación completa
- **File:** `CMakeLists.txt:59-110`
- **Evidencia:** Referencia `Draw.cpp`, `Telemetry.cpp`, `Input.cpp` que no existen. Falta: MonixApp.cpp, AppWindowProc.cpp, todos los collectors (14 archivos), todos los snapshot files, todos los state files, todo el UI rendering, toda la logging, crash handling, updater, settings helpers, shaders.
- **Impacto:** El target `monix` de CMake produce errores de linker masivos (~161+ símbolos sin resolver). Solo `monix_core` compila limpio.
- **Corrección:** Reescribir CMakeLists.txt para reflejar la estructura real, o unificar los árboles de código.
- **Estado:** OPEN

### CF-009: `build_run.bat` apunta a directorio hardcoded equivocado
- **File:** `Monix/Monix/build_run.bat:4`
- **Evidencia:** `cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-2ago-unestable\Monix\Monix` — apunta al snapshot `Monix-2ago-unestable` en vez del repo clonado.
- **Impacto:** El buildRun no funciona desde el repo clonado. Hay que editar la ruta manualmente cada vez.
- **Corrección:** Usar `%~dp0` o ruta relativa al script.
- **Estado:** OPEN

### CF-010: `renderer_vk/` DUPLICADO en dos ubicaciones
- **File:** `src/native/renderer_vk/` vs `Monix/Monix/src/native/renderer_vk/`
- **Evidencia:** Ambos contienen ~50 archivos .cpp/.hpp idénticos (VkBuffer, VkDevice, VkInstance, ShaderCache, ShaderRuntime, etc.).
- **Impacto:** Cambios en uno no se reflejan en el otro. El build usa la de `Monix/Monix/`. La de `src/native/` es código muerto que puede causar confusión.
- **Corrección:** Decidir cuál es la fuente oficial y eliminar la otra.
- **Estado:** OPEN

---

## MEDIUM

### CF-011: `/w` (suppress ALL warnings) en todos los targets
- **Files:** `CMakeLists.txt` (monix, monix_core, monix_tests), `Monix/Monix/build.ps1`
- **Evidencia:** Los 3 targets de CMake y build.ps1 usan `/w` que suprime todos los warnings.
- **Impacto:** No se detectan bugs en compile-time. Problemas como truncation, signed/unsigned mismatch, unused variables, etc. pasan silenciosamente.
- **Corrección:** Reemplazar `/w` con `/W4 /permissive-`. No activar `/WX` hasta limpiar warnings relevantes.
- **Estado:** OPEN

### CF-012: `WndProc` de 974 líneas en AppWindowProc.cpp
- **File:** `Monix/Monix/src/native/AppWindowProc.cpp:861-1834`
- **Evidencia:** Un solo `switch(message)` con manejo de 15+ mensajes, incluyendo WM_LBUTTONDOWN (356 líneas) y WM_KEYDOWN (367 líneas).
- **Impacto:** Difícil de mantener, testear, y razonar sobre thread safety.
- **Corrección:** Extraer handlers por mensaje en funciones separadas.
- **Estado:** OPEN (ARCHITECTURAL)

### CF-013: `TelemetryThread.cpp` solo tiene StaticWndProc — naming engañoso
- **File:** `Monix/Monix/src/native/TelemetryThread.cpp` (35 líneas) vs `Monix/Monix/src/native/telemetry/runtime/TelemetryThread.cpp` (124 líneas)
- **Evidencia:** El archivo en `src/native/` se llama `TelemetryThread.cpp` pero contiene `StaticWndProc`. El real está en `telemetry/runtime/TelemetryThread.cpp` con la función `TelemetryLoop()`.
- **Impacto:** Confusión sobre dónde está la implementación real del telemetry thread.
- **Corrección:** Renombrar o eliminar el stub.
- **Estado:** OPEN (CLEANUP)

### CF-014: `extern void runCompilationPipelineTests()` en main.cpp
- **File:** `Monix/Monix/src/native/main.cpp:29-32`
- **Evidencia:** `extern void runCompilationPipelineTests();` y `extern void runMegadrivePresetTests();` — funciones declaradas extern sin definición visible.
- **Impacto:** Si las funciones no se definen en ningún TU compilado, el linker fallará. Dependencia frágil.
- **Corrección:** Separar tests en ejecutable independiente.
- **Estado:** OPEN

### CF-015: `recursive_mutex` en MonixApp
- **File:** `src/native/main.cpp:1110` / `Monix/Monix/src/native/AppWindowProc.cpp` (múltiples ubicaciones)
- **Evidencia:** `mutable std::recursive_mutex stateMutex_` — recursive_mutex indica diseño ambiguo de ownership.
- **Impacto:** Difícil razonar sobre qué thread posee el lock en cada momento. Riesgo de deadlocks si se introducen locks externos.
- **Corrección:** Refactorizar a non-recursive_mutex con ownership claro.
- **Estado:** OPEN (ARCHITECTURAL)

### CF-016: `config_` leído sin `configMutex_` en UI
- **File:** `Monix/Monix/src/native/AppWindowProc.cpp:789-792` (TickAnimations)
- **Evidencia:** Comentario explícito: `// config_ fields read without configMutex_ — documented accepted risk`.
- **Impacto:** Torn reads posibles si `LoadConfig()` escribe mientras la UI lee.
- **Corrección:** Usar `shared_lock` en la UI o devolver snapshot de config.
- **Estado:** OPEN

### CF-017: `findingState_` crece monótonicamente sin límite
- **File:** `Monix/Monix/src/native/scram/ScramEngine.cpp:50-101`
- **Evidencia:** `findingState_` es un `std::map<size_t, FindingState>` que agrega entradas por cada finding único pero nunca las elimina. Solo resetea campos individuales.
- **Impacto:** Memory leak lento a largo plazo (miles de finding keys únicas).
- **Corrección:** Agregar limpieza periódica de entradas con `cooldownRemaining == 0 && debounceCount == 0`.
- **Estado:** OPEN

### CF-018: No hay `ctest` integration ni framework de tests
- **File:** `CMakeLists.txt` (falta `enable_testing()` y `add_test()`)
- **Evidencia:** El target `monix_tests` existe pero sin framework de tests ni comandos `ctest`.
- **Impacto:** No se pueden ejecutar tests desde CMake. El test runner depende de `.bat` scripts manuales.
- **Corrección:** Agregar `enable_testing()`, `add_test()`, y opcionalmente integrar un framework ligero (Catch2 header-only).
- **Estado:** OPEN

### CF-019: No hay CI/CD
- **Evidence:** No existe `.github/workflows/` ni ningún pipeline de build automatizado.
- **Impacto:** No hay validación automática de builds, tests, o regresiones.
- **Corrección:** Agregar GitHub Actions workflow para build + tests.
- **Estado:** OPEN

---

## LOW

### CF-020: 12 raw handles sin RAII en MonixApp
- **File:** `Monix/Monix/src/native/MonixApp.hpp` (HWND, 6x HFONT, 2x HICON, HINSTANCE, HANDLE, GDI+ token)
- **Evidencia:** Creación y destrucción manual en constructor/destructor.
- **Impacto:** Riesgo de leak si el destructor falla o no se ejecuta.
- **Corrección:** Crear RAII wrappers (FontGuard, IconGuard, HandleGuard).
- **Estado:** OPEN

### CF-021: `LoadCommonModules` usa cache `static` sin invalidación robusta
- **File:** `src/native/main.cpp:370-401`
- **Evidencia:** `static std::string cached; static std::atomic<bool> loaded{false};` — si el rootDir cambia, el cache queda stale.
- **Impacto:** Si el usuario mueve la instalación, los módulos comunes no se recargan.
- **Corrección:** Cache invalidation basada en path (el auditorío anterior ya reportó esto como FIXED, pero el código actual no tiene la corrección).
- **Estado:** OPEN

### CF-022: `debugCallback` no null-checks `callbackData`
- **File:** `src/native/vulkan_renderer.cpp:124-133`
- **Evidencia:** `callbackData->pMessage` se accede sin verificar que `callbackData` no es null.
- **Impacto:** Crash si el driver invoca el callback con datos incompletos (no debería pasar, pero es defensive coding).
- **Corrección:** Agregar `if (!callbackData) return VK_FALSE;` antes de acceder.
- **Estado:** OPEN

### CF-023: Archivos sueltos en Monix/Monix/
- **Files:** `0a6d0857b27dea91.stderr.txt`, `0a6d0857b27dea91.vertex.glsl`, `MEGA_BEZEL_PHASE*`, `PHASE4_VISUAL_REPORT.txt`, `test.input.glsl`, `pass_captures/`
- **Evidencia:** Archivos de testing/debugging sueltos en la raíz del proyecto.
- **Impacto:** Clutter, confusión sobre qué es código oficial.
- **Corrección:** Mover a `Monix-2ago-unestable/` o eliminar.
- **Estado:** OPEN (CLEANUP)

### CF-024: `Monix-2ago-unestable/` (482 .cpp) no referenciado por ningún build
- **File:** `Monix-2ago-unestable/`
- **Evidencia:** Snapshot completo del repo entero. 482 archivos .cpp. No está en .gitignore. No lo referencia CMakeLists.txt ni build.ps1.
- **Impacto:** Duplica el tamaño del repo innecesariamente. Confusión sobre qué código es activo.
- **Corrección:** Archivar al repo `Archive` y agregar a .gitignore.
- **Estado:** OPEN (CLEANUP)

### CF-025: `original/` (16 archivos) sin referencia en build
- **File:** `original/`
- **Evidencia:** Contiene pre-refactor files (main_original.cpp, login originals, etc.). No referenciado por ningún build.
- **Impacto:** Código legacy que puede confundir.
- **Corrección:** Archivar al repo `Archive`.
- **Estado:** OPEN (CLEANUP)

---

## ARCHITECTURAL

### CF-026: MonixApp es God Object con 75+ miembros y ~100+ métodos
- **Files:** `Monix/Monix/src/native/MonixApp.hpp` (322 líneas), `Monix/Monix/src/native/main.cpp` (src/ version: 10,423 líneas)
- **Evidencia:** Un solo clase posee: Win32 window, 6 HFONT, Vulkan, OpenGL, telemetry loop, process sampling, network sampling, WMI, PDH, logging, sound, notifications, SCRAM engine, 29 risk rules, config, shaders (library + runtime + compiler + browser panel), kernel auth, font management, history buffers, animation state, device context, GDI+ token, telemetry handle, DPI, border management, intro state.
- **Impacto:** Testing imposible. Cualquier cambio riska cascada de breakage. threading model es ambiguo.
- **Corrección:** Extraer subsystems progresivamente: TelemetryService, RendererService, LoggingService, ScramService, ConfigService, NotificationService, ProcessService.
- **Estado:** OPEN (pre-existente, requiere fases incrementales)

### CF-027: Build system dual sin sincronización
- **Files:** `CMakeLists.txt` vs `Monix/Monix/build.ps1`
- **Evidencia:** CMakeLists.txt compila ~30 archivos de `src/native/` (incompleto). build.ps1 compila ~80+ archivos de `Monix/Monix/src/native/` (completo). Son sistemas completamente differentes que producen diferentes ejecutables.
- **Impacto:** No se puede construir la app completa desde CMake. CMake es inútil para el build real.
- **Corrección:** Unificar en un solo sistema de build.
- **Estado:** OPEN

### CF-028: Hay 3 versiones de `main.cpp`
- **Files:**
  1. `src/native/main.cpp` — 10,423 líneas (legacy god object inline)
  2. `Monix/Monix/src/native/main.cpp` — 49 líneas (clean entry point)
  3. `original/main_original.cpp` — pre-refactor
- **Impacto:** Confusión extrema sobre cuál es la fuente oficial.
- **Corrección:** Conservar solo `Monix/Monix/src/native/main.cpp` como oficial. Archivar los otros.
- **Estado:** OPEN

---

## CLEANUP

### CF-029: Archivos test unity mezclados con source en `src/native/`
- **Files:** 12 archivos `*_test_unity.cpp` en `src/native/` raíz
- **Evidencia:** `external_compat_test_unity.cpp`, `fase13_validation_test_unity.cpp`, `fase16_test_unity.cpp`, etc. Compilados por `.bat` scripts, no por CMake.
- **Corrección:** Mover a `tests/` o directorio dedicado.
- **Estado:** OPEN

### CF-030: No hay README detallado de setup/build
- **File:** `README.md` (13 líneas)
- **Evidencia:** Solo descripción de directorios. No incluye: requisitos, cómo compilar, cómo ejecutar tests, arquitectura.
- **Corrección:** Expandir README con instrucciones completas.
- **Estado:** OPEN

---

## Resumen

| Severidad | Total | Open | Fix Status |
|-----------|-------|------|------------|
| CRITICAL | 4 | 4 | Auditoría anterior reportó fixes, pero código actual no los tiene |
| HIGH | 6 | 6 | CF-004, CF-005, CF-006 son los más impactantes |
| MEDIUM | 9 | 9 | CF-011 (`/w`) es el más fácil de corregir |
| LOW | 6 | 6 | CLEANUP items |
| ARCHITECTURAL | 3 | 3 | Requieren fases incrementales |
| CLEANUP | 2 | 2 | Repo hygiene |
| **TOTAL** | **30** | **30** | |

**Nota:** La auditoría anterior (`AUDIT-REPORT.md`) reportó 12 fixes (FIX-01 a FIX-12). El código actual en `316ea2d` **no tiene estos fixes aplicados**. Los VkResult siguen sin verificarse, `FreeLibrary` no existe, los contadores de validación no tienen mutex, y CMakeLists.txt sigue con los archivos fantasma. Esto indica que los fixes fueron aplicados en una rama que no se mergeó al HEAD actual.
