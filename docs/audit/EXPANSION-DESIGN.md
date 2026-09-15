# Monix Expansion Design — Technical Specification v2

**Fecha:** 15 de septiembre de 2026
**Repositorio:** Monix-31ago-stable (HEAD: 50ad095)
**Objetivo:** Disenar la evolucion de Monix hacia una plataforma completa de observacion, diagnostico, analisis e interaccion con Windows.

---

## INDICE

1. [Inventario del codigo existente](#1-inventario-del-codigo-existente)
2. [Snapshot como modelo central](#2-snapshot-como-modelo-central)
3. [Sistema de eventos](#3-sistema-de-eventos)
4. [Snapshot Diff](#4-snapshot-diff)
5. [Timeline](#5-timeline)
6. [Sesiones](#6-sesiones)
7. [SCRAM 2.0](#7-scram-20)
8. [Correlacion](#8-correlacion)
9. [Deteccion de anomalias](#9-deteccion-de-anomalias)
10. [Historial de procesos](#10-historial-de-procesos)
11. [Historial de red](#11-historial-de-red)
12. [Historial de hardware](#12-historial-de-hardware)
13. [Centro de diagnostico](#13-centro-de-diagnostico)
14. [Logging evolucionado](#14-logging-evolucionado)
15. [UI](#15-ui)
16. [Dashboard](#16-dashboard)
17. [Sistema de acciones](#17-sistema-de-acciones)
18. [Exportacion](#18-exportacion)
19. [Comparacion](#19-comparacion)
20. [Sistema visual](#20-sistema-visual)
21. [Arquitectura final propuesta](#21-arquitectura-final-propuesta)
22. [Reorganizacion del codigo](#22-reorganizacion-del-codigo)
23. [CMake](#23-cmake)
24. [Testing](#24-testing)
25. [Plan de implementacion](#25-plan-de-implementacion)

---

## 1. INVENTARIO DEL CODIGO EXISTENTE

### 1.1 Estructuras centrales analizadas

| Estructura | Archivo | Lineas | Campos/Metodos | Estado |
|------------|---------|--------|----------------|--------|
| Snapshot | `telemetry/Snapshot.hpp` | 413 | ~300 campos, 2 structs anidados | Funcional, sin sub-structs |
| ProcessInfo | `telemetry/Snapshot.hpp:18-30` | 13 | 10 campos | Funcional |
| NetworkFlow | `telemetry/Snapshot.hpp:32-37` | 6 | 4 campos | Funcional |
| ScramEngine | `scram/ScramEngine.hpp/cpp` | 74+132 | Evaluate(), debounce, cooldown | Funcional, 29 reglas |
| RiskRule | `scram/RiskRule.hpp` | 26 | Interfaz virtual pura | Funcional |
| ScramFinding | `scram/RiskRule.hpp:10-15` | 6 | headline, insight, diagnostic, riskDelta | Funcional |
| ScramResult | `scram/ScramEngine.hpp:41-46` | 6 | headline, insight, diagnostics[], riskScore | Funcional |
| ScramFindingState | `scram/ScramEngine.hpp:32-39` | 8 | debounce, cooldown, wasEmitted | Funcional |
| TelemetryEvents | `telemetry/TelemetryEvents.hpp` | 237 | EventType, TelemetrySource, CausalGroup, HysteresisState, TelemetryHealthManager | Funcional |
| TelemetrySource | `telemetry/TelemetryEvents.hpp:57-92` | 36 | state, staleness, failures | Funcional |
| CausalGroup | `telemetry/TelemetryEvents.hpp:113-137` | 25 | rootCause, relatedEvents, emit logic | Funcional |
| HysteresisState | `telemetry/TelemetryEvents.hpp:139-174` | 36 | enter/exit thresholds, hysteresis | Funcional |
| ProcessChangeDetector | `telemetry/state/ProcessChangeDetector.hpp` | 33 | Detect() via EntityTracker | Funcional |
| EntityTracker | `telemetry/state/EntityTracker.hpp` | 66 | UpdateProcesses(), UpdateDrivers(), TrackedProcess/Thread/Driver | Funcional |
| EntityIdentity | `telemetry/contract/EntityIdentity.hpp` | 70 | Process, Thread, Module, Driver, Device, etc. | Funcional |
| ChangeSet | `telemetry/state/ChangeSet.hpp` | 62 | ChangeEvent (Created/Terminated/Modified/StateChanged/ValueChanged) | Funcional |
| ProcessDelta | `telemetry/state/ChangeSet.hpp:11-17` | 7 | cpuDelta, ramDelta, ioDelta, handleDelta | Funcional |
| StateStore | `telemetry/state/StateStore.hpp` | 44 | Update(), TimelineRing, EntityTracker, ChangeSet | Funcional |
| TimelineRing | `telemetry/state/TimelineRing.hpp` | 53 | Ring buffer de Snapshots (64 por defecto) | Funcional |
| UeoCorrelator | `telemetry/state/UeoCorrelator.hpp` | 106 | AddSignal(), Flush(), correlation patterns | Funcional |
| CorrelationEngine | `telemetry/correlation/CorrelationEngine.hpp` | 167 | 16 reglas, Positive/Negative/Lagging/Threshold/Inverse | Funcional |
| CausalChain | `telemetry/correlation/CausalChain.hpp` | 139 | CausalLink, CausalEvent, lag detection | Funcional |
| Normalizer | `telemetry/normalization/Normalizer.hpp` | 178 | NormalizeCpu/Ram/Gpu/Disk/Network/Thermal/Power/Time/Iops | Funcional |
| Validator | `telemetry/normalization/Validator.hpp` | 259 | ValidateCpu/Ram/Gpu/Disk/Network/Thermal/Power/Processes/Scheduler/Security/Audio | Funcional |
| PipelineOrchestrator | `telemetry/PipelineOrchestrator.hpp` | 132 | ProcessSnapshot(), Normalizer+Validator+Correlation+StateStore+Relay | Funcional |
| TelemetryBaselines | `telemetry/TelemetryBaselines.hpp` | 209 | ~190 campos para delta detection | Funcional |
| ChangeDetector | `telemetry/ChangeDetector.hpp` | 129 | Environment-level change detection, callbacks | Funcional |
| LogManager | `logging/LogManager.hpp/cpp` | 143+147 | Push(), dedup, rate limiting, EMA history | Funcional |
| LogEntry | `logging/LogEntry.hpp` | 53 | 16 campos, structured log | Funcional |
| ScramHistoryRing | `telemetry/export/ScramHistoryRing.hpp` | 132 | Ring buffer de ScramEvents, serializacion JSON | Funcional |
| AppState | `ui/AppStateData.hpp` | 74 | snapshot, previousSnapshot, 16+ sub-states | Funcional |
| AppStateGroups | `ui/AppStateGroups.hpp` | 247 | LogState, ScramState, HistoryBuffers, ProcessLifecycleState, NetworkFlowState, etc. | Funcional |
| MetricsWindow | `ui/AppStateGroups.hpp:151-208` | 58 | Windowed aggregation (CPU/RAM/GPU/Net/Disk/Risk) | Funcional |
| IncidentTracker | `ui/AppStateGroups.hpp:211-232` | 22 | BeginIncident(), EndIncident(), correlationId | Funcional |

### 1.2 Collectors existentes

| Collector | Archivo | Campos Snapshot que alimenta |
|-----------|---------|------------------------------|
| SchedulerCollector | `collectors/sys/SchedulerCollector.hpp` | processorQueueLength, contextSwitchesPerSec, interruptsPerSec, threadCounts, processorAffinities |
| OsKernelCollector | `collectors/sys/OsKernelCollector.hpp` | driverCount, driverNames, totalHandles, totalObjects, totalSyscalls, ntStatusErrors, sessionCount, pnpDeviceCount |
| SecurityCollector | `collectors/security/SecurityCollector.hpp` | selfSignatureValid, unsignedDriverCount, suspiciousScriptHosts, uacConsentProcesses, lsassAccessCount, debugPortActive, vmIndicators, hookModulesDetected, peHeaderTamper, scheduledTaskCount |
| PowerCollector | `collectors/power/PowerCollector.hpp` | acLineStatus, batteryFlag, batteryLifePercent, batteryLifeTimeSec, batteryChargeRate, batteryChargeState, batteryChargePercent, batteryWearLevel, batteryCycleCount, batteryTemperature, powerPlanIndex, powerSaverActive, highPerfActive, idlePowerDrawHigh |
| ThermalCollector | `collectors/thermal/ThermalCollector.hpp` | cpuCoreTempC, cpuCoreTempMax, motherboardTempC, vrmTempC, ambientTempC, cpuThrottleTempC, cpuThrottling, fanCount, fanSpeeds, pumpSpeed, pumpPresent, thermalSensorCount, thermalSensorFailures, thermalTrendC, thermalRecoveryCount, thermalHeatSoakIndex, coolingCurveSlope |
| HardwareBoardCollector | `collectors/hw/HardwareBoardCollector.hpp` | smbiosHash, acpiHash, biosVersion, biosManufacturer, voltages, tpmPresent, tpmReady, cmosBatteryOk, sensorPollFailures, pcieErrors, usbResets, sataResets, thunderboltEvents, ecEvents, boardTempHotspot |
| FilesystemCollector | `collectors/fs/FilesystemCollector.hpp` | fsSystem32FileCount, fsDriversFileCount, fsRecycleBinContentCount, fsProgramFilesCount, fsVolumeDirtyBit, fsMountPointCount, fsReparsePointCount, fsSparseFileCount, fsAdsWithDataCount, fsIntegrityFailures, fsUsnJournalId, fsCorruptionWarnings, fsChkdskPending |
| RegistryCollector | `collectors/fs/RegistryCollector.hpp` | regKeyCount*/regValueCount*/regHash* (19 hives), startupFolderCount, envVarCount |
| AudioCollector | `collectors/audio/AudioCollector.hpp` | audioOutputDeviceCount, audioInputDeviceCount, audioMixerCount, audioMasterVolume, audioMasterMuted, audioSampleRate, audioBitsPerSample, audioChannels, audioWaveOutOpen, audioWaveInOpen, audioServiceRunning, audioMicMuted, audioSystemMuted, audioHeadphoneJack, audioSpatialization, audioPlaybackActive, audioRecordingActive, audioDeviceHash, audioFormatHash, audioLatencyMs, audioCodecEvents, audioDecoderErrors |
| ReliabilityCollector | `collectors/sys/ReliabilityCollector.hpp` | sehExceptionCount, unhandledExceptionCount, accessViolationCount, heapCorruptionDetected, assertionFailureCount, stackOverflowCount, deadlockRecoveryCount, processRestartCount, moduleReloadCount, uiFreezeDetected, hangDetected, timeoutExceeded, retryStormDetected, backoffEscalation, circuitBreakerOpen, circuitBreakerClose, fallbackModeEntry, fallbackModeExit, configRollbackDetected, safeModeActive, telemetryDropDetected, watchdogResetDetected, serviceRecoveryAction, crashEventsToday, exceptionLogCount, processHash, moduleHash, mainThreadResponsive, uiResponsivenessMs, threadHealthOk |
| GpuDisplayCollector | `collectors/gpu/GpuDisplayCollector.hpp` | gpuModel, gpuDriverVersion, gpuVramTotalBytes, gpuVramUsedBytes, displayRefreshRateHz, displayWidth, displayHeight, displayBitsPerPel, displayMonitorCount, gpuPowerWatts, gpuFanRpm, hdrEnabled, vsyncEnabled, desktopCompositionEnabled, frameTimeMs, tdrLevel, displayNames |
| NetworkCollectors | `collectors/network/NetworkCollectors.hpp` | netUpBytesPerSec, netDownBytesPerSec, inboundConnections, outboundConnections, dnsPseudo, latencyMs, latencySource, netAdapterAddresses/Gateways/Speeds/OperStatuses/Types, netPrimaryLinkSpeedBps, pingRttMs, dnsResolutionMs, dnsResolutionOk, tcpRetransmits, tcpResets, routeTableHash, proxyEnabled, netAdapterCount, udpConnectionCount, vpnAdapterDescriptions |
| EnvironmentCollector | `telemetry/EnvironmentCollector.cpp` | Modules, drivers, services, startup items |
| ModuleDetector | `telemetry/ModuleDetector.cpp` | Module enumeration |

### 1.3 SCRAM Rules existentes (29)

| Regla | Archivo | Evalua |
|-------|---------|--------|
| AudioMultimediaRule | `scram/AudioMultimediaRule.cpp` | Audio device issues, codec errors |
| CpuIdentityRule | `scram/CpuIdentityRule.cpp` | CPU identity/tampering |
| CpuPressureRule | `scram/CpuPressureRule.cpp` | CPU load pressure |
| DiskStorageRule | `scram/DiskStorageRule.cpp` | Disk health, SMART, queue |
| DisplayConfigRule | `scram/DisplayConfigRule.cpp` | Display configuration changes |
| FilesystemDataRule | `scram/FilesystemDataRule.cpp` | Filesystem anomalies |
| FrameTimeRule | `scram/FrameTimeRule.cpp` | Frame time anomalies |
| GpuDriverRule | `scram/GpuDriverRule.cpp` | GPU driver issues |
| GpuThermalPowerFanRule | `scram/GpuThermalPowerFanRule.cpp` | GPU thermal/power/fan |
| GpuUtilizationRule | `scram/GpuUtilizationRule.cpp` | GPU utilization |
| GpuVramRule | `scram/GpuVramRule.cpp` | GPU VRAM usage |
| HandleObjectRule | `scram/HandleObjectRule.cpp` | Handle/object leaks |
| HardwareSensorsRule | `scram/HardwareSensorsRule.cpp` | Sensor failures |
| MemorySubsystemRule | `scram/MemorySubsystemRule.cpp` | Memory pressure |
| NetworkLatencyRule | `scram/NetworkLatencyRule.cpp` | Network latency |
| NetworkTopologyRule | `scram/NetworkTopologyRule.cpp` | Network topology changes |
| NetworkTrafficRule | `scram/NetworkTrafficRule.cpp` | Network traffic anomalies |
| OsKernelRule | `scram/OsKernelRule.cpp` | OS kernel issues |
| PowerBatteryRule | `scram/PowerBatteryRule.cpp` | Power/battery issues |
| ProcessTriageRule | `scram/ProcessTriageRule.cpp` | Process triage |
| RamExhaustionRule | `scram/RamExhaustionRule.cpp` | RAM exhaustion |
| RegistryConfigRule | `scram/RegistryConfigRule.cpp` | Registry configuration |
| ReliabilityRecoveryRule | `scram/ReliabilityRecoveryRule.cpp` | Reliability/recovery |
| SecurityIntegrityRule | `scram/SecurityIntegrityRule.cpp` | Security integrity |
| TdrStabilityRule | `scram/TdrStabilityRule.cpp` | TDR stability |
| ThermalCoolingRule | `scram/ThermalCoolingRule.cpp` | Thermal/cooling |
| ThermalDriftRule | `scram/ThermalDriftRule.cpp` | Thermal drift |
| ThreadSchedulingRule | `scram/ThreadSchedulingRule.cpp` | Thread scheduling |
| VpnTunnelRule | `scram/VpnTunnelRule.cpp` | VPN tunnel issues |

### 1.4 Subsistemas de AppState

| Sub-estado | Archivo | Funcion |
|------------|---------|---------|
| LogState | `AppStateGroups.hpp:20-32` | Entradas de log, contadores, scroll, filtro, historial EMA |
| ScramState | `AppStateGroups.hpp:35-52` | Headline, risk score, severity tracking, temperature flags |
| HistoryBuffers | `AppStateGroups.hpp:55-62` | Sparklines de CPU/RAM/GPU/Net/Latency |
| NotificationState | `AppStateGroups.hpp:65-69` | Notificaciones toast |
| ProcessLifecycleState | `AppStateGroups.hpp:97-101` | Map de procesos conocidos con seen/gone counts |
| NetworkFlowState | `AppStateGroups.hpp:104-115` | Deteccion de burst en flujos, CS/DPC spikes |
| LatencyState | `AppStateGroups.hpp:117-121` | Ultima latencia emitida |
| ReadyQueueState | `AppStateGroups.hpp:123-128` | Deteccion de cola de procesos elevada |
| DirtyBitState | `AppStateGroups.hpp:130-133` | Tracking de bit sucio |
| MetricsWindow | `AppStateGroups.hpp:151-208` | Agregacion ventana de metricas (min/max/sum) |
| IncidentTracker | `AppStateGroups.hpp:211-232` | Correlacion de incidentes bajo correlationId |
| UeoCorrelator | `UeoCorrelator.hpp` | Correlacion de senales en incidentes |
| ProcessChangeDetector | `ProcessChangeDetector.hpp` | Deteccion de procesos via EntityTracker |

### 1.5 Lo que ya existe y se reutiliza directamente

1. **Snapshot** — El modelo de datos central con ~300 campos. Se reorganizara en sub-structs pero los campos se conservan.
2. **Collectors** — 12+ modulos de recoleccion. Se mantienen, se alimentan al Snapshot reorganizado.
3. **ScramEngine** — Motor de evaluacion con 29 reglas. Se amplia con contexto enriquecido.
4. **RiskRule** — Interfaz base. Se extiende con nueva interfaz ScramRuleV2.
5. **CorrelationEngine** — 16 reglas de correlacion. Se amplia con multi-signal patterns.
6. **CausalChain** — Deteccion de causalidad. Se integra con Timeline.
7. **Normalizer** — Normalizacion de campos. Se conserva tal cual.
8. **Validator** — Validacion de campos. Se conserva tal cual.
9. **PipelineOrchestrator** — Orquestador del pipeline. Se extiende con EventBus y Diff.
10. **StateStore** — Almacenamiento de estado con TimelineRing. Se amplia.
11. **TimelineRing** — Ring buffer de snapshots. Se extiende a Timeline general.
12. **EntityTracker** — Tracking de entidades. Se amplia con historial.
13. **ChangeSet** — Cambios detectados. Se alimenta al EventBus.
14. **UeoCorrelator** — Correlacion basica. Se integra en CorrelationEngine V2.
15. **LogManager** — Logger con dedup/rate-limiting. Se evoluciona con categorias.
16. **LogEntry** — Entrada de log. Se enriquece con correlationId/snapshotId.
17. **ScramHistoryRing** — Historial de findings SCRAM. Se conserva.
18. **TelemetryBaselines** — Baselines para delta detection. Se reutiliza.
19. **ChangeDetector** — Deteccion de cambios ambientales. Se integra.
20. **MetricsWindow** — Agregacion ventaneada. Se reutiliza en Dashboard.
21. **IncidentTracker** — Tracking de incidentes. Se integra en Timeline.
22. **ProcessLifecycleState** — Lifecycle de procesos. Se amplia a ProcessHistory.
23. **TelemetryHealthManager** — Salud de collectors. Se conserva.
24. **HysteresisState** — Histeresis para umbrales. Se reutiliza en anomalias.

### 1.6 Lo que falta por disenar

| Sistema | Estado actual | Necesita |
|---------|---------------|----------|
| Snapshot reorganizado | Flat struct de 300+ campos | Sub-structs por dominio |
| EventBus centralizado | No existe | Publish/subscribe con replay |
| SnapshotDiff | No existe | Comparacion estructurada |
| Timeline general | Solo TimelineRing de snapshots | Timeline de eventos completa |
| Sessions | SessionInfo basico (id, sampleCount) | Sesion completa con persistencia |
| SCRAM 2.0 | ScramFinding basico | Finding enriquecido con evidencia |
| Correlacion avanzada | UeoCorrelator + CorrelationEngine basicos | Multi-signal pattern matching |
| Deteccion de anomalias | No existe | Baseline + desviacion estandar |
| Historial de procesos | ProcessLifecycleState basico | Historia completa por proceso |
| Historial de red | NetworkFlowState basico | Historia por interfaz/conexion |
| Historial de hardware | No existe | Historia por componente |
| Centro de diagnostico | No existe | Diagnostico por categoria |
| Logging evolucionado | LogManager con domain/severity | Categorias, correlationId, snapshotId |
| UI redesign | 6 tabs actuales | 14+ pantallas modulares |
| Dashboard | Coleccion de stats | Panel central con respuestas |
| Acciones | No existe | Capa independiente de acciones |
| Exportacion | ExportLogsJson/Csv basicos | Sesion, timeline, findings |
| Comparacion | No existe | Snapshots, sesiones, procesos |

---

## 2. SNAPSHOT COMO MODELO CENTRAL

### 2.1 Analisis del Snapshot actual

El `Snapshot` actual (`telemetry/Snapshot.hpp`, 413 lineas) es un struct plano con ~300 campos. No tiene sub-structs. Todos los campos estan en el nivel raiz de `struct Snapshot`.

**Campos bien alimentados** (collectors existentes y activos):
- CPU: cpuPct, kernelTimePct, userTimePct, cpuVendor, cpuBrand, cpuCores, cpuLogicalCpus, cpuTscPerSec, cpuBaseClockMhz, estimatedFrequencyMhz, kernelUserRatio, ipcEstimate
- RAM: ramUsedBytes, ramTotalBytes, ramAvailBytes, commitUsedBytes, commitLimitBytes, commitPeakBytes, kernelPoolPagedBytes, kernelPoolNonpagedBytes, systemCacheBytes, pageFaultsDelta, hardPageFaultsDelta, standbyListBytes, modifiedListBytes, freeListBytes, zeroListBytes, workingSetTotalBytes, pagefilePctUsed, commitPressurePct
- GPU: gpuPct, gpuTempC, gpuModel, gpuDriverVersion, gpuVramTotalBytes, gpuVramUsedBytes, gpuPowerWatts, gpuFanRpm, frameTimeMs, tdrLevel
- Disk: diskReadBytesPerSec, diskWriteBytesPerSec, diskTotalBytes, diskFreeBytes, diskPctUsed, diskQueueLength, diskReadLatencyMs, diskWriteLatencyMs, diskReadIops, diskWriteIops, smartHealthOk, nvmeTempC
- Network: netUpBytesPerSec, netDownBytesPerSec, inboundConnections, outboundConnections, latencyMs, dnsResolutionMs, dnsResolutionOk, tcpRetransmits, tcpResets, routeTableHash, proxyEnabled, netAdapterCount, udpConnectionCount, pingRttMs
- Processes: processes vector (ProcessInfo: name, pid, parentPid, sessionId, cpuPct, ramBytes, gpuPct, createTime100ns, status, priority, processGuid)
- Power: acLineStatus, batteryFlag, batteryLifePercent, batteryLifeTimeSec, batteryChargeRate, batteryChargeState, batteryChargePercent, batteryWearLevel, batteryCycleCount, batteryTemperature, powerPlanIndex, powerSaverActive, highPerfActive, balancedActive, modernStandbyActive, sleepStateActive, hibernateActive, cpuPackagePowerCap, systemPowerCap, idlePowerDrawHigh
- Thermal: cpuCoreTempC, cpuCoreTempMax, motherboardTempC, vrmTempC, ambientTempC, cpuThrottleTempC, cpuThrottling, fanCount, fanSpeeds, pumpSpeed, pumpPresent, thermalSensorCount, thermalSensorFailures, thermalTrendC, thermalRecoveryCount, thermalHeatSoakIndex, coolingCurveSlope
- Security: selfSignatureValid, selfHashComputed, unsignedDriverCount, unsignedDriverNames, suspiciousScriptHosts, uacConsentProcesses, lsassAccessCount, debugPortActive, vmIndicators, hookModulesDetected, peHeaderTamper, scheduledTaskCount, suspiciousModules, processPaths, selfExePath

**Campos infrautilizados** (recogidos pero no mostrados en UI ni en SCRAM):
- Registry hashes (19 hives: regKeyCount*/regValueCount*/regHash* = 57 campos)
- Filesystem counts (fsSystem32*, fsDrivers*, fsRecycleBin*, fsProgramFiles*, fsVolumeDirtyBit, etc. = 19 campos)
- Audio details (audioOutputDeviceCount, audioSampleRate, audioBitsPerSample, audioChannels, etc. = 22 campos)
- Reliability counters (sehExceptionCount, unhandledExceptionCount, accessViolationCount, etc. = 30 campos)
- Hardware details (smbiosHash, acpiHash, biosVersion, voltages, tpmPresent, etc. = 15 campos)
- Display details (displayRefreshRateHz, displayWidth, displayHeight, hdrEnabled, vsyncEnabled, etc. = 10 campos)
- Kernel details (totalHandles, totalObjects, totalSyscalls, ntStatusErrors, etc. = 10 campos)

**Campos derivados/redundantes** que deben eliminarse del Snapshot principal:
- `previousPageFaults` / `pageFaultsDelta` — El delta se computa en TelemetryBaselines
- `previousDiskReadBytesPerSec` / `previousDiskWriteBytesPerSec` — Redundantes con TelemetryBaselines
- `previousDiskReadIops` / `previousDiskWriteIops` — Redundantes con TelemetryBaselines
- `cpuBaseClockMhz` / `estimatedFrequencyMhz` — Estimaciones derivadas, computing on demand
- `kernelUserRatio` — Derivado de kernelTimePct/userTimePct, computing on demand
- `ipcEstimate` — Derivado, computing on demand
- `cpuTscDelta` — Derivado, computing on demand
- `diskPctUsed` — Derivado de diskTotalBytes/diskFreeBytes
- `pagefilePctUsed` — Derivado de pageFileUsedBytes/pageFileTotalBytes
- `commitPressurePct` — Derivado de commitUsedBytes/commitLimitBytes

### 2.2 Estructura propuesta: SystemSnapshot

```cpp
struct SystemSnapshot {
    uint64_t id = 0;
    uint64_t timestampNs = 0;        // monotonic
    uint64_t wallTimeMs = 0;          // wall clock
    std::wstring host;
    uint64_t uptimeSeconds = 0;
    int bootPhase = 0;

    CpuState cpu;
    MemoryState memory;
    GpuState gpu;
    StorageState storage;
    NetworkState network;
    ProcessTable processes;
    PowerState power;
    ThermalState thermal;
    SecurityState security;
    FilesystemState filesystem;
    RegistryState registry;
    AudioState audio;
    DisplayState display;
    KernelState kernel;
    ReliabilityState reliability;
    HardwareBoardState hardware;
};
```

### 2.3 Sub-structs propuestos

```cpp
struct CpuState {
    double pct = 0.0;
    double kernelTimePct = 0.0;
    double userTimePct = 0.0;
    char vendor[13] = {};
    char brand[49] = {};
    uint32_t family = 0;
    uint32_t model = 0;
    uint32_t stepping = 0;
    uint32_t cores = 0;
    uint32_t logicalCpus = 0;
    uint32_t htEnabled = 0;
    uint64_t tscPerSec = 0;
    uint32_t microcodeRev = 0;
    uint32_t featuresEdx = 0;
    uint32_t featuresEcx = 0;
    uint32_t extFeatures = 0;
    int processorQueueLength = 0;
    int contextSwitchesPerSec = 0;
    int systemCallsPerSec = 0;
    int interruptsPerSec = 0;
    int processorCount = 0;
    uint64_t totalContextSwitches = 0;
    uint64_t totalInterruptCount = 0;
    uint64_t totalDpcCount = 0;
    uint64_t dpcTimePerSec = 0;
    uint64_t totalIsrCount = 0;
    uint64_t isrTimePerSec = 0;
    int suspendedThreadCount = 0;
    int readyThreadCount = 0;
    int waitingThreadCount = 0;
    int realtimeThreadCount = 0;
    int highPriorityThreadCount = 0;
    std::set<int> processorAffinities;
    int threadCountDelta = 0;
    int threadCreationDelta = 0;
    int threadTerminationDelta = 0;
};

struct MemoryState {
    uint64_t usedBytes = 0;
    uint64_t totalBytes = 0;
    uint64_t availBytes = 0;
    uint64_t commitUsedBytes = 0;
    uint64_t commitLimitBytes = 0;
    uint64_t commitPeakBytes = 0;
    uint64_t kernelPoolPagedBytes = 0;
    uint64_t kernelPoolNonpagedBytes = 0;
    uint64_t systemCacheBytes = 0;
    uint64_t pageFaultsDelta = 0;
    uint64_t hardPageFaultsDelta = 0;
    uint64_t standbyListBytes = 0;
    uint64_t modifiedListBytes = 0;
    uint64_t freeListBytes = 0;
    uint64_t zeroListBytes = 0;
    uint64_t workingSetTotalBytes = 0;
    uint64_t totalWorkingSetBytes = 0;
    uint64_t pageFileUsedBytes = 0;
    uint64_t pageFileTotalBytes = 0;
};

struct GpuState {
    double pct = 0.0;
    int pctValid = 0;
    double tempC = 0.0;
    bool tempEstimated = false;
    std::wstring model;
    std::wstring driverVersion;
    uint64_t vramTotalBytes = 0;
    uint64_t vramUsedBytes = 0;
    double powerWatts = 0.0;
    int fanRpm = 0;
    double frameTimeMs = 0.0;
    int tdrLevel = 3;
};

struct StorageState {
    uint64_t readBytesPerSec = 0;
    uint64_t writeBytesPerSec = 0;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    double pctUsed = 0.0;
    double queueLength = 0.0;
    double readLatencyMs = 0.0;
    double writeLatencyMs = 0.0;
    uint64_t readIops = 0;
    uint64_t writeIops = 0;
    int smartHealthOk = -1;
    double nvmeTempC = 0.0;
    int nvmeTempValid = 0;
    double tempC = 0.0;
    bool tempEstimated = false;
};

struct NetworkState {
    uint64_t upBytesPerSec = 0;
    uint64_t downBytesPerSec = 0;
    int inboundConnections = 0;
    int outboundConnections = 0;
    int udpConnectionCount = 0;
    int dnsPseudo = 0;
    int latencyMs = -1;
    std::wstring latencySource = L"unavailable";
    int pingRttMs = -1;
    int dnsResolutionMs = -1;
    int dnsResolutionOk = -1;
    uint64_t tcpRetransmits = 0;
    uint64_t tcpResets = 0;
    uint64_t routeTableHash = 0;
    int proxyEnabled = 0;
    int adapterCount = 0;
    uint64_t primaryLinkSpeedBps = 0;
    std::set<std::wstring> adapterAddresses;
    std::set<std::wstring> adapterGateways;
    std::set<DWORD> adapterSpeeds;
    std::set<DWORD> adapterOperStatuses;
    std::set<DWORD> adapterTypes;
    std::set<std::wstring> vpnAdapterDescriptions;
    std::vector<NetworkFlow> flows;
};

struct ProcessTable {
    std::vector<ProcessInfo> list;
    int count = 0;
    int threadCount = 0;
    int handleCount = 0;
};

struct PowerState {
    int acLineStatus = -1;
    int batteryFlag = 128;
    int batteryLifePercent = -1;
    long long batteryLifeTimeSec = -1;
    int batteryChargeRate = 0;
    int batteryChargeState = 0;
    int batteryChargePercent = -1;
    int batteryWearLevel = -1;
    int batteryCycleCount = -1;
    int batteryTemperature = -1;
    int powerPlanIndex = -1;
    GUID powerPlanGuid = {};
    int powerSaverActive = 0;
    int highPerfActive = 0;
    int balancedActive = 0;
    int modernStandbyActive = -1;
    int sleepStateActive = 0;
    int hibernateActive = 0;
    int cpuPackagePowerCap = -1;
    int systemPowerCap = -1;
    int idlePowerDrawHigh = 0;
};

struct ThermalState {
    double cpuCoreTempC = 0.0;
    double cpuCoreTempMax = 0.0;
    double motherboardTempC = 0.0;
    double vrmTempC = 0.0;
    double ambientTempC = 0.0;
    double cpuThrottleTempC = 0.0;
    int cpuThrottling = 0;
    int fanCount = 0;
    std::vector<int> fanSpeeds;
    int pumpSpeed = 0;
    int pumpPresent = 0;
    int thermalSensorCount = 0;
    int thermalSensorFailures = 0;
    double thermalTrendC = 0.0;
    int thermalRecoveryCount = 0;
    double thermalHeatSoakIndex = 0.0;
    double coolingCurveSlope = 0.0;
};

struct SecurityState {
    int selfSignatureValid = -1;
    int selfHashComputed = -1;
    int unsignedDriverCount = 0;
    std::set<std::wstring> unsignedDriverNames;
    int suspiciousScriptHosts = 0;
    int uacConsentProcesses = 0;
    int lsassAccessCount = 0;
    int debugPortActive = 0;
    int vmIndicators = 0;
    int hookModulesDetected = 0;
    int peHeaderTamper = 0;
    int scheduledTaskCount = 0;
    std::set<std::wstring> suspiciousModules;
    std::set<std::wstring> processPaths;
    std::wstring selfExePath;
};

struct FilesystemState {
    int system32FileCount = 0;
    int system32HiddenCount = 0;
    int system32SystemCount = 0;
    int driversFileCount = 0;
    int driversHiddenCount = 0;
    int recycleBinContentCount = 0;
    int programFilesCount = 0;
    int volumeDirtyBit = 0;
    std::wstring volumeLabel;
    unsigned long volumeSerial = 0;
    int mountPointCount = 0;
    int reparsePointCount = 0;
    int sparseFileCount = 0;
    int adsWithDataCount = 0;
    int integrityFailures = 0;
    unsigned long long usnJournalId = 0;
    long long lastUsn = 0;
    int corruptionWarnings = 0;
    int chkdskPending = 0;
};

struct RegistryState {
    struct Hive {
        int keyCount = 0;
        int valueCount = 0;
        unsigned long hash = 0;
    };
    Hive run, runOnce, shell, policies, services, drivers;
    Hive firewall, uac, taskSched, com, telemetry, audit;
    Hive env, path, appAssoc, shellExt, defApp, fileAssoc, configFile;
    int startupFolderCount = 0;
    int envVarCount = 0;
    unsigned long regHashEnvPath = 0;
};

struct AudioState {
    int outputDeviceCount = 0;
    int inputDeviceCount = 0;
    int mixerCount = 0;
    DWORD masterVolume = 0;
    int masterMuted = 0;
    DWORD masterVolumeLeft = 0;
    DWORD masterVolumeRight = 0;
    int sampleRate = 0;
    int bitsPerSample = 0;
    int channels = 0;
    int waveOutOpen = 0;
    int waveInOpen = 0;
    int serviceRunning = 1;
    int micMuted = 0;
    int systemMuted = 0;
    int headphoneJack = -1;
    int spatialization = 0;
    int playbackActive = 0;
    int recordingActive = 0;
    unsigned long deviceHash = 0;
    unsigned long formatHash = 0;
    int latencyMs = 0;
    int codecEvents = 0;
    int decoderErrors = 0;
};

struct DisplayState {
    int refreshRateHz = 0;
    int width = 0;
    int height = 0;
    int bitsPerPel = 0;
    int monitorCount = 0;
    int hdrEnabled = 0;
    int vsyncEnabled = 0;
    int desktopCompositionEnabled = 1;
    std::set<std::wstring> displayNames;
};

struct KernelState {
    unsigned long long systemTime100ns = 0;
    unsigned long long uptimeMs = 0;
    unsigned long driverCount = 0;
    std::set<std::wstring> driverNames;
    unsigned long long totalHandles = 0;
    unsigned long long totalObjects = 0;
    unsigned long long totalSyscalls = 0;
    unsigned long long ntStatusErrors = 0;
    unsigned long long registryOps = 0;
    unsigned long sessionCount = 0;
    int currentSessionId = 0;
    unsigned long long ioReadBytesDelta = 0;
    unsigned long long ioWriteBytesDelta = 0;
    unsigned long long pnpDeviceCount = 0;
    int timeChangeDetected = 0;
    int processCount = 0;
    int threadCount = 0;
    int handleCount = 0;
};

struct ReliabilityState {
    int sehExceptionCount = 0;
    int unhandledExceptionCount = 0;
    int accessViolationCount = 0;
    int heapCorruptionDetected = 0;
    int assertionFailureCount = 0;
    int stackOverflowCount = 0;
    int deadlockRecoveryCount = 0;
    int processRestartCount = 0;
    int moduleReloadCount = 0;
    int uiFreezeDetected = 0;
    int hangDetected = 0;
    int timeoutExceeded = 0;
    int retryStormDetected = 0;
    int backoffEscalation = 0;
    int circuitBreakerOpen = 0;
    int circuitBreakerClose = 0;
    int fallbackModeEntry = 0;
    int fallbackModeExit = 0;
    int configRollbackDetected = 0;
    int safeModeActive = 0;
    int telemetryDropDetected = 0;
    int watchdogResetDetected = 0;
    int serviceRecoveryAction = 0;
    int crashEventsToday = 0;
    int exceptionLogCount = 0;
    unsigned long processHash = 0;
    unsigned long moduleHash = 0;
    int mainThreadResponsive = 1;
    int uiResponsivenessMs = 0;
    int threadHealthOk = 1;
};

struct HardwareBoardState {
    unsigned long smbiosHash = 0;
    unsigned long acpiHash = 0;
    std::wstring biosVersion;
    std::wstring biosManufacturer;
    int biosMajorVer = 0;
    int biosMinorVer = 0;
    double voltage12V = 0.0;
    double voltage5V = 0.0;
    double voltage33V = 0.0;
    double voltageVcore = 0.0;
    double voltageVram = 0.0;
    int tpmPresent = 0;
    int tpmReady = 0;
    int tpmVersion = 0;
    int cmosBatteryOk = 1;
    int sensorPollFailures = 0;
    int pcieErrors = 0;
    int usbResets = 0;
    int sataResets = 0;
    int thunderboltEvents = 0;
    int ecEvents = 0;
    double boardTempHotspot = 0.0;
};
```

### 2.4 Campos que deben convertirse en eventos (no quedarse en Snapshot)

| Campo actual | Evento propuesto | Trigger |
|--------------|------------------|---------|
| processes created/terminated | ProcessStarted / ProcessStopped | ProcessChangeDetector |
| driverNames changes | DriverLoaded / DriverUnloaded | Set diff en EntityTracker |
| unsignedDriverCount > 0 | UnsignedDriverDetected | Umbral |
| cpuThrottling > 0 | ThermalThrottlingDetected | Umbral |
| batteryFlag changes | PowerStateChanged | Comparacion con baseline |
| displayRefreshRateHz changes | DisplayModeChanged | Comparacion |
| audioDeviceHash changes | AudioDeviceChanged | Comparacion |
| registry hash changes (por hive) | RegistryChanged | Comparacion por hive |
| fsCorruptionWarnings > 0 | FilesystemCorruptionDetected | Umbral |
| smartHealthOk == 0 | SmartDriveWarning | Umbral |
| selfSignatureValid == 0 | IntegrityViolationDetected | Umbral |
| hookModulesDetected > 0 | HookModuleDetected | Umbral |
| peHeaderTamper > 0 | PeHeaderTamperDetected | Umbral |
| network adapter set changes | NetworkAdapterChanged | Set diff |
| vpnAdapterDescriptions changes | VpnStateChanged | Set diff |
| routeTableHash changes | RouteTableChanged | Comparacion |
| systemTime change detected | SystemTimeChanged | Umbral |
| pnpDeviceCount changes | DeviceConnected / DeviceDisconnected | Delta |
| tdrLevel changes | TdrLevelChanged | Comparacion |
| thermalRecoveryCount changes | ThermalRecovery | Delta |

### 2.5 Mapeo collector → sub-struct

| Collector | Alimenta sub-struct |
|-----------|---------------------|
| SchedulerCollector | CpuState (queue, CS, interrupts, threads) |
| OsKernelCollector | KernelState (drivers, handles, objects, syscalls) |
| SecurityCollector | SecurityState (signature, unsigned drivers, hooks) |
| PowerCollector | PowerState (battery, AC, power plan) |
| ThermalCollector | ThermalState (temps, fans, pump, throttle) |
| HardwareBoardCollector | HardwareBoardState (BIOS, voltages, TPM, PCIe) |
| FilesystemCollector | FilesystemState (file counts, volumes, corruption) |
| RegistryCollector | RegistryState (19 hives, startup, env vars) |
| AudioCollector | AudioState (devices, volume, codec, latency) |
| ReliabilityCollector | ReliabilityState (exceptions, crashes, recovery) |
| GpuDisplayCollector | GpuState + DisplayState (GPU + monitors) |
| NetworkCollectors | NetworkState (adapters, connections, DNS, latency) |
| NtQuerySystemInformation | CpuState (CPU pct, times), MemoryState (RAM), KernelState |
| PerfCounter collectors | CpuState, MemoryState, StorageState |

---

## 3. SISTEMA DE EVENTOS

### 3.1 Modelo de evento

```cpp
enum class EventCategory : uint8_t {
    System, Process, Network, Hardware, Thermal,
    Power, Storage, Security, Configuration,
    User, Scram, Diagnostic, Filesystem, Registry,
    Audio, Display, Driver, Renderer
};

enum class EventSeverity : uint8_t {
    Info = 0, Low = 1, Medium = 2, High = 3, Critical = 4
};

struct SystemEvent {
    uint64_t id = 0;
    uint64_t timestampNs = 0;
    EventCategory category = EventCategory::System;
    EventSeverity severity = EventSeverity::Info;
    std::wstring type;              // "ProcessStarted", "HighCpu", etc.
    std::wstring description;
    std::wstring subsystem;         // "cpu", "gpu", "network", etc.
    int processId = 0;
    std::wstring processName;
    uint64_t snapshotIdBefore = 0;
    uint64_t snapshotIdAfter = 0;
    DWORD threadId = 0;
    std::wstring correlationId;
    std::wstring metadata;          // JSON extra
    bool uiActionable = false;      // Puede generar accion
    bool structuredOnly = false;    // Solo para logging estructurado, no UI
};
```

### 3.2 Eventos predefinidos

```cpp
// === PROCESS ===
ProcessStarted             // Nuevo proceso detectado
ProcessStopped             // Proceso terminado
ProcessCrashed             // Excepcion no manejada / heap corruption
ProcessHighCpu             // CPU > umbral sostenido
ProcessHighMemory          // RAM > umbral sostenido
ProcessPriorityChanged     // Cambio de prioridad
ProcessThreadSpike         // Creacion masiva de hilos
ProcessHandleLeak          // Handles creciendo sin limite

// === NETWORK ===
NetworkConnected           // Nueva conexion activa
NetworkDisconnected        // Conexion terminada
NetworkChanged             // Adaptador/modificaciones
NetworkTrafficSpike        // Trafico inusualmente alto
DnsFailure                 // Resolucion DNS fallo
DnsSlow                    // Resolucion DNS lenta
VpnConnected               // VPN detectada
VpnDisconnected            // VPN perdida
TcpRetransmitSpike         // Retransmisiones elevadas
TcpResetSpike              // Resets TCP elevados

// === HARDWARE ===
DeviceConnected            // PnP device nuevo
DeviceDisconnected         // PnP device removido
DriverLoaded               // Driver cargado
DriverUnloaded             // Driver descargado
DriverChanged              // Driver modificado
UsbDeviceChanged           // Cambio en dispositivos USB
PcieError                  // Error PCIe detectado
SataReset                  // Reset SATA detectado
ThunderboltEvent           // Evento Thunderbolt

// === THERMAL ===
ThermalWarning             // Temperatura en zona de warning
ThermalCritical            // Temperatura critica
ThermalRecovery            // Temperatura recuperada de warning
ThrottlingDetected         // Throttling activo
FanSpeedChanged            // Velocidad de fan cambio
FanSpeedLow                // Fan no responde proporcionalmente

// === POWER ===
PowerStateChanged          // AC/battery cambio
BatteryLow                 // Bateria < 20%
BatteryCritical            // Bateria < 5%
PowerPlanChanged           // Plan de energia cambio
AcConnected                // Conectado a corriente
AcDisconnected             // Desconectado de corriente
BatteryWearHigh            > 20% wear

// === STORAGE ===
DiskWarning                // Cola de disco elevada / latencia alta
DiskCritical               // SMART failing / disco fallo
DiskCorruption             // Corrupcion detectada
NvmeTempHigh               // NVMe temperatura alta
DiskSpaceLow               // Espacio < 10%

// === SECURITY ===
SecurityWarning            // Hallazgo de seguridad
SecurityCritical           // Hallazgo critico
UnsignedDriverDetected     // Driver sin firmar
SuspiciousModule           // Modulo sospechoso
IntegrityFailure           // Fallo de integridad
TamperDetected             // Manipulacion detectada
LsassAccessAnomaly         // Acceso inusual a LSASS
DebugPortActive            // Puerto de debug activo
HookModuleDetected         // Hook/inline hook detectado

// === CONFIGURATION ===
ConfigurationChanged       // Config de Monix cambiada
RegistryChanged            // Hive de registry modificado
EnvironmentVarChanged      // Variable de entorno cambiada
StartupItemChanged         // Item de inicio cambiado

// === SYSTEM ===
SystemTimeChanged          // Hora del sistema cambio
SessionStarted             // Sesion Monix iniciada
SessionStopped             // Sesion Monix detenida
SystemBoot                 // Boot detectado
SystemShutdown             // Shutdown detectado
UptimeMilestone            // Hito de uptime (24h, 7d, 30d)
CollectorDegradado         // Collector en estado Degraded/Error
CollectorRecovered         // Collector recupero estado Healthy

// === SCRAM ===
ScramFindingEmitted        // SCRAM emitio finding
ScramRiskChanged           // Risk score cambio significativamente
ScramIncidentStarted       // Incidente SCRAM iniciado
ScramIncidentEnded         // Incidente SCRAM terminado

// === FILESYSTEM ===
FilesystemWarning          // Advertencia filesystem
FilesystemCorruption       // Corrupcion detectada
ChkdskPending              // Chkdsk pendiente

// === AUDIO ===
AudioDeviceChanged         // Dispositivo de audio cambio
AudioCodecError            // Error de codec
AudioServiceStopped        // Servicio de audio detenido

// === DISPLAY ===
DisplayModeChanged         // Resolucion/frecuencia cambio
TdrLevelChanged            // TDR level modificado

// === RENDERER ===
RendererInitFailed         // Vulkan/OpenGL init fallo
RendererRecovered          // Renderer recupero
ShaderCompilationFailed    // Shader compile fallo
```

### 3.3 EventBus centralizado

```cpp
class EventBus {
public:
    using EventCallback = std::function<void(const SystemEvent&)>;

    // Suscripcion
    uint64_t Subscribe(EventCategory category, EventCallback cb);
    uint64_t Subscribe(const std::wstring& eventType, EventCallback cb);
    uint64_t SubscribeAll(EventCallback cb);
    void Unsubscribe(uint64_t subscriptionId);

    // Emision
    void Emit(SystemEvent event);
    void EmitBatch(std::vector<SystemEvent> events);

    // Historial
    const SystemEvent& Event(uint64_t id) const;
    std::vector<SystemEvent> RecentEvents(int count = 100) const;
    std::vector<SystemEvent> EventsInTimeRange(uint64_t startNs, uint64_t endNs) const;
    std::vector<SystemEvent> EventsByCategory(EventCategory cat, int count = 100) const;
    std::vector<SystemEvent> EventsByProcess(int pid, int count = 100) const;
    std::vector<SystemEvent> EventsByCorrelation(const std::wstring& id) const;

    // Estadisticas
    int EventCount(EventCategory cat) const;
    int EventsPerMinute() const;
    std::map<EventCategory, int> CategoryCounts() const;

    // Config
    void SetMaxEvents(int max);
    void SetReplayBuffer(int seconds);

private:
    std::deque<SystemEvent> events_;         // Historial circular
    std::vector<std::pair<uint64_t, EventCallback>> categorySubs_;
    std::vector<std::pair<std::wstring, EventCallback>> typeSubs_;
    std::vector<std::pair<uint64_t, EventCallback>> globalSubs_;
    uint64_t nextEventId_ = 1;
    uint64_t nextSubId_ = 1;
    int maxEvents_ = 10000;
    mutable std::mutex mutex_;
};
```

### 3.4 Flujo de datos: Snapshot → Diff → EventBus

```
Collector
    ↓
Snapshot (crudo)
    ↓
Normalizer
    ↓
Validator
    ↓
StateStore.Update()
    ├── DetectChanges() → ChangeSet
    └── TimelineRing.Push()
    ↓
SnapshotDiffer.Compute(prev, current)
    ↓
SnapshotDiff
    ↓
EventGenerator.FromDiff(diff)
    ↓
SystemEvent[]  →  EventBus.Emit()
    ↓
Suscriptores: UI, Logger, Timeline, SCRAM, ProcessHistory, etc.
```

### 3.5 Integracion con codigo existente

El **ChangeDetector** actual ya detecta cambios en:
- Modulos (rutas)
- Drivers (nombres)
- Procesos (PIDs)
- Servicios
- Startup items

El **EntityTracker** ya trackea:
- TrackedProcess (con lifecycle, delta, firstSeen/lastSeen)
- TrackedDriver (con lifecycle)
- TrackedThread (con lifecycle)

El **UeoCorrelator** ya correlaciona senales en incidentes con rootCause y severity.

**Plan de integracion:**
1. ChangeDetector callbacks → EventBus.Emit()
2. EntityTracker cambios → EventBus.Emit()
3. UeoCorrelator → Se integra en CorrelationEngine V2
4. CorrelationEngine events → EventBus.Emit()

---

## 4. SNAPSHOT DIFF

### 4.1 Modelo de diff

```cpp
enum class DiffType : uint8_t {
    ValueChanged,        // Campo numerico cambio
    ItemAdded,           // Elemento nuevo en vector/set
    ItemRemoved,         // Elemento eliminado
    StateChanged,        // Cambio de estado categorico
    ThresholdCrossed,    // Valor cruzo umbral
    ConfigChanged,       // Configuracion modificada
    SetChanged           // Set/diff de coleccion
};

struct DiffField {
    const char* path = nullptr;    // "cpu.pct", "network.adapters"
    DiffType type = DiffType::ValueChanged;
    std::wstring previousValue;
    std::wstring currentValue;
    double delta = 0.0;
    double deltaPercent = 0.0;
    EventSeverity severity = EventSeverity::Info;
    bool isSignificant = false;    // Para filtrar ruido
};

struct SnapshotDiff {
    uint64_t id = 0;
    uint64_t timestampNs = 0;
    uint64_t snapshotIdBefore = 0;
    uint64_t snapshotIdAfter = 0;
    std::vector<DiffField> fields;

    bool HasChanges() const { return !fields.empty(); }
    int ChangeCount() const { return static_cast<int>(fields.size()); }
    bool HasSignificantChanges() const;
    std::vector<DiffField> ChangesByPrefix(const char* prefix) const;
    std::vector<DiffField> HighSeverityChanges() const;
    std::vector<DiffField> ThresholdCrossings() const;
};
```

### 4.2 Motor de diff

```cpp
class SnapshotDiffer {
public:
    SnapshotDiff Compute(const SystemSnapshot& a, const SystemSnapshot& b);

    // Configurables
    void SetThreshold(const char* field, double warn, double critical);
    void AddIgnoredField(const char* field);
    void SetMinDeltaPercent(const char* field, double pct);

private:
    void DiffScalar(const char* path, double a, double b, SnapshotDiff& diff,
                    double warnThreshold = 0, double criticalThreshold = 0);
    void DiffBool(const char* path, int a, int b, SnapshotDiff& diff);
    void DiffSet(const char* path,
                 const std::set<std::wstring>& a,
                 const std::set<std::wstring>& b,
                 SnapshotDiff& diff);
    void DiffVector(const char* path,
                    const std::vector<ProcessInfo>& a,
                    const std::vector<ProcessInfo>& b,
                    SnapshotDiff& diff);

    // Sub-differs
    void DiffCpu(const CpuState& a, const CpuState& b, SnapshotDiff& diff);
    void DiffMemory(const MemoryState& a, const MemoryState& b, SnapshotDiff& diff);
    void DiffGpu(const GpuState& a, const GpuState& b, SnapshotDiff& diff);
    void DiffStorage(const StorageState& a, const StorageState& b, SnapshotDiff& diff);
    void DiffNetwork(const NetworkState& a, const NetworkState& b, SnapshotDiff& diff);
    void DiffProcesses(const ProcessTable& a, const ProcessTable& b, SnapshotDiff& diff);
    void DiffPower(const PowerState& a, const PowerState& b, SnapshotDiff& diff);
    void DiffThermal(const ThermalState& a, const ThermalState& b, SnapshotDiff& diff);
    void DiffSecurity(const SecurityState& a, const SecurityState& b, SnapshotDiff& diff);
    void DiffFilesystem(const FilesystemState& a, const FilesystemState& b, SnapshotDiff& diff);
    void DiffRegistry(const RegistryState& a, const RegistryState& b, SnapshotDiff& diff);
    void DiffAudio(const AudioState& a, const AudioState& b, SnapshotDiff& diff);
    void DiffDisplay(const DisplayState& a, const DisplayState& b, SnapshotDiff& diff);
    void DiffKernel(const KernelState& a, const KernelState& b, SnapshotDiff& diff);
    void DiffReliability(const ReliabilityState& a, const ReliabilityState& b, SnapshotDiff& diff);
    void DiffHardware(const HardwareBoardState& a, const HardwareBoardState& b, SnapshotDiff& diff);

    std::unordered_map<std::string, std::pair<double, double>> thresholds_;
    std::unordered_set<std::string> ignored_;
    std::unordered_map<std::string, double> minDeltaPct_;
    uint64_t nextDiffId_ = 1;
};
```

### 4.3 Generador de eventos desde diff

```cpp
class EventGenerator {
public:
    std::vector<SystemEvent> FromDiff(const SnapshotDiff& diff,
                                       uint64_t snapshotIdBefore,
                                       uint64_t snapshotIdAfter);

private:
    // Mapeo de campos del diff a eventos
    struct FieldEventMapping {
        const char* fieldPrefix;
        std::wstring eventType;
        EventCategory category;
        EventSeverity minSeverity;
    };

    std::vector<FieldEventMapping> mappings_;
};
```

### 4.4 Ejemplo de diff

```
SnapshotDiff @ 18:42:17 → 18:42:18
├── cpu.pct: 45.2 → 92.1 (+46.9, +104.2%) [HIGH] [ThresholdCrossed]
├── thermal.cpuCoreTempC: 62.0 → 73.0 (+11.0, +17.7%) [HIGH]
├── cpu.estimatedFrequencyMhz: 3800 → 3200 (-600, -15.8%) [HIGH]
├── thermal.cpuThrottling: 0 → 1 [CRITICAL] [StateChanged]
├── thermal.fanSpeeds: {1200} → {1200} (+0, +0%) [MEDIUM] [No cambio]
├── network.outboundConnections: 42 → 47 (+5) [INFO]
├── processes.list: +1 chrome.exe (PID 12345) [INFO] [ItemAdded]
└── gpu.tempC: 71.0 → 71.0 (+0, +0%) [INFO] [Ignored]
```

---

## 5. TIMELINE

### 5.1 Modelo de timeline

```cpp
struct TimelineEntry {
    uint64_t id = 0;
    uint64_t timestampNs = 0;

    enum class Type {
        Snapshot, Event, Finding, Warning, Error,
        UserAction, SessionStart, SessionEnd, Diagnostic
    } type = Type::Event;

    uint64_t snapshotId = 0;
    uint64_t eventId = 0;
    uint64_t findingId = 0;

    std::wstring label;
    std::wstring detail;
    EventSeverity severity = EventSeverity::Info;
    EventCategory category = EventCategory::System;

    std::wstring correlationId;
    std::vector<uint64_t> relatedEntryIds;
    int processId = 0;
    std::wstring processName;
};

class Timeline {
public:
    // Agregar
    void AddSnapshot(uint64_t tsNs, uint64_t snapshotId, const std::wstring& label);
    void AddEvent(const SystemEvent& event);
    void AddFinding(uint64_t findingId, const ScramFinding& f, uint64_t tsNs);
    void AddUserAction(const std::wstring& action, uint64_t tsNs);
    void AddDiagnostic(const DiagnosticResult& dr, uint64_t tsNs);

    // Consulta
    std::vector<TimelineEntry> EntriesInRange(uint64_t startNs, uint64_t endNs) const;
    std::vector<TimelineEntry> EntriesByCategory(EventCategory cat) const;
    std::vector<TimelineEntry> EntriesBySeverity(EventSeverity min) const;
    std::vector<TimelineEntry> EntriesByProcess(int pid) const;
    std::vector<TimelineEntry> EntriesByCorrelation(const std::wstring& id) const;

    // Busqueda
    std::vector<TimelineEntry> Search(const std::wstring& query) const;

    // Contexto
    std::vector<TimelineEntry> ContextAround(uint64_t entryId,
                                              int before = 5, int after = 5) const;

    // Estadisticas
    int TotalEntries() const;
    int EntriesPerMinute() const;
    std::map<EventCategory, int> EntriesByCategory() const;

    // Persistencia
    void Save(const std::filesystem::path& path) const;
    void Load(const std::filesystem::path& path);

    // Limpieza
    void TrimBefore(uint64_t timestampNs);
    void SetMaxEntries(int max);

private:
    std::vector<TimelineEntry> entries_;
    uint64_t nextEntryId_ = 1;
    int maxEntries_ = 50000;
    mutable std::mutex mutex_;
};
```

### 5.2 Timeline integrada con subsystemas existentes

```
Timeline ← Evento del EventBus
    ├── ProcessStarted/Stopped → Timeline entry
    ├── NetworkChanged → Timeline entry
    ├── ThermalWarning → Timeline entry
    ├── ScramFindingEmitted → Timeline entry
    └── UserAction → Timeline entry

Timeline ← StateStore
    ├── Snapshot new → Timeline entry (cada N samples)
    └── ChangeSet no vacio → Timeline entries

Timeline ← DiagnosticsCenter
    └── DiagnosticResult → Timeline entry
```

---

## 6. SESIONES

### 6.1 Modelo de sesion

```cpp
struct SessionMetadata {
    std::wstring id;                // "SESSION-20260915-184217"
    std::wstring name;              // Nombre descriptivo
    uint64_t startTimeNs = 0;
    uint64_t endTimeNs = 0;
    std::wstring machineName;
    std::wstring osVersion;
    std::wstring monixVersion;
    int totalSnapshots = 0;
    int totalEvents = 0;
    int totalFindings = 0;
    int peakRiskScore = 0;
    std::wstring notes;
};

struct Session {
    SessionMetadata metadata;

    // Datos de la sesion
    std::vector<SystemSnapshot> snapshots;
    std::vector<SystemEvent> events;
    std::vector<ScramEvent> findings;
    std::vector<TimelineEntry> timeline;
    std::map<int, ProcessHistory> processHistories;

    // Resumen
    double avgCpu = 0.0;
    double avgRamPct = 0.0;
    double avgGpu = 0.0;
    int errorCount = 0;
    int warningCount = 0;
    int criticalCount = 0;
    std::vector<std::wstring> topProcesses;
};

class SessionManager {
public:
    // Lifecycle
    Session& StartSession(const std::wstring& name = L"");
    void StopSession();
    void PauseSession();
    void ResumeSession();

    // Acceso
    Session& CurrentSession();
    const Session& CurrentSession() const;
    bool HasActiveSession() const;

    // Persistencia
    void SaveSession(const Session& session, const std::filesystem::path& path);
    Session LoadSession(const std::filesystem::path& path);
    std::vector<SessionMetadata> ListSessions(const std::filesystem::path& dir);

    // Comparacion
    struct SessionComparison {
        std::vector<DiffField> metricDiffs;
        std::vector<std::wstring> processDiffs;
        std::vector<std::wstring> eventDiffs;
        int snapshotCountDiff = 0;
        int eventCountDiff = 0;
        int findingCountDiff = 0;
        std::wstring summary;
    };
    SessionComparison CompareSessions(const Session& a, const Session& b);

    // Exportacion
    void ExportReport(const Session& session, const std::filesystem::path& path);

private:
    std::unique_ptr<Session> current_;
    std::vector<SessionMetadata> history_;
    bool paused_ = false;
};
```

### 6.2 Persistencia de sesiones

```
sessions/
├── SESSION-20260915-184217/
│   ├── meta.json              // SessionMetadata
│   ├── snapshots.bin          // Snapshots serializados
│   ├── events.json            // Eventos
│   ├── findings.json          // SCRAM findings
│   ├── timeline.json          // Timeline
│   ├── processes.json         // Historial de procesos
│   └── report.html            // Reporte exportado
└── SESSION-20260916-090000/
    └── ...
```

---

## 7. SCRAM 2.0

### 7.1 Extension del ScramEngine existente

El `ScramEngine` actual evalua 29 `RiskRule`s contra un `Snapshot` y produce `ScramResult` con headline/insight/diagnostics[]/riskScore.

**SCRAM 2.0 preserva la interfaz existente y agrega:**

### 7.2 Entrada enriquecida

```cpp
struct ScramContext {
    const SystemSnapshot& current;
    const SystemSnapshot* previous = nullptr;
    const SnapshotDiff* diff = nullptr;                    // Nuevo
    const std::vector<SystemEvent>* recentEvents = nullptr; // Nuevo
    const Timeline* timeline = nullptr;                    // Nuevo
    const ProcessHistory* processHistory = nullptr;        // Nuevo
    uint64_t timestampNs = 0;
    int sampleIndex = 0;
};
```

### 7.3 ScramRuleV2 — nueva interfaz

```cpp
struct ScramFindingV2 {
    std::wstring findingId;
    std::wstring headline;
    std::wstring explanation;           // Explicacion detallada
    std::wstring evidence;              // Evidencia estructurada
    std::vector<std::wstring> evidenceItems;
    int riskDelta = 0;
    EventSeverity severity = EventSeverity::Info;

    // Correlacion
    std::wstring correlationId;
    std::vector<std::wstring> relatedFindings;
    std::vector<int> relatedProcessPids;

    // Confianza
    int confidence = 0;                 // 0-100
    std::wstring confidenceReason;

    // Temporalidad
    uint64_t firstObservedNs = 0;
    uint64_t lastObservedNs = 0;
    int observationCount = 0;
    bool isOngoing = false;

    // Recomendacion
    std::wstring recommendation;
    std::vector<std::wstring> suggestedActions;

    // Metadata
    std::wstring ruleName;
    std::wstring subsystem;
};

class ScramRuleV2 {
public:
    virtual ~ScramRuleV2() = default;
    virtual const wchar_t* Name() const = 0;
    virtual void Evaluate(const ScramContext& ctx,
                          std::vector<ScramFindingV2>& findings) = 0;
};
```

### 7.4 Conversion de reglas existentes

Las 29 reglas existentes se mantienen y se adaptan:

```cpp
// Wrapper para reglas existentes
class LegacyRuleAdapter : public ScramRuleV2 {
public:
    LegacyRuleAdapter(std::unique_ptr<RiskRule> legacy)
        : legacy_(std::move(legacy)) {}

    const wchar_t* Name() const override { return legacy_->Name(); }

    void Evaluate(const ScramContext& ctx,
                  std::vector<ScramFindingV2>& findings) override {
        std::vector<ScramFinding> legacyFindings;
        legacy_->Evaluate(ctx.current, ctx.previous, legacyFindings);
        for (const auto& lf : legacyFindings) {
            ScramFindingV2 f;
            f.headline = lf.headline;
            f.explanation = lf.insight;
            f.evidence = lf.diagnostic;
            f.riskDelta = lf.riskDelta;
            f.ruleName = Name();
            f.firstObservedNs = ctx.timestampNs;
            f.lastObservedNs = ctx.timestampNs;
            f.confidence = 50;  // Basico
            findings.push_back(std::move(f));
        }
    }

private:
    std::unique_ptr<RiskRule> legacy_;
};
```

### 7.5 Nuevas reglas V2

```cpp
class CorrelationScramRule : public ScramRuleV2 {
    // Evalua correlacion multi-senal usando SnapshotDiff + EventBus
    // Ejemplo: CPU↑ + Temp↑ + Fan↓ = Thermal issue
};

class TrendScramRule : public ScramRuleV2 {
    // Evalua tendencias en el tiempo usando Timeline
    // Ejemplo: CPU subiendo consistentemente por 5 minutos
};

class AnomalyScramRule : public ScramRuleV2 {
    // Evalua anomalias contra baseline usando AnomalyDetector
    // Ejemplo: Valor 3 desviaciones sobre la media
};

class ProcessHistoryScramRule : public ScramRuleV2 {
    // Evalua historial de procesos
    // Ejemplo: Proceso con memoria creciente sin limite
};
```

### 7.6 Ejemplo de finding V2

```json
{
  "findingId": "FIND-0042",
  "headline": "Possible thermal throttling",
  "explanation": "CPU temperature increased 11°C while frequency decreased 14%. Fan speed did not increase proportionally, suggesting inadequate cooling response.",
  "evidenceItems": [
    "CPU temperature: 62°C → 73°C (+17.7%)",
    "CPU frequency: 3.8GHz → 3.2GHz (-15.8%)",
    "CPU load: 45% → 92% (+104.4%)",
    "Fan speed: 1200 RPM → 1200 RPM (+0%)",
    "Throttling flag: false → true"
  ],
  "riskDelta": 25,
  "severity": "High",
  "confidence": 87,
  "confidenceReason": "Temperature increase correlates with frequency drop and throttling flag. Fan response missing.",
  "firstObservedNs": 1694700000000000000,
  "observationCount": 34,
  "isOngoing": true,
  "recommendation": "Inspect cooling system. Check fan curves. Consider reducing CPU load.",
  "suggestedActions": [
    "Open hardware monitoring panel",
    "Check fan configuration",
    "Review process CPU usage"
  ],
  "ruleName": "ThermalThrottleRule",
  "subsystem": "thermal",
  "relatedProcessPids": [12345],
  "correlationId": "UEO-1042"
}
```

---

## 8. CORRELACION

### 8.1 CorrelationEngine V2

Extiende el `CorrelationEngine` existente (16 reglas de pares de campos) con:

```cpp
struct CorrelationSignal {
    std::wstring type;          // "cpu_spike", "thermal_warning", etc.
    std::wstring detail;
    double value = 0.0;
    uint64_t timestampNs = 0;
    int processId = 0;
    std::wstring processName;
    uint64_t snapshotId = 0;
};

struct CorrelationPattern {
    std::wstring patternId;
    std::wstring name;
    std::wstring description;
    std::vector<std::wstring> requiredSignals;
    std::vector<std::wstring> optionalSignals;
    int minSignals = 2;
    EventSeverity severity = EventSeverity::Medium;
    int confidenceBoost = 0;
    int maxAgeMs = 30000;       // Ventana de correlacion
};

class CorrelationEngineV2 {
public:
    void AddPattern(CorrelationPattern pattern);

    // Alimentar desde EventBus/Diff
    void AddSignal(const CorrelationSignal& signal);
    void Observe(const SystemSnapshot& current, const SystemSnapshot* prev,
                 uint64_t timestampNs);

    // Evaluar
    struct CorrelationResult {
        std::wstring patternId;
        std::wstring name;
        std::wstring description;
        std::vector<CorrelationSignal> matchedSignals;
        int confidence = 0;
        EventSeverity severity;
        std::wstring rootCause;
        std::wstring explanation;
        uint64_t timestampNs = 0;
    };

    std::vector<CorrelationResult> Evaluate();
    void ClearOldSignals(uint64_t olderThanNs);

    // Acceso
    const std::vector<CorrelationSignal>& RecentSignals() const;

private:
    std::vector<CorrelationPattern> patterns_;
    std::vector<CorrelationSignal> signals_;
    std::vector<CorrelationResult> activeCorrelations_;
    uint64_t signalWindowNs_ = 30000000000ULL; // 30 seconds
};
```

### 8.2 Patrones predefinidos

| Patron | Senales requeridas | Senales opcionales | Severidad |
|--------|-------------------|-------------------|-----------|
| thermal_throttle_risk | cpu_high_temp + cpu_freq_drop | fan_no_change + high_cpu_load | High |
| network_degradation | high_latency + high_dns_latency | tcp_retransmits + packet_loss | Medium |
| process_resource_exfil | process_cpu_spike + network_spike | new_process + suspicious_path | High |
| disk_failing | smart_warning + high_latency | io_errors + temp_increase | Critical |
| memory_pressure | high_ram + high_page_faults | commit_pressure + oom_events | High |
| security_anomaly | unsigned_driver + lsass_access | debug_port + hook_modules | High |
| power_instability | voltage_fluctuation + battery_drain | thermal_spike + throttle | Medium |
| process_crash_pattern | heap_corruption + access_violation | ui_freeze + deadlock_recovery | High |
| registry_tampering | registry_changed + unsigned_driver | scheduled_task_change | High |
| gpu_instability | tdr_event + gpu_temp_high | driver_changed + vram_spike | High |

### 8.3 Niveles de evidencia

```
Observacion    → Se detecto un valor/cambio
Correlacion    → Dos o mas observaciones relacionadas temporalmente
Sospecha       → Correlacion que matchea un patron conocido
Evidencia      → Sospecha con confidence > 60%
Conclusion     → Evidencia con confidence > 85% + confirmation
```

---

## 9. DETECCION DE ANOMALIAS

### 9.1 Modelo

```cpp
struct AnomalyBaseline {
    std::wstring field;
    double mean = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
    int sampleCount = 0;
    uint64_t lastUpdateNs = 0;
};

struct Anomaly {
    std::wstring field;
    double value = 0.0;
    double expectedValue = 0.0;
    double deviation = 0.0;        // En desviaciones estandar
    EventSeverity severity = EventSeverity::Info;
    uint64_t timestampNs = 0;
    std::wstring description;
    bool isRecurring = false;
    int occurrenceCount = 0;
};

class AnomalyDetector {
public:
    // Actualizar baseline
    void UpdateBaseline(const std::wstring& field, double value, uint64_t tsNs);

    // Detectar
    std::vector<Anomaly> Detect(const SystemSnapshot& snap, uint64_t tsNs);

    // Configuracion
    void SetThreshold(EventSeverity severity, double standardDeviations);
    void SetMinSamples(int n);
    void EnableField(const std::wstring& field, bool enabled);

    // Persistencia
    void SaveBaselines(const std::filesystem::path& path);
    void LoadBaselines(const std::filesystem::path& path);

    // Consulta
    std::vector<AnomalyBaseline> GetBaselines() const;
    std::vector<Anomaly> RecentAnomalies(int count = 50) const;

private:
    std::unordered_map<std::wstring, AnomalyBaseline> baselines_;
    std::vector<Anomaly> recentAnomalies_;
    double lowThreshold_ = 2.0;
    double mediumThreshold_ = 3.0;
    double highThreshold_ = 4.0;
    int minSamples_ = 30;
    int maxRecent_ = 500;
};
```

### 9.2 Campos a monitorear

| Campo | Tipo de anomalia | Ejemplo |
|-------|-----------------|---------|
| cpu.pct | Spike, trend | CPU sube de 20% a 95% |
| memory.usedBytes | Trend, threshold | RAM crece 500MB/min |
| gpu.tempC | Spike | GPU sube 15°C en 10s |
| network.upBytesPerSec | Spike | Upload sube 10x |
| storage.readBytesPerSec | Spike | Lectura sube 5x |
| network.latencyMs | Spike, trend | Latencia sube 50ms |
| kernel.processCount | Trend | Procesos suben 50 en 1min |
| cpu.contextSwitchesPerSec | Spike | CS sube 3x |
| memory.pageFaultsDelta | Spike | Page faults suben 10x |
| gpu.vramUsedBytes | Trend | VRAM crece sin limite |
| thermal.cpuCoreTempC | Spike | Temp sube 15°C |
| storage.queueLength | Spike | Cola disco > 32 |

### 9.3 Mecanismos deterministas (sin ML)

1. **Desviacion estandar**: Si valor > mean + N*stddev → anomalia
2. **Tendencia lineal**: Si pendiente > umbral por N muestras → trend anomaly
3. **Cambio brusco**: Si |delta| > umbral → spike
4. **Umbral absoluto**: Si valor > critico → critical anomaly
5. **Patron repetitivo**: Si mismo patron se repite > N veces → pattern anomaly
6. **Comparacion con historial**: Si valor > max historico → outlier

---

## 10. HISTORIAL DE PROCESOS

### 10.1 Modelo

```cpp
struct ProcessHistoryEntry {
    uint64_t timestampNs = 0;
    double cpuPct = 0.0;
    uint64_t ramBytes = 0;
    double gpuPct = 0.0;
    int threadCount = 0;
    int handleCount = 0;
    uint64_t readBytes = 0;
    uint64_t writeBytes = 0;
    int priority = 0;
    std::wstring status;
};

struct ProcessHistory {
    int pid = 0;
    std::wstring name;
    std::wstring executablePath;
    int parentPid = 0;
    uint64_t firstSeenNs = 0;
    uint64_t lastSeenNs = 0;
    uint64_t terminatedNs = 0;
    bool isRunning = true;

    std::vector<ProcessHistoryEntry> samples;

    // Estadisticas
    double avgCpu = 0.0;
    double maxCpu = 0.0;
    uint64_t avgRam = 0;
    uint64_t peakRam = 0;
    int totalSamples = 0;

    // Eventos relacionados
    std::vector<uint64_t> relatedEventIds;

    // Red (si se trackea)
    struct NetworkActivity {
        uint64_t timestampNs;
        std::wstring remoteAddress;
        int port;
        std::wstring protocol;
        uint64_t bytesSent;
        uint64_t bytesReceived;
    };
    std::vector<NetworkActivity> networkActivity;
};

class ProcessHistoryManager {
public:
    void RecordSample(const ProcessInfo& process, uint64_t tsNs);
    void RecordTermination(int pid, uint64_t tsNs);

    ProcessHistory GetHistory(int pid) const;
    std::vector<ProcessHistory> GetActiveProcesses() const;
    std::vector<ProcessHistory> GetTerminatedProcesses() const;
    std::vector<ProcessHistory> GetTopCpuProcesses(int count = 10) const;
    std::vector<ProcessHistory> GetTopMemoryProcesses(int count = 10) const;
    std::vector<ProcessHistory> FindByName(const std::wstring& name) const;

    // Persistencia
    void Save(const std::filesystem::path& path);
    void Load(const std::filesystem::path& path);

private:
    std::unordered_map<int, ProcessHistory> histories_;
    int maxSamplesPerProcess_ = 3600; // 1 hora a 1/sample
};
```

### 10.2 Integracion con Timeline y SCRAM

```
ProcessHistoryManager
    ├── RecordSample() → actualiza ProcessHistory
    ├── RecordSample() → si proceso nuevo → EventBus → Timeline
    ├── RecordSample() → si proceso terminado → EventBus → Timeline
    ├── RecordSample() → si CPU > umbral → EventBus → SCRAM
    └── RecordSample() → si RAM > umbral → EventBus → SCRAM
```

### 10.3 Preguntas que responde

- ¿Que ocurrio con este proceso?
- ¿Cuando aparecio?
- ¿Cuando empezo a consumir CPU?
- ¿Cuando abrio conexiones?
- ¿Cuando termino?
- ¿Que cambio justo antes de su caida?
- ¿Cual fue su patron de uso de recursos?

---

## 11. HISTORIAL DE RED

### 11.1 Modelo

```cpp
struct NetworkInterfaceHistory {
    std::wstring name;
    std::wstring macAddress;
    std::vector<std::wstring> ipAddresses;
    uint64_t linkSpeedBps = 0;
    DWORD operStatus = 0;
    DWORD type = 0;

    struct Sample {
        uint64_t timestampNs = 0;
        uint64_t inBytes = 0;
        uint64_t outBytes = 0;
        int connections = 0;
        int errors = 0;
    };
    std::vector<Sample> samples;
    std::vector<uint64_t> relatedEventIds;
};

struct NetworkConnectionRecord {
    int pid = 0;
    std::wstring processName;
    std::wstring localAddress;
    int localPort = 0;
    std::wstring remoteAddress;
    int remotePort = 0;
    std::wstring protocol;
    std::wstring state;
    uint64_t firstSeenNs = 0;
    uint64_t lastSeenNs = 0;
    bool isActive = true;
};

struct NetworkHistory {
    std::vector<NetworkInterfaceHistory> interfaces;
    std::vector<NetworkConnectionRecord> connections;

    struct DnsSample {
        uint64_t timestampNs = 0;
        int resolutionMs = 0;
        bool success = false;
        std::wstring query;
    };
    std::vector<DnsSample> dnsHistory;

    struct LatencySample {
        uint64_t timestampNs = 0;
        int rttMs = 0;
        std::wstring target;
    };
    std::vector<LatencySample> latencyHistory;

    // Eventos
    struct NetworkEvent {
        uint64_t timestampNs = 0;
        std::wstring type;
        std::wstring detail;
        std::wstring adapterName;
    };
    std::vector<NetworkEvent> events;
};
```

### 11.2 Cambios de red detectables

| Cambio | Como se detecta |
|--------|----------------|
| Adaptador nuevo | Set diff de adapterAddresses |
| Adaptador removido | Set diff de adapterAddresses |
| Velocidad cambio | Comparacion de adapterSpeeds |
| Estado cambio | Comparacion de adapterOperStatuses |
| Gateway cambio | Set diff de adapterGateways |
| Ruta cambio | Comparacion de routeTableHash |
| VPN conectada | Set diff de vpnAdapterDescriptions |
| DNS fallo | dnsResolutionOk == 0 |
| Latencia elevada | latencyMs > umbral |
| Retransmisiones | tcpRetransmits delta > umbral |
| Conexiones | inbound/outbound delta |

---

## 12. HISTORIAL DE HARDWARE

### 12.1 Modelo

```cpp
struct HardwareHistory {
    struct CpuSample {
        uint64_t timestampNs = 0;
        double frequencyMhz = 0.0;
        double temperatureC = 0.0;
        int throttling = 0;
        double powerWatts = 0.0;
        double loadPct = 0.0;
    };
    std::vector<CpuSample> cpuHistory;

    struct GpuSample {
        uint64_t timestampNs = 0;
        double temperatureC = 0.0;
        double usagePct = 0.0;
        uint64_t vramUsedBytes = 0;
        double powerWatts = 0.0;
        int fanRpm = 0;
    };
    std::vector<GpuSample> gpuHistory;

    struct StorageSample {
        uint64_t timestampNs = 0;
        uint64_t readBytesPerSec = 0;
        uint64_t writeBytesPerSec = 0;
        double temperatureC = 0.0;
        int smartHealthOk = 0;
        double queueLength = 0.0;
    };
    std::vector<StorageSample> storageHistory;

    struct FanSample {
        uint64_t timestampNs = 0;
        std::vector<int> speeds;
        int pumpSpeed = 0;
    };
    std::vector<FanSample> fanHistory;

    struct PowerSample {
        uint64_t timestampNs = 0;
        int batteryPercent = 0;
        int acLineStatus = 0;
        double voltage12V = 0.0;
        double voltage5V = 0.0;
        double voltage33V = 0.0;
    };
    std::vector<PowerSample> powerHistory;

    struct TemperatureSample {
        uint64_t timestampNs = 0;
        double cpuCoreC = 0.0;
        double gpuC = 0.0;
        double nvmeC = 0.0;
        double motherboardC = 0.0;
        double vrmC = 0.0;
    };
    std::vector<TemperatureSample> temperatureHistory;
};
```

### 12.2 Vista de hardware

Para cada componente:
```
CPU
├── Current: 45% @ 3.8GHz, 62°C
├── History: [grafica de 1 hora]
├── Trends: "Temperatura subiendo 0.5°C/min"
├── Events: "Throttling detectado a las 18:42"
├── Warnings: "Temperatura > 80°C por 34 segundos"
└── Correlations: "CPU↑ ↔ Temp↑ ↔ Fan unchanged"
```

---

## 13. CENTRO DE DIAGNOSTICO

### 13.1 Modelo

```cpp
enum class DiagnosticCategory {
    Cpu, Memory, Gpu, Storage, Network,
    Thermal, Power, Drivers, Windows,
    Security, Hardware, Filesystem, Registry,
    Audio, Display, Process
};

struct DiagnosticResult {
    DiagnosticCategory category;
    std::wstring status;          // "Healthy", "Warning", "Critical"
    std::wstring summary;
    std::vector<std::wstring> evidence;
    std::vector<std::wstring> recommendations;
    std::vector<int> relatedProcessPids;
    std::vector<uint64_t> relatedEventIds;
    int severity = 0;
    uint64_t timestampNs = 0;
};

class DiagnosticsCenter {
public:
    // Ejecutar diagnostico
    DiagnosticResult RunDiagnostic(DiagnosticCategory cat,
                                    const SystemSnapshot& snap);
    std::vector<DiagnosticResult> RunAllDiagnostics(const SystemSnapshot& snap);

    // Diagnostico dirigido
    DiagnosticResult InvestigateProcess(int pid, const ProcessHistory& history);
    DiagnosticResult InvestigateNetworkInterface(const std::wstring& name);
    DiagnosticResult InvestigateStorageDevice(const std::wstring& device);

    // Historial
    std::vector<DiagnosticResult> RecentDiagnostics(int count = 50) const;
    std::vector<DiagnosticResult> ByCategory(DiagnosticCategory cat) const;

    // Reglas
    void AddDiagnosticRule(std::unique_ptr<DiagnosticRule> rule);

private:
    std::vector<std::unique_ptr<DiagnosticRule>> rules_;
    std::vector<DiagnosticResult> history_;
};
```

### 13.2 Reglas de diagnostico

| Categoria | Regla | Ejemplo |
|-----------|-------|---------|
| CPU | CpuLoadDiagnostic | CPU > 90% por > 60s → Warning |
| Memory | MemoryPressureDiagnostic | RAM > 90% + high page faults → Warning |
| Gpu | GpuThermalDiagnostic | GPU temp > 85°C → Critical |
| Storage | StorageHealthDiagnostic | SMART failing → Critical |
| Network | NetworkLatencyDiagnostic | Latency > 200ms por > 30s → Warning |
| Thermal | ThermalThrottleDiagnostic | Throttling activo → Warning |
| Power | BatteryHealthDiagnostic | Wear > 30% → Warning |
| Drivers | DriverIntegrityDiagnostic | Unsigned drivers → Warning |
| Security | SecurityPostureDiagnostic | Multiple indicators → Critical |
| Process | ProcessAnomalyDiagnostic | Resource leak pattern → Warning |

---

## 14. LOGGING EVOLUCIONADO

### 14.1 Categorias

```cpp
enum class LogCategory : uint8_t {
    System, Process, Network, Hardware, Gpu,
    Storage, Thermal, Power, Security, Filesystem,
    Registry, Audio, Display, Driver, Scram,
    Ui, User, Renderer, Kernel, Diagnostics,
    Timeline, Session, Correlation, Anomaly,
    Collector, Pipeline, EventBus
};
```

### 14.2 LogEntry enriquecido

```cpp
struct LogEntryV2 {
    uint64_t id = 0;
    uint64_t timestampNs = 0;
    std::wstring fullTimestamp;
    LogLevel level = LogLevel::Info;
    LogCategory category = LogCategory::System;
    std::wstring subsystem;
    std::wstring domain;
    std::wstring message;
    std::wstring detail;
    std::wstring metadata;        // JSON

    // Correlacion
    uint64_t eventId = 0;
    uint64_t snapshotId = 0;
    std::wstring correlationId;
    int processId = 0;
    DWORD threadId = 0;
    std::wstring sessionId;

    // Dedup
    int repeatCount = 1;
    std::wstring firstTimestamp;
};
```

### 14.3 Evolucion del LogManager actual

El `LogManager` actual ya tiene:
- Buffer circular (deque, 480 entradas)
- Dedup (100 entradas scan)
- Rate limiting (global + por dominio)
- EMA history (warn, err, crit, kernel, net)
- Notificaciones push

**Se agrega:**
- Categorias (LogCategory)
- correlationId
- snapshotId
- Filtro por categoria en UI
- Exportacion por categoria
- Persistencia a disco (opcional)

---

## 15. UI

### 15.1 Pantallas propuestas

```
Tab          | Pantalla        | Descripcion
-------------|-----------------|--------------------------------------------
Dashboard    | Dashboard       | Estado general, alertas, eventos recientes
Processes    | Processes       | Lista de procesos con historial
Network      | Network         | Adaptadores, conexiones, latencia, DNS
Hardware     | Hardware        | CPU, GPU, RAM, Fans, Temperaturas
Storage      | Storage         | Disco, SMART, IOPS, latencia
Thermal      | Thermal         | Temperaturas, fans, throttling
Power        | Power           | Bateria, plan de energia, voltajes
Security     | Security        | Firmas, drivers, hooks, integridad
Diagnostics  | Diagnostics     | Centro de diagnostico por categoria
Timeline     | Timeline        | Cronologia de eventos/foundings
SCRAM        | SCRAM           | Analisis de riesgo, findings, historial
Logs         | Logs            | Log categorizado con filtros
Sessions     | Sessions        | Gestion de sesiones, comparacion
Settings     | Settings        | Configuracion general
```

### 15.2 Principios de diseno

1. **Una sola fuente de verdad**: Todas las pantallas leen de los sistemas centrales (Snapshot, EventBus, Timeline, etc.)
2. **Sin logica duplicada**: Ninguna pantalla implementa su propia logica de monitoreo
3. **Desacoplamiento**: La UI no conoce collectors ni algorithms internos
4. **Reactividad**: La UI se actualiza cuando EventBus emite eventos
5. **Consistencia**: Todas las pantallas usan los mismos componentes de presentacion

### 15.3 Integracion con Win98Theme

El `Win98Theme` existente proporciona:
- Paneles con bordes 3D
- Scrollbars
- Headers de columna
- Toolbars
- Botones con hover/pressed
- Fuentes bitmaps

Se conserva la identidad visual. Se agregan paneles para las nuevas pantallas.

---

## 16. DASHBOARD

### 16.1 Preguntas que responde

- ¿Que esta ocurriendo ahora?
- ¿Que cambio?
- ¿Hay algo importante?
- ¿Que esta degradandose?
- ¿Que acaba de ocurrir?
- ¿Que recomienda Monix?

### 16.2 Layout propuesto

```
┌─────────────────────────────────────────────────────────────┐
│ MONIX DASHBOARD                                  [Sesiones] │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─ESTADO GENERAL──┐  ┌─ALERTAS──────┐  ┌─EVENTOS───────┐ │
│  │ CPU: 45%  RAM:62%│  │ ⚠ Thermal   │  │ 18:42 chrome  │ │
│  │ GPU: 78%  NET:OK │  │ ⚠ Disk 95%  │  │ 18:41 vpn     │ │
│  │ Risk: 23/100     │  │ ℹ 3 findings │  │ 18:40 crash   │ │
│  └──────────────────┘  └──────────────┘  └───────────────┘ │
│                                                             │
│  ┌─TENDENCIAS──────┐  ┌─PROCESOS─────┐  ┌─SCRAM─────────┐ │
│  │ CPU [grafica]   │  │ chrome  45%  │  │ Headline:     │ │
│  │ RAM [grafica]   │  │ game    32%  │  │ "Thermal..."  │ │
│  │ GPU [grafica]   │  │ system  12%  │  │ Risk: 23      │ │
│  │ Net [grafica]   │  │             │  │ Confidence:87%│ │
│  └──────────────────┘  └──────────────┘  └───────────────┘ │
│                                                             │
│  ┌─RED─────────────┐  ┌─HARDWARE─────┐  ┌─RECOMENDACION─┐ │
│  │ Up: 1.2 MB/s    │  │ CPU: 62°C    │  │ "Inspect      │ │
│  │ Down: 3.4 MB/s  │  │ GPU: 71°C    │  │  cooling..."  │ │
│  │ Conn: 42/47     │  │ Fan: 1200RPM │  │               │ │
│  │ DNS: 12ms       │  │ NVMe: 45°C   │  │               │ │
│  └──────────────────┘  └──────────────┘  └───────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 16.3 Fuentes de datos

| Panel | Fuente |
|-------|--------|
| Estado General | Snapshot actual |
| Alertas | EventBus (ultimos N eventos severidad >= Medium) |
| Eventos Recientes | EventBus (ultimos 10 eventos) |
| Tendencias | MetricsWindow (agregacion) |
| Procesos | Snapshot.processes (top 5 CPU) |
| SCRAM | ScramResult actual |
| Red | Snapshot.network |
| Hardware | Snapshot.thermal + Snapshot.gpu |
| Recomendacion | ScramFindingV2.recommendation |

---

## 17. SISTEMA DE ACCIONES

### 17.1 Modelo

```cpp
enum class ActionCategory {
    Process, Network, Configuration, Diagnostic,
    System, Security
};

enum class ActionRisk {
    Safe,           // Solo lectura/informacion
    Moderate,       // Cambio no destructivo
    Destructive,    // Requiere confirmacion
    Critical        // Requiere confirmacion + justificacion
};

struct ActionDefinition {
    std::wstring id;
    std::wstring name;
    std::wstring description;
    ActionCategory category;
    ActionRisk risk;
    std::wstring subsystem;
};

struct ActionResult {
    std::wstring actionId;
    bool success = false;
    std::wstring message;
    std::wstring errorDetail;
    uint64_t timestampNs = 0;
    std::wstring executedBy;     // "user", "system", "auto"
};

class ActionExecutor {
public:
    // Definir acciones disponibles
    void RegisterAction(ActionDefinition def);

    // Ejecutar
    ActionResult Execute(const std::wstring& actionId,
                         const std::map<std::wstring, std::wstring>& params);

    // Confirmacion para acciones destructivas
    bool RequiresConfirmation(const std::wstring& actionId) const;
    ActionResult ExecuteConfirmed(const std::wstring& actionId,
                                   const std::map<std::wstring, std::wstring>& params,
                                   const std::wstring& justification);

    // Historial
    std::vector<ActionResult> History(int count = 50) const;

private:
    std::vector<ActionDefinition> definitions_;
    std::vector<ActionResult> history_;
};
```

### 17.2 Acciones predefinidas

| Accion | Riesgo | Descripcion |
|--------|--------|-------------|
| process.kill | Destructive | Terminar proceso |
| process.suspend | Moderate | Suspender proceso |
| process.resume | Safe | Reanudar proceso |
| process.set_priority | Moderate | Cambiar prioridad |
| network.disconnect | Moderate | Desconectar adaptador |
| network.flush_dns | Safe | Limpiar cache DNS |
| network.reset_tcp | Safe | Resetear conexiones TCP |
| thermal.open_fan_control | Safe | Abrir control de fans |
| diagnostic.run_full | Safe | Ejecutar diagnostico completo |
| diagnostic.run_category | Safe | Ejecutar diagnostico de categoria |
| session.save | Safe | Guardar sesion actual |
| session.export | Safe | Exportar sesion |
| config.export | Safe | Exportar configuracion |
| system.open_device_manager | Safe | Abrir Administrador de dispositivos |
| system.open_event_viewer | Safe | Abrir Visor de eventos |

### 17.3 Seguridad

1. **Nunca conectar botones de UI directamente a APIs destructivas**
2. **Toda accion pasa por ActionExecutor**
3. **Acciones destructivas requieren confirmacion en dialogo**
4. **Toda accion se registra en historial**
5. **Toda accion genera evento en EventBus**
6. **Toda accion aparece en Timeline**

---

## 18. EXPORTACION

### 18.1 Formatos

```cpp
enum class ExportFormat {
    Json, Csv, Txt, Html, Report
};

class Exporter {
public:
    // Snapshots
    void ExportSnapshot(const SystemSnapshot& snap,
                        const std::filesystem::path& path,
                        ExportFormat format);

    // Sesiones
    void ExportSession(const Session& session,
                       const std::filesystem::path& path,
                       ExportFormat format);

    // Timeline
    void ExportTimeline(const Timeline& timeline,
                        const std::filesystem::path& path,
                        ExportFormat format);

    // Eventos
    void ExportEvents(const std::vector<SystemEvent>& events,
                      const std::filesystem::path& path,
                      ExportFormat format);

    // SCRAM findings
    void ExportFindings(const std::vector<ScramFindingV2>& findings,
                        const std::filesystem::path& path,
                        ExportFormat format);

    // Logs
    void ExportLogs(const std::vector<LogEntryV2>& entries,
                    const std::filesystem::path& path,
                    ExportFormat format);

    // Reporte completo de sesion
    void GenerateReport(const Session& session,
                        const std::filesystem::path& path);
};
```

### 18.2 Reporte HTML

```html
<!-- Estructura del reporte -->
<html>
<head><title>Monix Session Report</title></head>
<body>
  <h1>Session: SESSION-20260915-184217</h1>
  <h2>Summary</h2>
  <!-- Duracion, snapshots, eventos, findings -->

  <h2>System Overview</h2>
  <!-- CPU/RAM/GPU/Net stats promediados -->

  <h2>Timeline</h2>
  <!-- Timeline de eventos ordenados cronologicamente -->

  <h2>SCRAM Findings</h2>
  <!-- Findings con evidencia y recomendaciones -->

  <h2>Process History</h2>
  <!-- Top procesos con historial de recursos -->

  <h2>Network History</h2>
  <!-- Conexiones, latencia, DNS -->

  <h2>Hardware History</h2>
  <!-- Temperaturas, fans, voltajes -->

  <h2>Diagnostics</h2>
  <!-- Resultados de diagnostico -->
</body>
</html>
```

---

## 19. COMPARACION

### 19.1 Tipos de comparacion

```cpp
class Comparator {
public:
    // Comparar dos snapshots
    SnapshotDiff CompareSnapshots(const SystemSnapshot& a,
                                   const SystemSnapshot& b);

    // Comparar dos sesiones
    SessionComparison CompareSessions(const Session& a, const Session& b);

    // Comparar dos procesos (mismo nombre, diferentes PIDs/tiempos)
    struct ProcessComparison {
        std::vector<DiffField> resourceDiffs;
        std::vector<DiffField> behaviorDiffs;
        std::wstring summary;
    };
    ProcessComparison CompareProcesses(const ProcessHistory& a,
                                        const ProcessHistory& b);

    // Comparar estados de hardware
    struct HardwareComparison {
        std::vector<DiffField> cpuDiffs;
        std::vector<DiffField> gpuDiffs;
        std::vector<DiffField> thermalDiffs;
        std::vector<DiffField> storageDiffs;
        std::wstring summary;
    };
    HardwareComparison CompareHardware(const SystemSnapshot& a,
                                        const SystemSnapshot& b);

    // Comparar estados de red
    struct NetworkComparison {
        std::vector<DiffField> adapterDiffs;
        std::vector<DiffField> connectionDiffs;
        std::vector<DiffField> latencyDiffs;
        std::wstring summary;
    };
    NetworkComparison CompareNetwork(const SystemSnapshot& a,
                                      const SystemSnapshot& b);

    // Comparar rangos temporales
    struct TimeRangeComparison {
        double avgCpuDiff = 0.0;
        double avgRamDiff = 0.0;
        double avgGpuDiff = 0.0;
        int eventCountDiff = 0;
        int findingCountDiff = 0;
        std::wstring summary;
    };
    TimeRangeComparison CompareTimeRanges(
        const Session& session,
        uint64_t startA, uint64_t endA,
        uint64_t startB, uint64_t endB);
};
```

---

## 20. SISTEMA VISUAL

### 20.1 Desacoplamiento

```
MonixApp (logica)
    ↓
EventBus / StateStore / Timeline
    ↓
UI State (AppState)
    ↓
Render Layer (Win98Theme + Vulkan/OpenGL)
    ↓
Pantalla
```

### 20.2 Identidad visual existente

- **Win98Theme**: Paneles 3D, scrollbars, headers, botones, fuentes bitmap
- **Vulkan renderer**: Back-end grafico con pipeline completo
- **CRT shaders**: Efecto visual retro
- **Transiciones**: Animaciones entre pantallas

### 20.3 Evolucion

1. Conservar Win98Theme como base
2. Agregar componentes nuevos (Timeline visual, graficas de historial)
3. Desacoplar rendering de logica de negocio
4. Permitir cambio de tema sin modificar collectors
5. Componentes reutilizables: SparklineChart, TimelineView, DiffViewer, etc.

---

## 21. ARQUITECTURA FINAL PROPUESTA

### 21.1 Capas

```
┌─────────────────────────────────────────────────────────────┐
│                        UI LAYER                             │
│  Dashboard | Processes | Network | Hardware | Thermal | ... │
│  Timeline | SCRAM | Logs | Sessions | Diagnostics | Settings│
├─────────────────────────────────────────────────────────────┤
│                      CORE LAYER                             │
│  EventBus | Timeline | Sessions | Actions | Export | Config │
├─────────────────────────────────────────────────────────────┤
│                    ANALYSIS LAYER                           │
│  SnapshotDiff | CorrelationEngineV2 | AnomalyDetector       │
│  DiagnosticsCenter | ScramEngine (V1 + V2 rules)            │
├─────────────────────────────────────────────────────────────┤
│                   HISTORY LAYER                             │
│  ProcessHistoryManager | NetworkHistory | HardwareHistory   │
├─────────────────────────────────────────────────────────────┤
│                   COLLECTORS LAYER                          │
│  SchedulerCollector | OsKernelCollector | SecurityCollector │
│  PowerCollector | ThermalCollector | HardwareBoardCollector │
│  FilesystemCollector | RegistryCollector | AudioCollector   │
│  ReliabilityCollector | GpuDisplayCollector | NetworkCollectors│
├─────────────────────────────────────────────────────────────┤
│                    PLATFORM LAYER                           │
│  NtQuerySystemInformation | WMI | Performance Counters      │
│  SetupAPI | Power Management | Sensor APIs                  │
├─────────────────────────────────────────────────────────────┤
│                    RENDER LAYER                             │
│  Win98Theme | Vulkan Renderer | GDI Helpers | Shaders       │
├─────────────────────────────────────────────────────────────┤
│                    PERSISTENCE LAYER                        │
│  Session Files | Log Files | Baseline Files | Config Files  │
└─────────────────────────────────────────────────────────────┘
```

### 21.2 Dependencias permitidas

```
UI → Core, Analysis, History (NUNCA → Collectors, Platform)
Core → Analysis, History (NUNCA → UI, Collectors, Platform)
Analysis → Core (History solo lectura)
History → Core (solo tipos)
Collectors → Platform (solo para recolectar datos)
Collectors → Snapshot (para escribir datos)
Platform → nada (es la capa mas baja)
Render → nada (es independiente)
Persistence → nada (es independiente)
```

### 21.3 Flujo de datos completo

```
1. Platform APIs
   ↓
2. Collectors escriben en SystemSnapshot
   ↓
3. Normalizer normaliza campos
   ↓
4. Validator valida campos
   ↓
5. StateStore.Update()
   ├── DetectChanges() → ChangeSet
   ├── EntityTracker actualiza
   └── TimelineRing.Push()
   ↓
6. SnapshotDiffer.Compute(prev, current) → SnapshotDiff
   ↓
7. EventGenerator.FromDiff(diff) → SystemEvent[]
   ↓
8. EventBus.Emit(event)
   ├── Suscriptores: UI, Logger, Timeline, SCRAM
   ├── ProcessHistoryManager.RecordSample()
   ├── NetworkHistory update
   ├── HardwareHistory update
   └── AnomalyDetector.UpdateBaseline() + Detect()
   ↓
9. CorrelationEngineV2.Observe() + Evaluate()
   ↓
10. ScramEngine.Evaluate(scramContext)
    ├── Reglas V1 (29 existentes)
    └── Reglas V2 (Correlation, Trend, Anomaly, ProcessHistory)
    ↓
11. ScramFindingV2 → EventBus → Timeline → UI
    ↓
12. DiagnosticsCenter.RunAllDiagnostics() (on-demand)
    ↓
13. UI actualiza paneles
    ↓
14. User interaction → ActionExecutor → EventBus → Timeline
```

---

## 22. REORGANIZACION DEL CODIGO

### 22.1 Movimientos propuestos

| Origen | Destino | Motivo | Dependencias | Riesgo | Orden |
|--------|---------|--------|--------------|--------|-------|
| `telemetry/Snapshot.hpp` | `core/Snapshot.hpp` + `core/snapshot/*.hpp` | Reorganizar en sub-structs | Collectors, SCRAM, UI | Alto | 1 |
| `telemetry/TelemetryEvents.hpp` | `events/SystemEvent.hpp` + `events/EventBus.hpp` | Separar tipos de EventBus | Timeline, UI, SCRAM | Medio | 2 |
| `telemetry/state/ChangeSet.hpp` | `events/ChangeSet.hpp` | Mover a eventos | EventBus | Bajo | 3 |
| `telemetry/correlation/` | `analysis/correlation/` | Mover a capa de analisis | SCRAM V2 | Bajo | 4 |
| `telemetry/state/StateStore.hpp` | `core/StateStore.hpp` | Mover a core | EventBus, Diff | Bajo | 5 |
| `telemetry/state/TimelineRing.hpp` | `core/TimelineRing.hpp` | Mover a core | StateStore | Bajo | 6 |
| `scram/ScramEngine.hpp` | `analysis/scram/ScramEngine.hpp` | Mover a analisis | SCRAM V2 | Medio | 7 |
| `scram/RiskRule.hpp` | `analysis/scram/RiskRule.hpp` | Mover a analisis | SCRAM V2 | Bajo | 8 |
| `ui/AppStateGroups.hpp` | `core/AppStateGroups.hpp` | Mover a core | UI | Medio | 9 |
| (nuevo) `events/EventBus.hpp` | `core/EventBus.hpp` | Nuevo | Timeline, UI, SCRAM | N/A | 10 |
| (nuevo) `analysis/SnapshotDiffer.hpp` | `analysis/SnapshotDiffer.hpp` | Nuevo | EventBus | N/A | 11 |
| (nuevo) `analysis/AnomalyDetector.hpp` | `analysis/AnomalyDetector.hpp` | Nuevo | SCRAM, EventBus | N/A | 12 |
| (nuevo) `analysis/DiagnosticsCenter.hpp` | `analysis/DiagnosticsCenter.hpp` | Nuevo | EventBus | N/A | 13 |
| (nuevo) `history/ProcessHistoryManager.hpp` | `history/ProcessHistoryManager.hpp` | Nuevo | EventBus | N/A | 14 |
| (nuevo) `history/NetworkHistory.hpp` | `history/NetworkHistory.hpp` | Nuevo | EventBus | N/A | 15 |
| (nuevo) `history/HardwareHistory.hpp` | `history/HardwareHistory.hpp` | Nuevo | EventBus | N/A | 16 |
| (nuevo) `core/Timeline.hpp` | `core/Timeline.hpp` | Nuevo | EventBus | N/A | 17 |
| (nuevo) `core/SessionManager.hpp` | `core/SessionManager.hpp` | Nuevo | Timeline, EventBus | N/A | 18 |
| (nuevo) `core/ActionExecutor.hpp` | `core/ActionExecutor.hpp` | Nuevo | EventBus | N/A | 19 |
| (nuevo) `core/Exporter.hpp` | `core/Exporter.hpp` | Nuevo | Session, Timeline | N/A | 20 |

### 22.2 Orden de implementacion

1. **Core primero**: Snapshot reorganizado → EventBus → Timeline → Diff
2. **Despues analisis**: AnomalyDetector → CorrelationEngineV2 → ScramEngine V2
3. **Despues historial**: ProcessHistory → NetworkHistory → HardwareHistory
4. **Despues infraestructura**: Sessions → Actions → Export
5. **Finalmente UI**: Dashboard → Pantallas nuevas → Integracion

---

## 23. CMAKE

### 23.1 Organizacion futura de targets

```
monix_platform    — APIs de Windows, WMI, Performance Counters
    ↓
monix_collectors  — Todos los collectors (12+)
    ↓
monix_core        — Snapshot, EventBus, Timeline, StateStore, Config
    ↓
monix_events      — SystemEvent, EventBus, ChangeSet
    ↓
monix_analysis    — SnapshotDiff, CorrelationV2, AnomalyDetector, SCRAM
    ↓
monix_history     — ProcessHistory, NetworkHistory, HardwareHistory
    ↓
monix_storage     — Session persistence, Log persistence, Baselines
    ↓
monix_logging     — LogManager evolucionado
    ↓
monix_renderer    — Vulkan, OpenGL, Win98Theme, Shaders, GDI
    ↓
monix_ui          — Todas las pantallas
    ↓
monix             — Main, integracion final
```

### 23.2 Notas

- CMake actual ya tiene source lists explicitos en `Monix/Monix/src/native/`
- Los targets se definiran gradualmente, no todo de una
- Mantener compilacion unity actual por ahora
- Separar targets solo cuando la dependencia sea clara

---

## 24. TESTING

### 24.1 Tests por capa

| Capa | Test | Tipo |
|------|------|------|
| Snapshot | Sub-struct initialization | Unit |
| Snapshot | Field access patterns | Unit |
| SnapshotDiff | Scalar diff | Unit |
| SnapshotDiff | Set diff (added/removed) | Unit |
| SnapshotDiff | Vector diff (processes) | Unit |
| SnapshotDiff | Threshold crossing | Unit |
| EventBus | Subscribe/Unsubscribe | Unit |
| EventBus | Emit y receive | Unit |
| EventBus | Category filter | Unit |
| EventBus | Time range query | Unit |
| Events | SystemEvent creation | Unit |
| Events | EventGenerator.FromDiff | Unit |
| Timeline | Add entries | Unit |
| Timeline | Query by range | Unit |
| Timeline | Query by category | Unit |
| Timeline | Context around entry | Unit |
| Sessions | Start/Stop/Pause | Unit |
| Sessions | Save/Load | Unit |
| Sessions | Compare sessions | Unit |
| SCRAM | Legacy rule adapter | Unit |
| SCRAM | ScramContext creation | Unit |
| SCRAM | ScramFindingV2 fields | Unit |
| Correlation | Pattern match | Unit |
| Correlation | Multi-signal correlation | Unit |
| Anomaly | Baseline update | Unit |
| Anomaly | Spike detection | Unit |
| Anomaly | Trend detection | Unit |
| ProcessHistory | Record sample | Unit |
| ProcessHistory | Record termination | Unit |
| ProcessHistory | Top CPU query | Unit |
| Collectors | Individual collectors | Unit |
| Persistence | Save/Load sessions | Integration |
| Persistence | Export formats | Integration |

### 24.2 Test de integracion

```
Test: Full Pipeline
1. Crear SystemSnapshot A
2. Crear SystemSnapshot B (con cambios)
3. Ejecutar SnapshotDiffer.Compute(A, B)
4. Verificar SnapshotDiff contiene cambios esperados
5. Ejecutar EventGenerator.FromDiff(diff)
6. Verificar SystemEvents generados
7. Emitir eventos en EventBus
8. Verificar suscriptores reciben eventos
9. Ejecutar ScramEngine.Evaluate(scramContext)
10. Verificar findings generados
11. Verificar Timeline tiene entradas
12. Verificar ProcessHistory actualizado
```

### 24.3 Test de regresion

```
Test: Collector Compatibility
1. Para cada collector existente:
   a. Crear SystemSnapshot vacio
   b. Ejecutar collector
   c. Verificar campos esperados no son cero
   d. Verificar no hay excepciones

Test: SCRAM Legacy Compatibility
1. Para cada RiskRule existente:
   a. Crear ScramContext con datos de prueba
   b. Adaptar con LegacyRuleAdapter
   c. Ejecutar Evaluate
   d. Verificar findings no vacios
   e. Verificar riskDelta > 0
```

---

## 25. PLAN DE IMPLEMENTACION

### FASE 1: Core y Snapshot (2-3 semanas)

**Objetivo**: Reorganizar Snapshot en sub-structs, crear EventBus, crear SnapshotDiff.

**Tareas**:
1. Definir sub-structs en `core/snapshot/` (CpuState, MemoryState, etc.)
2. Adaptar Snapshot existente para usar sub-structs (mantener compatibilidad)
3. Actualizar todos los collectors para escribir a sub-structs
4. Crear EventBus con subscribe/emit/historial
5. Crear SnapshotDiffer con diff por sub-struct
6. Crear EventGenerator que convierta diffs en eventos
7. Integrar EventBus con ChangeDetector callbacks
8. Integrar EventBus con EntityTracker callbacks
9. Tests unitarios para cada componente

**Entregables**:
- `core/snapshot/SystemSnapshot.hpp` (con sub-structs)
- `core/EventBus.hpp`
- `analysis/SnapshotDiffer.hpp`
- `events/EventGenerator.hpp`
- Tests unitarios

### FASE 2: Timeline y Sesiones (1-2 semanas)

**Objetivo**: Crear Timeline general y sistema de sesiones.

**Tareas**:
1. Crear Timeline con add/query/search/context
2. Integrar Timeline con EventBus (eventos → timeline)
3. Integrar Timeline con StateStore (snapshots → timeline)
4. Crear SessionManager con start/stop/pause
5. Crear persistencia de sesiones (JSON/binario)
6. Crear exportacion basica (JSON, CSV)
7. Tests

**Entregables**:
- `core/Timeline.hpp`
- `core/SessionManager.hpp`
- `storage/SessionPersistence.hpp`
- Tests

### FASE 3: SCRAM 2.0 y Correlacion (2 semanas)

**Objetivo**: Enriquecer SCRAM con contexto, crear correlacion avanzada.

**Tareas**:
1. Definir ScramContext y ScramFindingV2
2. Crear LegacyRuleAdapter para las 29 reglas existentes
3. Crear nuevas reglas V2 (Correlation, Trend, Anomaly, ProcessHistory)
4. Extender CorrelationEngine con multi-signal patterns
5. Integrar CausalChain con Timeline
6. Integrar UeoCorrelator en CorrelationEngine V2
7. Tests

**Entregables**:
- `analysis/scram/ScramContext.hpp`
- `analysis/scram/ScramFindingV2.hpp`
- `analysis/scram/LegacyRuleAdapter.hpp`
- Nuevas reglas V2
- `analysis/correlation/CorrelationEngineV2.hpp`
- Tests

### FASE 4: Historial (2 semanas)

**Objetivo**: Crear historial de procesos, red y hardware.

**Tareas**:
1. Crear ProcessHistoryManager con samples y estadisticas
2. Integrar con EventBus (procesos → historial)
3. Crear NetworkHistory con interfaces, conexiones, DNS
4. Crear HardwareHistory con CPU, GPU, storage, fans, power
5. Integrar con Timeline
6. Persistencia de historiales
7. Tests

**Entregables**:
- `history/ProcessHistoryManager.hpp`
- `history/NetworkHistory.hpp`
- `history/HardwareHistory.hpp`
- Tests

### FASE 5: Anomalias y Diagnostico (1-2 semanas)

**Objetivo**: Crear deteccion de anomalias y centro de diagnostico.

**Tareas**:
1. Crear AnomalyDetector con baselines y desviacion estandar
2. Integrar con EventBus (anomalias → eventos)
3. Crear DiagnosticsCenter con reglas por categoria
4. Crear DiagnosticRule interface
5. Implementar reglas de diagnostico basicas
6. Tests

**Entregables**:
- `analysis/AnomalyDetector.hpp`
- `analysis/DiagnosticsCenter.hpp`
- `analysis/DiagnosticRule.hpp`
- Tests

### FASE 6: UI Dashboard (2 semanas)

**Objetivo**: Crear Dashboard y adaptar UI existente.

**Tareas**:
1. Crear panel Dashboard con paneles de estado
2. Integrar Dashboard con EventBus (reactivo)
3. Adaptar pantallas existentes (Log, Tasks, Hardware, Network, SCRAM)
4. Crear componentes reutilizables (SparklineChart, TimelineView)
5. Integrar con Win98Theme
6. Tests visuales

**Entregables**:
- `ui/dashboard/DashboardPanel.hpp`
- Componentes UI reutilizables
- Pantallas adaptadas

### FASE 7: Acciones y Exportacion (1 semana)

**Objetivo**: Crear sistema de acciones y exportacion completa.

**Tareas**:
1. Crear ActionExecutor con registro de acciones
2. Implementar acciones basicas (process, network, diagnostic)
3. Crear Exporter con multiples formatos
4. Crear generador de reportes HTML
5. Integrar acciones con EventBus y Timeline
6. Tests

**Entregables**:
- `core/ActionExecutor.hpp`
- `core/Exporter.hpp`
- `storage/ReportGenerator.hpp`
- Tests

### FASE 8: Refactor Final (1-2 semanas)

**Objetivo**: Limpiar, optimizar, verificar compatibilidad.

**Tareas**:
1. Verificar que todos los tests pasan
2. Optimizar rendimiento (redundancias, allocations)
3. Verificar que UI funciona correctamente
4. Documentar API publica
5. Verificar que build.ps1 funciona
6. Verificar que Monix.exe funciona correctamente

**Entregables**:
- Codigo limpio y funcional
- Tests pasando
- Build funcionando
- Documentacion

---

## RESPUESTAS A LAS 14 PREGUNTAS DEL OBJETIVO

### 1. ¿Que podemos reutilizar?
- Snapshot (300+ campos, reorganizar en sub-structs)
- 12+ Collectors (mantener tal cual, apuntar a sub-structs)
- 29 SCRAM rules (mantener, adaptar con LegacyRuleAdapter)
- CorrelationEngine (16 reglas de pares, extender)
- CausalChain (causalidad temporal, integrar)
- UeoCorrelator (correlacion basica, integrar en V2)
- Normalizer (normalizacion de campos, mantener)
- Validator (validacion de campos, mantener)
- PipelineOrchestrator (orquestador, extender)
- StateStore + TimelineRing (estado + ring buffer, extender)
- EntityTracker + ChangeSet (tracking + cambios, integrar con EventBus)
- LogManager (dedup + rate limiting + EMA, evolucionar)
- TelemetryBaselines (baselines, reutilizar)
- ChangeDetector (cambios ambientales, integrar)
- MetricsWindow (agregacion ventaneada, reutilizar en Dashboard)
- IncidentTracker (correlacion de incidentes, integrar)
- ProcessLifecycleState (lifecycle basico, ampliar a ProcessHistory)
- ScramHistoryRing (historial findings, mantener)
- Win98Theme (identidad visual, conservar)

### 2. ¿Que debemos reorganizar?
- Snapshot: de flat struct a sub-structs por dominio
- TelemetryEvents: separar tipos de EventBus
- ChangeSet: mover a capa de eventos
- CorrelationEngine: mover a capa de analisis
- StateStore: mover a core
- SCRAM: mover a analisis, extender
- AppStateGroups: mover a core

### 3. ¿Que sistemas faltan?
- EventBus centralizado
- SnapshotDiff
- Timeline general
- Sessions completas
- SCRAM 2.0 (enriquecido)
- Correlacion multi-senal
- Deteccion de anomalias
- Historial de procesos/red/hardware
- Centro de diagnostico
- Sistema de acciones
- Exportacion completa
- Dashboard reactivo

### 4. ¿Como deben comunicarse?
- EventBus como bus central
- Snapshot → Diff → EventBus → Timeline/SCRAM/UI
- Collectors → Snapshot (escritura)
- EventBus → suscriptores (lectura)
- StateStore como fuente de verdad para estado actual
- Timeline como fuente de verdad para cronologia

### 5. ¿Cual debe ser el modelo de datos?
- SystemSnapshot con sub-structs por dominio
- SystemEvent con category/severity/correlationId
- SnapshotDiff con DiffField (type/value/delta/severity)
- TimelineEntry con type/severity/correlationId
- ScramFindingV2 con evidence/confidence/recommendation
- Session con metadata + datos completos

### 6. ¿Cual debe ser el flujo de eventos?
```
Collectors → Snapshot → Normalizer → Validator → StateStore
    → SnapshotDiff → EventGenerator → EventBus
    → suscriptores: Timeline, SCRAM, Logger, UI, History
```

### 7. ¿Como almacenamos historico?
- Ring buffers (TimelineRing, ScramHistoryRing) para datos en memoria
- Session files (JSON/binario) para persistencia a disco
- ProcessHistoryManager con maxSamples por proceso
- NetworkHistory con samples por interfaz
- HardwareHistory con samples por componente

### 8. ¿Como funciona Timeline?
- Entradas de tipos: Snapshot, Event, Finding, Warning, UserAction, Diagnostic
- Consulta por rango de tiempo, categoria, severidad, proceso, correlacion
- Busqueda por texto
- Contexto alrededor de una entrada
- Persistencia a disco
- Integrada con EventBus (eventos → entradas)

### 9. ¿Como funciona SCRAM?
- ScramEngine.evaluate(scramContext) ← ScramContext enriquecido
- Context incluye: snapshot actual, anterior, diff, eventos recientes, timeline
- 29 reglas V1 adaptadas con LegacyRuleAdapter
- Nuevas reglas V2: Correlation, Trend, Anomaly, ProcessHistory
- Salida: ScramFindingV2 con evidence, confidence, recommendation

### 10. ¿Como correlacionamos informacion?
- CorrelationEngineV2 con multi-signal patterns
- CausalChain para causalidad temporal
- EventBus con correlationId para agrupar eventos
- IncidentTracker para agrupar durante elevated risk
- Niveles: observacion → correlacion → sospecha → evidencia → conclusion

### 11. ¿Como se integra la UI?
- UI lee de sistemas centrales (EventBus, StateStore, Timeline)
- UI no implementa logica de monitoreo
- Dashboard reactivo con paneles de estado
- Componentes reutilizables (SparklineChart, TimelineView)
- Win98Theme como identidad visual

### 12. ¿Como se mantiene desacoplado?
- Dependencias unidireccionales: UI → Core → Analysis → Collectors → Platform
- EventBus como intermediario
- Collectors no conocen UI
- UI no conoce collectors
- Analysis no conoce UI

### 13. ¿Como se prueba?
- Tests unitarios por componente
- Test de integracion: Snapshot → Diff → Event → SCRAM → Timeline
- Test de regresion: Collectors + SCRAM legacy
- Test visual: UI

### 14. ¿Como lo implementamos por fases?
- FASE 1: Core + Snapshot (2-3 semanas)
- FASE 2: Timeline + Sesiones (1-2 semanas)
- FASE 3: SCRAM 2.0 + Correlacion (2 semanas)
- FASE 4: Historial (2 semanas)
- FASE 5: Anomalias + Diagnostico (1-2 semanas)
- FASE 6: UI Dashboard (2 semanas)
- FASE 7: Acciones + Exportacion (1 semana)
- FASE 8: Refactor Final (1-2 semanas)

**Total estimado: 12-16 semanas**
