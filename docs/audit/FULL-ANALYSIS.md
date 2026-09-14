# Análisis Completo de Monix

**Fecha:** 14 de septiembre de 2026
**Repositorio:** Monix-31ago-stable (HEAD: 568a55e)
**Lenguaje:** C++20 / C (sensores) / ASM (CPU)
**Plataforma:** Windows (Win32 API)

---

## 1. Resumen General

Monix es una **aplicación de monitorización y diagnóstico de sistema** para Windows, escrita en C++20. Combina estética retro (estilo kernel de sistema operativo) con telemetría profunda del hardware y software.

**Qué es Monix:**
- Un monitor de sistema en tiempo real con 400+ campos de telemetría
- Un motor de diagnóstico con 29 reglas de salud (SCRAM)
- Un sistema de logging estructurado (plain text + JSONL)
- Un renderizador con soporte GDI/OpenGL/Vulkan y efectos CRT
- Un sistema de autenticación con secuencia de boot estilo OS
- Un gestor de procesos con prioridad y terminación
- Un explorador de shaders con compilación en caliente

**Qué no es:**
- No es un framework reutilizable
- No es un daemon de servicio
- No es una herramienta CLI (aunque tiene modos CLI)
- No es un editor de configuración visual

---

## 2. Inventario del Repositorio

### Archivos por tipo
| Tipo | Cantidad | Ubicación principal |
|------|----------|---------------------|
| .cpp | 482 | Monix/Monix/src/native/ (203), src/core/ (102), Monix-2ago-unestable/ (482) |
| .hpp | 499 | Dispersos en src/, Monix/Monix/ |
| .h | 100 | sensors/, login/, platform/ |
| .c | 1 | sensors/hardware.c |
| .asm | 1 | sensors/cpu.asm |
| .hlsl | 0 | (Shaders/ tiene .slangp/.glslp) |
| .ps1 | 4 | build scripts |
| .bat | 5 | build/run scripts |
| CMakeLists.txt | 2 | repo root, Monix-2ago-unestable/ |

### Módulos principales
| Módulo | Archivos | Estado |
|--------|----------|--------|
| **MonixApp** | MonixApp.hpp/cpp, AppWindowProc.cpp (~2800 líneas) | Funcional, God Object |
| **Telemetry** | TelemetryThread.cpp, Snapshot.hpp, Collectors.hpp | Funcional, 124 líneas de loop |
| **Logging** | LogManager.cpp, LogSink.cpp (226 líneas) | Funcional |
| **SCRAM** | ScramEngine.cpp, 29 rules/ | Funcional |
| **Config** | MonixConfig.hpp (434 líneas), SettingsRegistry | Funcional |
| **Auth** | AuthManager.hpp (sin .cpp), MonixKernel.cpp | Parcial |
| **Sensors** | sensors.h (556 líneas), hardware.c, cpu.asm | Funcional (C/ASM) |
| **Vulkan** | vulkan_renderer.cpp (~2900 líneas) | Funcional |
| **OpenGL** | RenderState.hpp, GlBackend.* | Funcional |
| **Shaders** | ShaderLibrary, ShaderRuntime, ShaderCompiler | Funcional |
| **Sound** | SoundPlayer.cpp (71 líneas) | Funcional (MCI) |
| **Notifications** | NotificationQueue.cpp (74 líneas) | Funcional |
| **Crash** | CrashHandler.cpp (58 líneas) | Funcional (VEH) |
| **Tests** | test_pipeline.cpp, test_runtime.cpp, tests/ | Parcial |

---

## 3. Mapa de Módulos

```
MonixApp (God Object)
├── Kernel (boot sequence)
│   ├── AuthManager (login)
│   ├── KernelDisplay (boot UI)
│   └── KernelSelfTest (diagnostics)
├── Telemetry
│   ├── TelemetryThread (background loop)
│   ├── Collectors (12+ collector modules)
│   ├── Snapshot (400+ field data structure)
│   ├── ProcessChangeDetector
│   └── UeoCorrelator
├── SCRAM
│   ├── ScramEngine (29 rules)
│   └── Rules (CPU, RAM, GPU, Network, Disk, etc.)
├── Logging
│   ├── LogManager (in-memory buffer)
│   ├── LogSink (file output)
│   ├── NotificationQueue
│   └── SoundPlayer
├── Config
│   ├── MonixConfig (INI parser)
│   ├── SettingsRegistry (setting definitions)
│   └── SettingSerializer
├── Rendering
│   ├── VulkanRenderer (full Vulkan pipeline)
│   ├── OpenGL (GDI fallback + GL context)
│   ├── ShaderLibrary/Runtime/Compiler
│   └── CRT post-processing
├── UI
│   ├── WndProc (973 líneas)
│   ├── Draw* methods (20+ drawing functions)
│   ├── HitTest/Input handling
│   └── DevShell
├── Security
│   ├── AuthManager (password)
│   ├── SecurityCollector
│   └── Integrity checks
└── Platform
    ├── DpiSetup
    ├── WindowFactory
    └── CrashHandler (VEH)
```

---

## 4. Mapa de Ejecución

### Secuencia de arranque confirmada por código:

1. **`wWinMain`** (`main.cpp:12`)
   - `SetupDpiAwareness()`
   - Crea `MonixApp`
   - Parsea CLI args
   - Si `--test`: ejecuta tests y sale
   - Si `--test-vulkan`: ejecuta Vulkan tests
   - Si `--capture/--compare`: activa parity mode
   - Si `--runtime-state`: activa runtime state mode
   - Si `--regression-test`: activa regression test mode
   - Llama `app.Run(instance, showCommand)`

2. **`MonixApp::MonixApp()`** (`MonixApp.cpp:18`)
   - `QueryPerformanceFrequency()`
   - Genera session ID
   - `WriteDefaultConfigIfMissing()` — crea monix.ini si no existe
   - `LoadConfig()` — carga configuración INI
   - `LoadRuntimeAssets()` — carga shaders, fuentes, sonidos
   - Crea directorios logs/ y exports/
   - `CleanupStaleLogFiles()` — elimina logs antiguos
   - `LoadRecentLogHistory()` — carga historial reciente
   - `auth_.Init()` — inicializa autenticación
   - `kernel_.Initialize()` — inicializa kernel
   - Registra 29 SCRAM rules
   - Crea `LogManager` con configuración
   - Crea `NotificationQueue`
   - Crea `SoundPlayer`
   - Conecta callbacks (onNotification, onSound, onAlertSound)
   - `SeedReferenceState()` — captura estado inicial
   - Inicializa `ShaderLibrary`, `ShaderRuntime`, `ShaderCompiler`, `ShaderBrowserPanel`

3. **`MonixApp::Run()`** (`MonixApp.cpp:107`)
   - `AddVectoredExceptionHandler(CrashVehHandler)`
   - `CreateMainWindow()` — crea ventana Win32
   - Log de estado OpenGL
   - `StartTelemetry()` — lanza hilo de telemetría
   - `SetFrameTimer()` — configura timer de renderizado
   - **Message loop** (`GetMessageW/TranslateMessage/DispatchMessageW`)

4. **`WndProc`** (`AppWindowProc.cpp:861`)
   - `WM_CREATE`:初始化 kernel, fuentes, OpenGL
   - `WM_PAINT`: llama `Render()`
   - `WM_TIMER`: tick animations, flush logs
   - `WM_KEYDOWN`: input handling (tabs, settings, devshell)
   - `WM_CHAR`: text input
   - `WM_LBUTTONDOWN`: click handling
   - `WM_SIZE`: resize renderer
   - `WM_MONIX_UPDATE`: refresh from telemetry thread
   - `WM_MOUSEWHEEL`: scroll

5. **`TelemetryLoop()`** (`TelemetryThread.cpp:65`)
   - Loop while `running_`
   - `PollSnapshot()` — recopila datos del sistema
   - `ConsumeSnapshot()` — actualiza estado bajo lock
   - `PostMessage(WM_MONIX_UPDATE)` — notifica UI
   - Delay adaptativo basado en riesgo SCRAM (75ms base × 1-5)
   - Puede interrumpirse con `refreshRequested_`

6. **Cierre**
   - `WM_DESTROY`: post quit message
   - `~MonixApp()`: `StopTelemetry()`, `FlushLogQueues(true)`, libera OpenGL/fuentes/iconos

---

## 5. Funcionalidades Existentes

### 5.1 Arranque y Cierre
- **Boot sequence** estilo kernel con fases: Startup → Diagnostics → Ready → Authenticating → AuthSuccess → LoadingEnvironment → LoadingGui → Running
- **Diagnostics** ejecuta tests del sistema al iniciar
- **Login** con password (Ctrl+Shift+K para skip en dev)
- **Loading** con barra de progreso animada
- **Shutdown** con fase Halted
- **Crash handler** con VEH que registra fase, dirección, TID

### 5.2 Autenticación
- `AuthManager` — password-based con lockout
- `MonixKernel` gestiona el flujo de auth
- Password visible/invisible con Tab
- Intentos de auth contados
- Skip con Ctrl+Shift+K (dev mode)

### 5.3 Pantalla Principal
- **6 tabs**: Log, Tasks, Hardware, Network, SCRAM, Settings
- **Tab navigation** con click y teclado
- **Footer** con información de estado
- **Toast messages** para feedback rápido
- **Notifications** overlay para errores/críticos

### 5.4 Logging
- **LogManager**: buffer en memoria con 480 entries por defecto
- **Niveles**: Debug, Info, Warn, Error, Critical
- **Dominios**: CONFIG, OPENGL, SYSTEM, SCRAM, etc.
- **Deduplicación** con repeat count
- **Rate limiting** por dominio y global
- **Salida**: plain text (.log) + JSONL (.jsonl)
- **Rotación** por tamaño (1MB por defecto)
- **Retención** configurable (7 días por defecto)
- **Historial EMA** para sparklines (warn, error, crit, kernel, net)

### 5.5 Telemetría
- **400+ campos** en Snapshot
- **12+ collectors** modulares
- **Hilo dedicado** con 8MB stack
- **Adaptativo**: delay basado en riesgo SCRAM
- **Crash logging** con fase tracking

### 5.6 SCRAM (Sistema de Reglas de Salud)
- **29 reglas** que evalúan salud del sistema:
  - CPU: presión, identidad, scheduling
  - RAM: agotamiento, subsistema
  - GPU: utilización, VRAM, driver, térmica, potencia, ventilador
  - TDR: estabilidad
  - Red: topología, tráfico, latencia, VPN
  - Disco: almacenamiento
  - Threads: scheduling, handles
  - Display: configuración, frame time
  - Procesos: triage
  - OS: kernel
  - Seguridad: integridad
  - Power: batería
  - Thermal: cooling, sensores
  - Hardware: board, sensores
  - Filesystem: datos
  - Registry: configuración
  - Audio: multimedia
  - Reliability: recuperación

### 5.7 Procesos
- **Enumeración** con PID, PPID, CPU, RAM, GPU, status, priority
- **Ajuste de prioridad** (ApplyPriorityToProcess)
- **Terminación** (TerminateProcessById)
- **Context menu** con 9 acciones
- **Change detector** para detectar inicio/fin
- **Scroll** y selección

### 5.8 Hardware
- **CPU**: vendor, brand, features, cache, TSC, IPC estimate
- **RAM**: total, used, available, commit, pagefile, working set
- **GPU**: usage, temp, VRAM, model, driver, power, fan
- **Disk**: read/write bytes, IOPS, queue, latency, SMART, NVMe temp
- **Display**: refresh rate, resolution, monitor count, HDR
- **Power**: battery, charger, wear, cycles, power plan
- **Thermal**: CPU temp, motherboard, VRM, fans, pump
- **BIOS**: version, manufacturer, SMBIOS hash
- **Audio**: devices, volume, sample rate

### 5.9 Red
- **Interfaces**: adapters, speeds, oper status, types
- **Conexiones**: inbound/outbound, TCP states
- **DNS**: resolution time, pseudo records
- **Ping**: RTT al gateway
- **Route table**: hash para detectar cambios
- **Traffic**: bytes in/out per sec
- **TCP**: retransmits, resets
- **VPN**: adapter detection
- **Proxy**: detection

### 5.10 Seguridad
- **Self signature**: verificación de integridad
- **Unsigned drivers**: detección
- **Suspicious modules**: heurística
- **LSASS access**: detección
- **Debug port**: detección
- **VM indicators**: detección
- **Hook modules**: detección
- **PE header tamper**: detección
- **Scheduled tasks**: count
- **Process paths**: collection

### 5.11 Configuración
- **INI-style** (monix.ini)
- **60+ settings** cubriendo todos los subsistemas
- **Live reload** con F5
- **Font scanning** con TTF face name reading
- **Path resolution** con repo marker detection
- **Export/Import** de logs (JSON, CSV)

### 5.12 UI
- **Win32 GDI** rendering con fallback
- **OpenGL** context para post-processing
- **Vulkan** shader pipeline
- **CRT effects**: curvature, scanlines, chromatic aberration, noise, vignette, bloom, burn-in
- **Borders**: imágenes decorativas
- **Fonts**: DPI-aware, múltiples fuentes
- **Animations**: intro logo, tick animations
- **Sparklines**: historial en miniatura
- **DevShell**: terminal interna (Ctrl+Shift+Alt+F5)

### 5.13 Sonido
- **MCI-based** playback
- **Log sounds**: 12 variantes
- **Alert sounds**: Error.mp3, Warning.mp3
- **Click sounds**: click.mp3

### 5.14 Notificaciones
- **Toast-style** para errores/críticos
- **Auto-expire** después de 4.2s
- **Alert sound** en notificación
- **Stack limit** (4 por defecto)

### 5.15 Shaders
- **ShaderLibrary**: escaneo de archivos .slangp/.glslp
- **ShaderRuntime**: ejecución de shaders
- **ShaderLibraryCompiler**: compilación en caliente
- **ShaderBrowserPanel**: UI para explorar shaders
- **TransactionalShaderState**: operaciones atómicas

### 5.16 CLI Modes
- `--test`: ejecuta compilation pipeline tests + Megadrive preset tests
- `--test-vulkan`: ejecuta Vulkan runtime tests
- `--capture/--compare`: parity mode para visual regression
- `--runtime-state`: comparación con RetroArch snapshots
- `--regression-test`: testing automatizado de presets

---

## 6. Funcionalidades Incompletas

### 6.1 AuthManager
- **Archivo**: `login/AuthManager.hpp` (solo header)
- **Problema**: No existe `AuthManager.cpp` — la implementación puede estar en el header o no haberse escrito
- **Estado**: Declarado pero posiblemente sin implementación completa

### 6.2 Sound Stubs
- `MonixApp::PlayAlertSound()` — vacío (`MonixApp.cpp:173`)
- `MonixApp::PlayClickSound()` — vacío (`MonixApp.cpp:176`)
- **Nota**: `SoundPlayer` tiene implementación real, pero MonixApp no los delega

### 6.3 Collectors.cpp
- **Archivo**: `telemetry/Collectors.cpp` — 1 línea (`#include "Collectors.hpp"`)
- **Problema**: Solo incluye el header, no implementa las funciones declaradas
- **Estado**: Las funciones pueden estar en otros archivos o no haberse escrito

### 6.4 SettingsRegistry.cpp
- **Archivo**: `settings/SettingsRegistry.cpp` — 3 líneas
- **Problema**: Solo incluye headers, no implementa RegistryLoadSetting/RegistrySaveSetting
- **Estado**: La implementación puede estar en headers inline

### 6.5 Export Functions
- `ExportLogsJson()` — declarada en MonixApp.hpp
- `ExportLogsCsv()` — declarada en MonixApp.hpp
- **Estado**: No se verificó implementación

### 6.6 Show_* Config Flags
- 40+ flags `show_*_panel` en MonixConfig
- **Estado**: No se verificó si están conectados a la UI

### 6.7 Event Bus
- Referenciado en AppStateData.hpp
- **Estado**: No se leyó implementación completa

---

## 7. Sistemas Conectados y Desconectados

### Completamente conectados
- Telemetry → Snapshot → ConsumeSnapshot → UI refresh
- LogManager → NotificationQueue → SoundPlayer
- SCRAM Engine → Risk calculation → Telemetry delay
- Config → LoadConfig → All subsystems
- WndProc → Render → Draw methods
- Process list → Context menu → Priority/Terminate

### Parcialmente conectados
- SoundPlayer (implementado pero MonixApp stubs son vacíos)
- AuthManager (puede estar incompleto)
- Show_* flags (puede no estar conectados a UI)
- Export functions (declaradas, implementación no verificada)

### Desconectados
- Monix-2ago-unestable/ (copia completa pero no usada por build principal)
- src/ (refactor incompleto, CMake roto)
- Algunos tests linked into production binary

---

## 8. Logging y Telemetría

### Sistema de Logging
| Característica | Estado |
|----------------|--------|
| Niveles | Debug, Info, Warn, Error, Critical |
| Dominios | CONFIG, OPENGL, SYSTEM, SCRAM, etc. |
| Timestamps | Sí (wall clock + monotonic) |
| Thread ID | Sí |
| Process ID | Sí |
| Deduplicación | Sí (repeat count) |
| Rate limiting | Sí (por dominio + global) |
| Rotación | Sí (por tamaño) |
| Retención | Sí (configurable) |
| Plain text | Sí (.log) |
| JSONL | Sí (.jsonl) |
| Export JSON | Declarado |
| Export CSV | Declarado |
| Cola asíncrona | Sí (pendingPlainLogs/pendingJsonLogs) |
| Bloquea UI | No (flush periódico) |

### Telemetría
| Collector | API Windows | Frecuencia | Estado |
|-----------|-------------|------------|--------|
| CPU | GetSystemTimes, CPUID | 75ms base | Funcional |
| RAM | GlobalMemoryStatusEx | 75ms base | Funcional |
| GPU | DXGI, WMI | 75ms base | Funcional |
| Disk | GetDiskTypesEx, PDH | 75ms base | Funcional |
| Network | GetAdaptersAddresses, GetTcpTable | 75ms base | Funcional |
| Processes | CreateToolhelp32Snapshot | 75ms base | Funcional |
| Power | GetSystemPowerStatus | 75ms base | Funcional |
| Thermal | WMI, SetupAPI | 75ms base | Funcional |
| Hardware | SMBIOS, ACPI, SetupAPI | 75ms base | Funcional |
| Audio | waveOut, PDH | 75ms base | Funcional |
| Security | WinVerifyTrust, PSAPI | 75ms base | Funcional |
| Filesystem | FindFirstFile, GetDiskFreeSpace | 75ms base | Funcional |
| Registry | RegOpenKeyEx, RegQueryValueEx | 75ms base | Funcional |

---

## 9. Procesos y Servicios

### Funciones de procesos
| Función | Archivo | Tipo | Estado |
|---------|---------|------|--------|
| Enumeración | Collectors (process) | Lectura | Funcional |
| Nombre | ProcessInfo.name | Lectura | Funcional |
| PID/PPID | ProcessInfo.pid/parentPid | Lectura | Funcional |
| CPU/RAM/GPU | ProcessInfo.cpuPct/ramBytes/gpuPct | Lectura | Funcional |
| Status/Priority | ProcessInfo.status/priority | Lectura | Funcional |
| Create time | ProcessInfo.createTime100ns | Lectura | Funcional |
| Prioridad | ApplyPriorityToProcess | Operación potencialmente peligrosa | Funcional |
| Terminación | TerminateProcessById | Operación destructiva | Funcional |
| Change detection | ProcessChangeDetector | Lectura | Funcional |

### Limitaciones conocidas
- No se verifica PID reuse
- No se manejan procesos protegidos
- No hay elevación de privilegios
- No hay confirmación antes de terminar

---

## 10. Red

### Funcionalidades implementadas
| Datos | API | Estado |
|-------|-----|--------|
| Interfaces de red | GetAdaptersAddresses | Funcional |
| IP local | GetAdaptersAddresses | Funcional |
| MAC | GetAdaptersAddresses | Funcional |
| Velocidad | GetAdaptersAddresses | Funcional |
| Conexiones activas | GetExtendedTcpTable | Funcional |
| DNS | DNS API | Funcional |
| Gateway | GetIpForwardTable | Funcional |
| Ping | IcmpSendEcho | Funcional |
| Traffic stats | GetIfTable2 | Funcional |
| TCP retransmits | GetTcpStatistics | Funcional |
| Route table | GetIpForwardTable | Funcional |
| Proxy detection | WinHTTP | Funcional |
| VPN detection | WMI | Funcional |

### No implementado
- IP pública
- Latencia a targets arbitrarios
- Puerto listening
- Firewall rules
- Wi-Fi specific (SSID, signal)
- DHCP lease info
- IPv6 detailed

---

## 11. Hardware

### Datos obtenidos
| Categoría | API | Fiabilidad |
|-----------|-----|------------|
| CPU info | CPUID, MSRs | Alta |
| CPU timing | RDTSC, RDTSCP | Alta |
| CPU frequency | TSC delta | Media (estimada) |
| RAM info | GlobalMemoryStatusEx | Alta |
| Memory details | Performance counters | Alta |
| GPU info | DXGI, WMI | Media (depende de driver) |
| GPU temp | WMI/NVML | Media (depende de hardware) |
| Disk info | GetDiskTypesEx | Alta |
| Disk health | SMART | Alta (NVMe) |
| Disk temp | NVMe ioctl | Media (depende de hardware) |
| Network | IPHlpApi | Alta |
| Display | EnumDisplayDevices | Alta |
| Battery | GetSystemPowerStatus | Alta |
| Power plan | Power Scheme API | Alta |
| Thermal | WMI/OpenHardwareMonitor | Baja (depende de hardware) |
| Fans | WMI | Baja (depende de hardware) |
| BIOS | SMBIOS/SetupAPI | Alta |
| Voltages | WMI | Baja (depende de hardware) |
| TPM | TBS API | Alta |
| Audio | waveOut/PDH | Media |

### Limitaciones
- Datos térmicos dependen de hardware/WMI
- GPU temp puede no estar disponible
- Voltajes pueden ser 0 si el hardware no los expone
- Fan speeds pueden no estar disponibles en desktops

---

## 12. UI

### Tabs existentes
| Tab | Contenido | Datos fuente |
|-----|-----------|--------------|
| **Log** | Lista de logs con toolbar, filtros, column headers, sidebar | LogManager |
| **Tasks** | Lista de procesos con context menu | Snapshot.processes |
| **Hardware** | Info de hardware del sistema | Snapshot (CPU, RAM, GPU, Disk) |
| **Network** | Info de red | Snapshot (interfaces, connections) |
| **SCRAM** | Salud del sistema con reglas | ScramEngine |
| **Settings** | Configuración con sub-categorías | Config |

### Acciones disponibles
- Click en tabs para navegar
- Scroll en listas
- Context menu en procesos (prioridad, terminar)
- Ajuste de settings con flechas
- F5 para reload config
- Ctrl+Shift+Alt+F5 para DevShell
- Ctrl+Shift+K para skip boot tests
- F10 para shutdown desde Running
- Tab para toggle password visibility
- Escape para shutdown desde auth

### Rendering
- **GDI**: fallback principal
- **OpenGL**: context para post-processing
- **Vulkan**: shader pipeline (no UI primaria)
- **CRT effects**: curvature, scanlines, chromatic aberration, noise, vignette, bloom, burn-in
- **Borders**: imágenes decorativas
- **Sparklines**: historial en miniatura

---

## 13. Renderizado

### Backends
| Backend | Uso | Estado |
|---------|-----|--------|
| GDI | UI principal, fallback | Funcional |
| OpenGL | Post-processing, CRT | Funcional |
| Vulkan | Shader pipeline | Funcional |

### Vulkan
- **Instance**: creación con validation layers
- **Surface**: Win32 surface
- **Device**: selección de GPU
- **Swapchain**: triple buffering
- **Command buffers**: frame-based
- **Sync**: fences + semaphores
- **Shaders**: SPIR-V compilation
- **Screenshot**: staging buffer

### OpenGL
- **Context**: wglCreateContext
- **UI surface**: DIB section para GDI rendering
- **Shader renderer**: GLSL pipeline
- **Preset system**: .glslp/.slangp files

### CRT Effects
- Curvature, scanlines, sharpness
- Chromatic aberration, RGB shift
- Noise, grain, jitter
- Flicker, vignette
- Subpixel mode, bloom
- Burn-in (accumulation + decay)

---

## 14. EventBus

### Tipos de evento (TelemetryEvents.hpp)
| EventType | Nombre | Uso |
|-----------|--------|-----|
| TelemetrySample | TELEMETRY | Datos de telemetría |
| StateChange | STATE_CHANGE | Cambios de estado |
| Anomaly | ANOMALY | Anomalías detectadas |
| Warning | WARNING | Advertencias |
| Error | ERROR | Errores |
| Debug | DEBUG | Debug info |
| Diagnostic | DIAGNOSTIC | Diagnósticos |
| LifecycleEvent | LIFECYCLE | Ciclo de vida |

### Mecanismos
- **TelemetrySource**: health tracking por collector
- **SubsystemRateLimiter**: flood protection (20 events/tick)
- **CausalGroup**: root cause analysis (agrupa eventos relacionados)
- **HysteresisState**: threshold detection con hysteresis
- **TelemetryHealthManager**: monitoreo de salud de collectors

---

## 15. Configuración

### Settings existentes (60+)
| Categoría | Settings |
|-----------|----------|
| **General** | theme_mode, language, always_on_top, start_maximized |
| **Rendering** | vsync, crt_postprocess, crt_intensity, border |
| **Font** | font_scale, font_face_index |
| **Window** | window_opacity, save_window_position |
| **Process** | process_priority |
| **Logging** | log_level, log_milliseconds, log_buffer_size, log_visible_lines, log_view |
| **Telemetry** | telemetry_interval_ms, snapshot_interval_ms |
| **Log Output** | log_flush_interval_ms, log_retention_days, log_plain_enabled, log_json_enabled, log_max_file_bytes, log_deduplicate |
| **Notifications** | notifications_enabled, notification_duration_ms, notification_max_stack |
| **Sound** | sound_enabled |
| **History** | analytics_history_enabled, history_capacity |
| **Auth** | user_id, intro_enabled |
| **Intro** | intro_step_px, intro_hold_ms, intro_credit_delay_ms |
| **CRT** | 15+ settings CRT |
| **Panels** | 40+ show_*_panel flags |

### Persistencia
- **Formato**: INI-style (key=value)
- **Archivo**: monix.ini
- **Guardado**: SaveConfigToFile()
- **Carga**: LoadConfigFromFile()
- **Live reload**: F5
- **Backup**: no implementado
- **Migración**: no implementada

---

## 16. Seguridad

### Funcionalidades existentes
- Password-based authentication
- Lockout tras intentos fallidos
- Self signature verification
- Unsigned driver detection
- LSASS access detection
- Debug port detection
- VM indicator detection
- Hook module detection
- PE header tamper detection

### Riesgos confirmados
- `TerminateProcessById` sin confirmación
- `ApplyPriorityToProcess` sin validación
- No hay modo read-only
- No hay sandboxing de operaciones

### Riesgos potenciales
- Config file escritura sin permisos
- Log files con información sensible (paths, usernames)
- Process names y paths registrados
- IP/MAC addresses registrados

---

## 17. Sistema de Comandos

### DevShell (Ctrl+Shift+Alt+F5)
- Terminal interna en la UI
- Historial de comandos
- Input de texto
- Scroll

### CLI Args
- `--test`: ejecuta tests
- `--test-vulkan`: ejecuta Vulkan tests
- `--capture/--compare`: parity mode
- `--runtime-state`: runtime state mode
- `--regression-test`: regression testing

### No existe
- Sistema de comandos en runtime
- scripting
- alias
- autocompletado

---

## 18. Tests

### Tests existentes
| Test | Tipo | Estado |
|------|------|--------|
| runCompilationPipelineTests | Unit | Linked into production |
| runMegadrivePresetTests | Unit | Linked into production |
| runVulkanRuntimeTests | Integration | Requires GPU |
| KernelSelfTest | Diagnostic | Funcional |
| tests/core/*.cpp | Unit | 30 archivos |

### Limitaciones
- Tests linked into production binary
- No test framework (printf-based)
- No CI integration
- Tests requieren GPU/Vulkan
- No test isolation

---

## 19. Problemas Técnicos Confirmados

### CRITICAL
1. **AuthManager sin .cpp** — implementación potencialmente incompleta
2. **Collectors.cpp vacío** — 1 línea, funciones declaradas no implementadas
3. **Tests en production** — test_pipeline.cpp y test_runtime.cpp compilados en Monix.exe

### HIGH
4. **God Object** — MonixApp 67+ miembros, 65+ métodos
5. **WndProc 973 líneas** — maneja todo el input/UI
6. **Dual code tree** — src/ y Monix/Monix/ divergidos
7. **CMake roto** — referencia archivos inexistentes
8. **recursive_mutex** — puede enmascarar deadlocks

### MEDIUM
9. **SoundPlayer stubs** — PlayAlertSound/PlayClickSound vacíos en MonixApp
10. **No backup de config** — monix.ini se sobreescribe
11. **No migración de config** — cambios de schema rompen configuración
12. **No validación de config** — valores inválidos se aceptan silenciosamente
13. **process_priority sin validación** — valores fuera de rango

### LOW
14. **No test framework** — printf-based testing
15. **No CI** — builds manuales
16. **No documentation** — README básico

---

## 20. Riesgos Potenciales

### Rendimiento
- Snapshot de 400+ campos puede ser lento en hardware viejo
- 75ms polling interval puede ser agresivo en batería
- SCRAM 29 reglas pueden acumular latencia

### Estabilidad
- Vulkan renderer sin cleanup en init failure (parcialmente mitigado)
- Telemetry thread sin proper shutdown timeout
- Config torn reads con shared_mutex

### Seguridad
- Process termination sin confirmación
- Password en plaintext en memoria
- Log files con información sensible

### Mantenibilidad
- God Object difícil de testear
- Dual code tree confunde贡献者
- CMake roto impide build limpio

---

## 21. Funcionalidades que Podemos Añadir

### Nuevos Collectors
- USB devices (SetupAPI)
- Bluetooth (BluetoothAPIs)
- Wi-Fi details (WLAN API)
- Disk health history (SMART trending)
- Process I/O per process
- GPU compute utilization
- Display HDR metadata
- Audio session details

### Nuevos Eventos
- Process lifecycle events
- Network topology changes
- Hardware plug/unplug
- Power state transitions
- Thermal events
- Security alerts
- Config changes
- User login/logout

### Nuevas Pantallas
- Process detail view
- Network connections map
- Disk health dashboard
- Thermal monitor
- Power analytics
- Security audit
- Event timeline
- System comparison

### Nuevos Widgets
- Gauge (circular progress)
- Timeline (horizontal history)
- Tree view (process hierarchy)
- Heatmap (resource usage)
- Alert panel (critical events)
- System info cards

### Nuevos Filtros
- Log filtering by domain/severity/module
- Process filtering by name/PID/CPU
- Network filtering by adapter/protocol
- SCRAM filtering by rule/severity

### Nuevos Modos
- Dark mode
- High contrast mode
- Minimal mode
- Fullscreen mode
- Multi-monitor support
- Kiosk mode

### Diagnóstico Avanzado
- System timeline (event history)
- Process tree with resource usage
- Network connection map
- Disk I/O heatmap
- Thermal history
- Power consumption graph

### Exportación
- PDF reports
- HTML dashboards
- CSV data export
- JSON API
- System snapshot comparison

---

## 22. Plan de Expansión por Fases

### Fase 1: Estabilización (1-2 semanas)
- Fix AuthManager
- Fix Collectors.cpp
- Separate tests from production
- Fix CMakeLists.txt
- Add /W4 warning level

### Fase 2: Refactor Core (2-4 semanas)
- Extract MonixApp responsibilities
- Create service interfaces
- Add config snapshot safety
- Fix data races
- Add proper test framework

### Fase 3: Nuevos Collectors (2-3 semanas)
- USB, Bluetooth, Wi-Fi collectors
- Process I/O per process
- GPU compute utilization
- Disk health trending

### Fase 4: UI Improvements (3-4 semanas)
- New widgets (gauge, timeline, heatmap)
- Dark mode
- Multi-monitor support
- Process detail view
- Network map

### Fase 5: Diagnóstico Avanzado (2-3 semanas)
- System timeline
- Event correlation
- Root cause analysis
- System comparison
- Snapshot diff

### Fase 6: Export y API (1-2 semanas)
- PDF reports
- JSON API
- CSV export
- Web dashboard

---

## 23. Qué Conservar

| Sistema | Razón |
|---------|-------|
| **Telemetry pipeline** | Bien diseñado, modular, eficiente |
| **SCRAM engine** | Arquitectura sólida, 29 reglas funcionales |
| **Logging system** | Completo, con dedup, rate limiting, rotación |
| **Snapshot struct** | 400+ campos, cubre todo el hardware |
| **Boot sequence** | UX único, funcional |
| **Config system** | INI simple, live reload |
| **Crash handler** | VEH con phase tracking |
| **Sensors (C/ASM)** | Rendimiento directo, close to hardware |

---

## 24. Qué Reorganizar

| Sistema | Problema | Solución |
|---------|----------|----------|
| **MonixApp** | God Object | Extraer en servicios independientes |
| **WndProc** | 973 líneas | Separar input, rendering, dispatch |
| **Dual code tree** | src/ vs Monix/Monix/ | Merge a un solo árbol |
| **CMakeLists.txt** | Roto, referencia archivos inexistentes | Reescribir matching build.ps1 |
| **Tests** | Linked into production | Separar en target independiente |

---

## 25. Qué Completar

| Sistema | Estado | Necesita |
|---------|--------|----------|
| **AuthManager** | Solo header | Implementación completa |
| **SoundPlayer delegation** | Stubs vacíos | Delegar a SoundPlayer |
| **Collectors.cpp** | 1 línea | Implementar funciones |
| **SettingsRegistry.cpp** | 3 líneas | Implementar load/save |
| **Export functions** | Declaradas | Implementar JSON/CSV |
| **Show_* flags** | Puede no estar conectado | Verificar conexión a UI |

---

## 26. Qué Reemplazar

| Sistema | Problema | Alternativa |
|---------|----------|-------------|
| **recursive_mutex** | Enmascara deadlocks | std::mutex con análisis de lock order |
| **GDI rendering** | Lento para UI compleja | Mantener como fallback, mejorar OpenGL |
| **printf-based tests** | No escalable | Adoptar Google Test o similar |

---

## 27. Prioridades

### Inmediato (esta semana)
1. Verificar AuthManager implementation
2. Fix Collectors.cpp
3. Separate tests from production
4. Fix CMakeLists.txt

### Corto plazo (2 semanas)
5. Extract MonixApp core services
6. Add config snapshot safety
7. Fix data races en telemetry
8. Add proper test framework

### Medio plazo (1 mes)
9. New collectors (USB, Bluetooth, Wi-Fi)
10. UI improvements (dark mode, new widgets)
11. System timeline
12. Export functionality

### Largo plazo (3 meses)
13. Plugin system
14. Web dashboard
15. Multi-platform (Linux?)
16. API externa

---

## 28. Conclusión

Monix es una aplicación **sólida y ambiciosa** que combina monitoreo profundo de sistema con una UX única estilo retro. Sus puntos fuertes son:

- **Telemetría exhaustiva** — 400+ campos cubren CPU, RAM, GPU, disco, red, seguridad, thermal, power, audio, filesystem, registry
- **SCRAM engine** — 29 reglas de salud con análisis de causa raíz
- **Logging completo** — dedup, rate limiting, rotación, JSONL
- **Boot sequence** — UX único que diferencia a Monix de otras herramientas
- **Sensors C/ASM** — rendimiento close-to-hardware

Sus debilidades principales son:

- **God Object** — MonixApp hace demasiado
- **Tests en production** — no hay separación
- **Dual code tree** — confusión de fuentes
- **AuthManager incompleto** — puede no funcionar

**Recomendación**: Monix tiene una base sólida. El camino es estabilizar (fix auth, separate tests, merge trees), luego refactorizar (extract services), luego expandir (new collectors, UI, export). No necesita reescritura — necesita organización.

---

*Análisis generado el 14 de septiembre de 2026*
*Repositorio: Monix-31ago-stable HEAD: 568a55e*
