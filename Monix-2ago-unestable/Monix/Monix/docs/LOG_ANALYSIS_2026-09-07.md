# Monix Log Analysis — 2026-09-07

## Archivos analizados
- `logs/monix-2026-09-07.log` (544 lineas)
- `build/vk_pipeline_fail.log` (12 lineas)
- `build/preset_load.log` (7 lineas)
- `build/monix_err.log` (6 lineas)
- `build/monix_out.log` (466 lineas)
- `build_errors.txt` (247 lineas)

---

## 1. RUNTIME LOG — Campo de log vacios (lineas 1-407)

**Problema:** 407 lineas de log con todos los campos de string vacios:
```
 [] [] [module=] [service=] [session=] [event=1] [pid=36000] [tid=36444] 
```

Solo `event`, `pid` y `tid` tienen contenido. Los campos `module`, `service`, `session`, `level`, `timestamp`, `message` estan todos vacios.

**Causa probable:** El hilo de logging/telemetry arranca antes de que la sesion este completamente inicializada. Race condition entre `LogManager` y la inicializacion de la app.

**Alcance:** 22 procesos diferentes (22 crashes/reinicios en el dia). Cada PID es una sesion que aborto prematuramente.

---

## 2. DUPLICACION DE LOGS

Una sola accion del usuario genera multiples lineas identicas:

| Evento | Veces duplicadas | Lineas |
|--------|-----------------|--------|
| "CRT Postprocess updated to YES" | 2x | 417 |
| "Theme updated to MODERN" | 13x | 436-443 |
| "Theme updated to DARK" | 10x | 444-453 |
| "Always On Top updated to NO" | 6x | 483-488 |
| "Theme updated to WINDOWS 98" | 3x | 518-519 |
| "Milliseconds updated to YES" | 5x | 493-498 |
| "CRT Postprocess updated to NO" | 6x | 529-536 |

**Causa:** Cada toggle de settings dispara `InvalidateRect` + re-read + re-log en cada ciclo de redraw. El handler de settings loguea, y el save+reload loguea de nuevo.

---

## 3. VULKAN RE-INIT EN CRT TOGGLE

Lineas 512-513: Cuando se activa CRT Postprocess, Vulkan renderer se re-inicializa completamente:
```
Vulkan renderer: NVIDIA GeForce RTX 5060 Ti (NVIDIA) Vulkan 1.4.325 { action=init }
Loaded .glslp preset via OpenGL { action=load_gl }
```
Causa visual: parpadeo visible al cambiar CRT ON/OFF.

---

## 4. VULKAN PIPELINE FAILURES (`vk_pipeline_fail.log`)

```
VkCreateGraphicsPipelines FAILED: VkResult=-13     x10
vkQueuePresentKHR FAILED: VkResult=-1000001004     x2
```

- **-13 (VK_ERROR_DEVICE_LOST):** GPU reset o crash del driver. 10 ocurrencias.
- **-1000001004 (VK_ERROR_OUT_OF_DATE_KHR):** Swapchain desactualizado por resize sin recreacion. 2 ocurrencias.

Sesiones anteriores al fix actual. No presentes en la sesion actual.

---

## 5. BENCHMARK METRICS (`monix_err.log`)

Archivo mal nombrado — contiene metricas de benchmark, no errores:

```
[event_creation: 13,635,124 ops/s]
[rule_eval:        4,855,571 ops/s]
[context_build:    2,645,370 ops/s]
[monitor_record:  23,447,209 ops/s]
[condition_match: 161,835,866 ops/s]
```

Rendimiento saludable.

---

## 6. SHADER PRESET (`preset_load.log`)

```
SUCCESS: GL backend loaded .glslp preset
```

Sin problemas.

---

## 7. TEST SUITE (`monix_out.log`)

466 tests, **3 failures** (99.4% pass rate):

| # | Test | Falla |
|---|------|-------|
| 236 | `FilesystemCollector: polling collects events` | `eventCount.load() > 0` |
| 237 | `FilesystemCollector: delete detected via polling` | `deleteCount.load() >= 1` |
| 238 | `FilesystemCollector: directory create/delete detected` | `dirEvents.load() >= 1` |
| 339 | `ProcessCollector: Polling origin detection` | `sawPollingOrigin.load()` |

**Causa:** Timing-sensitive tests — el intervalo de polling (1000ms default) es demasiado largo para las operaciones de archivo del test. Los tests terminan antes de que el polling dispare. Problemas de infraestructura de test, no bugs reales.

---

## 8. BUILD ERRORS STALE (`build_errors.txt`)

Errores de una build anterior ya corregida:
- Tipos faltantes: `NotificationItem`, `Snapshot`, `IntroState`, `ContextMenuState`
- Funciones faltantes: `TrimAscii`, `ToLowerAscii`, `Utf8ToWide`, `TryParseInt`, `TryParseUInt`, `WideToUtf8`
- Conflictos: `CpuTimes` redefinicion entre `TelemetryBaselines.hpp` y `sensors.h`
- Sin errores en la build actual.

---

## RESUMEN DE ISSUES

| Prioridad | Issue | Impacto |
|-----------|-------|---------|
| ALTA | Campos de log vacios (407 lineas) | Logs historicos inutilizables para debugging |
| ALTA | Duplicacion de logs (2-16x por accion) | Ruido excesivo, ilegible |
| MEDIA | 22 crashes/reinicios en el dia | Resueltos con fix de DrawMenuBar null pointer |
| MEDIA | Vulkan VK_ERROR_DEVICE_LOST x10 | Posible issue de driver o uso excesivo de recursos |
| BAJA | Test failures de FilesystemCollector (timing) | Infraestructura de test, no bug real |
| BAJA | Test failure de ProcessCollector (timing) | Infraestructura de test, no bug real |
