# Monix Expansion Design — Technical Specification

**Fecha:** 14 de septiembre de 2026
**Repositorio:** Monix-31ago-stable (HEAD: cebd55a)
**Objetivo:** Diseñar la evolución de Monix hacia una plataforma completa de observación, diagnóstico, análisis e interacción con Windows.

---

## ESTRUCTURA DE DATOS EXISTENTE ANTES DEL DISEÑO

### Lo que ya existe y se reutiliza

| Estructura | Archivo | Campos | Estado |
|------------|---------|--------|--------|
| **Snapshot** | Snapshot.hpp | 400+ campos | Funcional, centro del sistema |
| **ScramEngine** | ScramEngine.hpp/cpp | 29 rules, debounce, cooldown | Funcional |
| **RiskRule** | RiskRule.hpp | Interfaz base para reglas | Funcional |
| **ScramFinding** | RiskRule.hpp | headline, insight, diagnostic, riskDelta | Funcional |
| **TelemetryEvents** | TelemetryEvents.hpp | EventType, TelemetrySource, CausalGroup, HysteresisState | Funcional |
| **ProcessChangeDetector** | ProcessChangeDetector.hpp | Detects process start/stop via EntityTracker | Funcional |
| **UeoCorrelator** | UeoCorrelator.hpp | Correlates signals into incidents | Funcional |
| **TelemetryBaselines** | TelemetryBaselines.hpp | Previous state for delta detection | Funcional |
| **LogManager** | LogManager.hpp/cpp | Buffer, dedup, rate limiting, EMA history | Funcional |
| **LogEntry** | LogEntry.hpp | Structured log entry with domain/severity/module | Funcional |
| **AppStateGroups** | AppStateGroups.hpp | LogState, ScramState, HistoryBuffers, ProcessLifecycleState, etc. | Funcional |
| **IncidentTracker** | AppStateGroups.hpp | Groups events under correlation ID | Funcional |
| **MetricsWindow** | AppStateGroups.hpp | Windowed aggregation for metrics | Funcional |
| **Config** | MonixConfigTypes.hpp | 71+ settings | Funcional |

### Lo que falta por diseñar

| Sistema | Estado | Necesita |
|---------|--------|----------|
| SnapshotDiff | No existe | Comparación estructurada entre snapshots |
| Timeline | No existe | Almacenamiento cronológico de eventos |
| Sessions | No existe | Persistencia de sesiones de monitoreo |
| Event System centralizado | Parcial (UeoCorrelator) | EventBus unificado |
| Correlation Engine | Parcial (UeoCorrelator) | Correlación multi-señal |
| Anomaly Detection | No existe | Detección de anomalías |
| Process History | Parcial (ProcessChangeDetector) | Historia completa por proceso |
| Diagnostics Center | No existe | Centro de diagnóstico |
| Actions System | No existe | Capa de acciones controladas |
| Reports/Export | Parcial (ExportLogsJson/Csv) | Exportación completa |

---

## 1. SNAPSHOT COMO MODELO CENTRAL

### Análisis del Snapshot actual

El `Snapshot` actual (413 líneas) contiene ~300 campos organizados en:

**Campos bien alimentados** (collectors existentes):
- CPU: cpuPct, kernelTimePct, userTimePct, cpuVendor, cpuBrand, cpuCores
- RAM: ramUsedBytes, ramTotalBytes, ramAvailBytes, commitUsedBytes
- GPU: gpuPct, gpuTempC, gpuVramTotalBytes, gpuModel
- Disk: diskReadBytesPerSec, diskWriteBytesPerSec, diskTotalBytes, diskFreeBytes
- Network: netUpBytesPerSec, netDownBytesPerSec, inboundConnections, outboundConnections
- Processes: processes vector (name, pid, cpuPct, ramBytes, gpuPct)
- Power: batteryFlag, batteryLifePercent, acLineStatus
- Thermal: cpuCoreTempC, fanCount, fanSpeeds
- Security: selfSignatureValid, unsignedDriverCount, lsassAccessCount

**Campos infrautilizados** (recogidos pero no mostrados):
- Registry hashes (19 hives)
- Filesystem counts (System32, Drivers, RecycleBin)
- Audio details (sampleRate, bitsPerSample, channels)
- Reliability counters (deadlockRecoveryCount, uiFreezeDetected)
- Hardware details (smbiosHash, acpiHash, voltages)

**Campos duplicados o derivados**:
- previousPageFaults / pageFaultsDelta (redundantes)
- previousDiskReadBytesPerSec / diskReadBytesPerSec (redundantes)
- cpuBaseClockMhz / estimatedFrequencyMhz (estimaciones derivadas)

### Estructura propuesta: SystemSnapshot

```cpp
struct SystemSnapshot {
    uint64_t id;
    uint64_t timestampNs;
    uint64_t wallTimeMs;
    
    CpuSnapshot cpu;           // 20 campos
    MemorySnapshot memory;     // 15 campos
    GpuSnapshot gpu;           // 12 campos
    DiskSnapshot disk;         // 15 campos
    NetworkSnapshot network;   // 20 campos
    ProcessSnapshot processes; // vector + metadata
    PowerSnapshot power;       // 12 campos
    ThermalSnapshot thermal;   // 10 campos
    SecuritySnapshot security; // 15 campos
    FilesystemSnapshot fs;     // 10 campos
    RegistrySnapshot registry; // 19 hives
    AudioSnapshot audio;       // 12 campos
    DisplaySnapshot display;   // 8 campos
    ReliabilitySnapshot rel;   // 12 campos
    HardwareSnapshot hw;       // 10 campos
};
```

### Campos que deben convertirse en eventos

| Campo actual | Evento propuesto | Trigger |
|--------------|------------------|---------|
| processes created/terminated | ProcessStarted/ProcessStopped | Detección por ProcessChangeDetector |
| netAdapterChanges | NetworkChanged | Comparación con TelemetryBaselines |
| driverNames changes | DriverChanged | Set diff |
| unsignedDriverCount > 0 | SecurityWarning | Umbral |
| cpuThrottling > 0 | ThermalWarning | Umbral |
| batteryFlag changes | PowerStateChanged | Comparación |
| displayRefreshRateHz changes | DisplayChanged | Comparación |
| audioDeviceHash changes | AudioDeviceChanged | Comparación |
| registry hash changes | RegistryChanged | Comparación por hive |
| fsCorruptionWarnings > 0 | FilesystemWarning | Umbral |

---

## 2. SISTEMA DE EVENTOS

### Modelo de evento

```cpp
enum class EventCategory : uint8_t {
    System,         // Cambios generales del sistema
    Process,        // Procesos
    Network,        // Red
    Hardware,       // Hardware
    Thermal,        // Temperatura
    Power,          // Energía
    Storage,        // Almacenamiento
    Security,       // Seguridad
    Configuration,  // Configuración
    User,           // Acciones del usuario
    Scram,          // Hallazgos SCRAM
    Diagnostic      // Diagnósticos
};

enum class EventSeverity : uint8_t {
    Info = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Critical = 4
};

struct SystemEvent {
    uint64_t id;
    uint64_t timestampNs;
    EventCategory category;
    EventSeverity severity;
    std::wstring type;          // "ProcessStarted", "HighCpu", etc.
    std::wstring description;
    std::wstring subsystem;     // "cpu", "gpu", "network", etc.
    int processId = 0;          // PID relacionado
    std::wstring processName;
    int snapshotId = 0;         // Snapshot de origen
    int snapshotIdAfter = 0;    // Snapshot posterior
    DWORD threadId = 0;
    std::wstring correlationId; // Para agrupar eventos relacionados
    std::wstring metadata;      // JSON con datos extra
};
```

### Eventos predefinidos

```cpp
// Process
ProcessStarted, ProcessStopped, ProcessCrashed, ProcessHighCpu,
ProcessHighMemory, ProcessPriorityChanged, ProcessTerminated

// Network
NetworkConnected, NetworkDisconnected, NetworkChanged,
DnsFailure, DnsSlow, NetworkTrafficSpike, VpnConnected, VpnDisconnected

// Hardware
DeviceConnected, DeviceDisconnected, DriverChanged,
DriverLoaded, DriverUnloaded, UsbDeviceChanged

// Thermal
ThermalWarning, ThermalCritical, ThermalRecovery,
FanSpeedChanged, ThrottlingDetected

// Power
PowerStateChanged, BatteryLow, BatteryCritical,
PowerPlanChanged, AcConnected, AcDisconnected

// Storage
DiskWarning, DiskCritical, DiskCorruption,
SmartWarning, NvmeTempHigh

// Security
SecurityWarning, SecurityCritical, UnsignedDriverDetected,
SuspiciousModule, IntegrityFailure, TamperDetected

// Configuration
ConfigurationChanged, SettingChanged, ConfigReloaded

// System
SystemTimeChanged, SessionStarted, SessionStopped,
SystemBoot, SystemShutdown, UptimeMilestone

// SCRAM
ScramFindingEmitted, ScramRiskChanged, ScramIncidentStarted, ScramIncidentEnded
```

### EventBus centralizado

```cpp
class EventBus {
public:
    using EventCallback = std::function<void(const SystemEvent&)>;
    
    uint64_t Subscribe(EventCategory category, EventCallback callback);
    uint64_t Subscribe(const std::wstring& eventType, EventCallback callback);
    void Unsubscribe(uint64_t subscriptionId);
    
    void Emit(SystemEvent event);
    
    // Batch emission for performance
    void EmitBatch(std::vector<SystemEvent> events);
    
    // History access
    const std::vector<SystemEvent>& RecentEvents(int count = 100) const;
    std::vector<SystemEvent> EventsInTimeRange(uint64_t startNs, uint64_t endNs) const;
    std::vector<SystemEvent> EventsByCategory(EventCategory category, int count = 100) const;
    std::vector<SystemEvent> EventsByProcess(int pid, int count = 100) const;
    
    // Correlation
    std::vector<SystemEvent> CorrelatedEvents(const std::wstring& correlationId) const;
    
    // Statistics
    int EventCount(EventCategory category) const;
    int EventsPerMinute() const;
    
private:
    std::vector<SystemEvent> events_;
    std::unordered_map<uint64_t, std::pair<EventCategory, EventCallback>> subscribers_;
    std::unordered_map<std::wstring, std::vector<uint64_t>> typeSubscribers_;
    uint64_t nextEventId_ = 1;
    uint64_t nextSubscriptionId_ = 1;
    mutable std::mutex mutex_;
};
```

---

## 3. SNAPSHOT DIFF

### Modelo de diff

```cpp
enum class DiffType : uint8_t {
    ValueChanged,       // Campo numérico cambió
    ItemAdded,          // Elemento nuevo en vector/set
    ItemRemoved,        // Elemento eliminado de vector/set
    StateChanged,       // Cambio de estado categórico
    ThresholdCrossed,   // Valor cruzó un umbral
    ConfigChanged       // Configuración modificada
};

struct SnapshotDiffField {
    std::wstring path;          // "cpu.temperature", "network.adapters"
    DiffType type;
    std::wstring previousValue;
    std::wstring currentValue;
    double delta = 0.0;         // Para valores numéricos
    double deltaPercent = 0.0;
    EventSeverity severity = EventSeverity::Info;
};

struct SnapshotDiff {
    uint64_t timestampNs;
    int snapshotIdA;
    int snapshotIdB;
    std::vector<SnapshotDiffField> fields;
    
    // Convenience accessors
    bool HasChanges() const { return !fields.empty(); }
    int ChangeCount() const { return static_cast<int>(fields.size()); }
    std::vector<SnapshotDiffField> ChangesByCategory(const std::wstring& prefix) const;
    std::vector<SnapshotDiffField> HighSeverityChanges() const;
    bool HasThresholdCrossings() const;
};
```

### Motor de diff

```cpp
class SnapshotDiffer {
public:
    SnapshotDiff Compute(const SystemSnapshot& a, const SystemSnapshot& b);
    
    // Configurable thresholds for ThresholdCrossed events
    void SetThreshold(const std::wstring& field, double warning, double critical);
    
    // Ignore noisy fields
    void AddIgnoredField(const std::wstring& field);
    
private:
    void DiffCpu(const CpuSnapshot& a, const CpuSnapshot& b, SnapshotDiff& diff);
    void DiffMemory(const MemorySnapshot& a, const MemorySnapshot& b, SnapshotDiff& diff);
    void DiffGpu(const GpuSnapshot& a, const GpuSnapshot& b, SnapshotDiff& diff);
    void DiffDisk(const DiskSnapshot& a, const DiskSnapshot& b, SnapshotDiff& diff);
    void DiffNetwork(const NetworkSnapshot& a, const NetworkSnapshot& b, SnapshotDiff& diff);
    void DiffProcesses(const ProcessSnapshot& a, const ProcessSnapshot& b, SnapshotDiff& diff);
    void DiffPower(const PowerSnapshot& a, const PowerSnapshot& b, SnapshotDiff& diff);
    void DiffThermal(const ThermalSnapshot& a, const ThermalSnapshot& b, SnapshotDiff& diff);
    void DiffSecurity(const SecuritySnapshot& a, const SecuritySnapshot& b, SnapshotDiff& diff);
    
    std::unordered_map<std::wstring, std::pair<double, double>> thresholds_;
    std::unordered_set<std::wstring> ignoredFields_;
};
```

### Ejemplo de diff

```
SnapshotDiff @ 18:42:17 → 18:42:18
├── cpu.temperature: 62°C → 73°C (+17.7%) [HIGH]
├── cpu.frequency: 3.8GHz → 3.2GHz (-15.8%) [HIGH]
├── cpu.load: 45% → 92% (+104.4%) [HIGH]
├── process: chrome.exe started (PID 12345) [INFO]
├── network.connections: 42 → 47 (+5) [INFO]
├── gpu.temperature: 71°C → 71°C (+0%) [INFO]
├── fan.speed: 1200 → 1200 (+0%) [MEDIUM] ← No aumentó
└── thermal.throttling: false → true [CRITICAL]
```

---

## 4. TIMELINE

### Modelo de timeline

```cpp
struct TimelineEntry {
    uint64_t id;
    uint64_t timestampNs;
    enum class Type {
        Snapshot,
        Event,
        Finding,        // SCRAM finding
        Warning,
        Error,
        UserAction,
        SessionStart,
        SessionEnd
    } type;
    
    int snapshotId = 0;           // Para entradas tipo Snapshot
    int eventId = 0;              // Para entradas tipo Event
    int findingId = 0;            // Para entradas tipo Finding
    
    std::wstring label;           // Resumen legible
    std::wstring detail;          // Detalle extendido
    EventSeverity severity;
    EventCategory category;
    
    // Para agrupación
    std::wstring correlationId;
    std::vector<uint64_t> relatedEntryIds;
};

class Timeline {
public:
    // Agregar entradas
    void AddSnapshot(uint64_t timestampNs, int snapshotId, const std::wstring& label);
    void AddEvent(const SystemEvent& event);
    void AddFinding(int findingId, const ScramFinding& finding, uint64_t timestampNs);
    void AddUserAction(const std::wstring& action, uint64_t timestampNs);
    
    // Consulta
    std::vector<TimelineEntry> EntriesInRange(uint64_t startNs, uint64_t endNs) const;
    std::vector<TimelineEntry> EntriesByCategory(EventCategory category) const;
    std::vector<TimelineEntry> EntriesBySeverity(EventSeverity minSeverity) const;
    std::vector<TimelineEntry> EntriesByProcess(int pid) const;
    std::vector<TimelineEntry> EntriesByCorrelation(const std::wstring& id) const;
    
    // Búsqueda
    std::vector<TimelineEntry> Search(const std::wstring& query) const;
    
    // Contexto
    std::vector<TimelineEntry> ContextAround(uint64_t entryId, int beforeCount = 5, int afterCount = 5) const;
    
    // Comparación
    std::vector<TimelineEntry> CompareTimeRanges(uint64_t startA, uint64_t endA, uint64_t startB, uint64_t endB) const;
    
    // Persistencia
    void Save(const std::filesystem::path& path) const;
    void Load(const std::filesystem::path& path);
    
    // Estadísticas
    int TotalEntries() const;
    int EntriesPerMinute() const;
    std::map<EventCategory, int> EntriesByCategory() const;
    
private:
    std::vector<TimelineEntry> entries_;
    uint64_t nextEntryId_ = 1;
    mutable std::mutex mutex_;
};
```

---

## 5. SESIONES

### Modelo de sesión

```cpp
struct SessionMetadata {
    std::wstring id;
    std::wstring name;
    uint64_t startTimeNs;
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
    std::vector<SystemSnapshot> snapshots;
    std::vector<SystemEvent> events;
    std::vector<ScramFinding> findings;
    std::vector<TimelineEntry> timeline;
    
    // Resumen
    double avgCpu = 0.0;
    double avgRam = 0.0;
    double avgGpu = 0.0;
    int errorCount = 0;
    int warningCount = 0;
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
    
    // Persistencia
    void SaveSession(const Session& session, const std::filesystem::path& path);
    Session LoadSession(const std::filesystem::path& path);
    std::vector<SessionMetadata> ListSessions(const std::filesystem::path& directory);
    
    // Comparación
    struct SessionComparison {
        std::vector<SnapshotDiffField> metricDiffs;
        std::vector<std::wstring> processDiffs;
        std::vector<std::wstring> networkDiffs;
        std::vector<std::wstring> eventDiffs;
        std::wstring summary;
    };
    SessionComparison CompareSessions(const Session& a, const Session& b);
    
    // Exportación
    void ExportSessionReport(const Session& session, const std::filesystem::path& path);
    
private:
    std::unique_ptr<Session> current_;
    std::vector<SessionMetadata> history_;
};
```

---

## 6. SCRAM 2.0

### Extensión del ScramEngine existente

El ScramEngine actual evalúa 29 reglas contra un Snapshot y produce findings con headline/insight/diagnostic/riskDelta.

**SCRAM 2.0 extiende esto con:**

### Entrada enriquecida

```cpp
struct ScramContext {
    const SystemSnapshot& current;
    const SystemSnapshot* previous;
    const SnapshotDiff* diff;           // Nuevo: diff entre snapshots
    const std::vector<SystemEvent>* recentEvents;  // Nuevo: eventos recientes
    const Timeline* timeline;           // Nuevo: acceso a timeline
    uint64_t timestampNs;
};
```

### Salida enriquecida

```cpp
struct ScramFindingV2 {
    std::wstring findingId;            // "FIND-001"
    std::wstring headline;             // "Possible thermal throttling"
    std::wstring explanation;          // Explicación detallada
    std::wstring evidence;             // Evidencia estructurada
    std::vector<std::wstring> evidenceItems;  // Lista de evidencias
    int riskDelta = 0;
    EventSeverity severity;
    
    // Correlación
    std::wstring correlationId;
    std::vector<std::wstring> relatedFindings;
    std::vector<int> relatedProcessPids;
    std::vector<std::wstring> relatedSnapshots;
    
    // Confianza
    int confidence = 0;                // 0-100
    std::wstring confidenceReason;     // Por qué esta confianza
    
    // Temporalidad
    uint64_t firstObservedNs;
    uint64_t lastObservedNs;
    int observationCount = 0;
    bool isOngoing = false;
    
    // Recomendación
    std::wstring recommendation;
    std::vector<std::wstring> suggestedActions;
    
    // Metadata
    std::wstring ruleName;             // Regla que lo generó
    std::wstring subsystem;
};
```

### Reglas extendidas

```cpp
class CorrelationRule : public RiskRule {
    // Evalúa correlación multi-señal
    // Ejemplo: CPU↑ + Temp↑ + Fan↓ = Thermal issue
};

class TrendRule : public RiskRule {
    // Evalúa tendencias en el tiempo
    // Ejemplo: CPU subiendo consistentemente
};

class AnomalyRule : public RiskRule {
    // Evalúa anomalías contra baseline
    // Ejemplo: Valor 3 desviaciones sobre la media
};
```

### Ejemplo de finding V2

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
  "lastObservedNs": 1694700034000000000,
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

## 7. CORRELACIÓN

### Modelo de correlación

```cpp
struct CorrelationSignal {
    std::wstring type;          // "cpu_spike", "thermal_warning", etc.
    std::wstring detail;
    double value = 0.0;
    uint64_t timestampNs = 0;
    int processId = 0;
    std::wstring processName;
    int snapshotId = 0;
};

struct CorrelationPattern {
    std::wstring patternId;
    std::wstring name;          // "thermal_throttle_risk"
    std::wstring description;
    std::vector<std::wstring> requiredSignals;  // Señales que deben estar presentes
    std::vector<std::wstring> optionalSignals;  // Señales que refuerzan
    int minSignals = 2;         // Mínimo de señales para match
    EventSeverity severity;
    int confidenceBoost = 0;    // Boost por cada señal opcional
};

class CorrelationEngine {
public:
    // Agregar patrones
    void AddPattern(CorrelationPattern pattern);
    
    // Alimentar señales
    void AddSignal(const CorrelationSignal& signal);
    
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
    };
    
    std::vector<CorrelationResult> Evaluate();
    
    // Acceso a historial de señales
    std::vector<CorrelationSignal> RecentSignals(uint64_t windowNs) const;
    void ClearOldSignals(uint64_t olderThanNs);
    
private:
    std::vector<CorrelationPattern> patterns_;
    std::vector<CorrelationSignal> signals_;
    std::vector<CorrelationResult> activeCorrelations_;
    uint64_t signalWindowNs_ = 30000000000ULL; // 30 seconds
};
```

### Patrones predefinidos

| Patrón | Señales requeridas | Señales opcionales | Severidad |
|--------|-------------------|-------------------|-----------|
| thermal_throttle_risk | cpu_high_temp + cpu_freq_drop | fan_no_change + high_cpu_load | High |
| network_degradation | high_latency + high_dns_latency | tcp_retransmits + packet_loss | Medium |
| process_resource_exfil | process_cpu_spike + network_spike | new_process + suspicious_path | High |
| disk_failing | smart_warning + high_latency | io_errors + temp_increase | Critical |
| memory_pressure | high_ram + high_page_faults | commit_pressure + oom_events | High |
| security_anomaly | unsigned_driver + lsass_access | debug_port + hook_modules | High |
| power_instability | voltage_fluctuation + battery_drain | thermal_spike + throttle | Medium |

---

## 8. ANOMALÍAS

### Modelo de detección

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
    double value;
    double expectedValue;
    double deviation;          // En desviaciones estándar
    EventSeverity severity;
    uint64_t timestampNs;
    std::wstring description;
    bool isRecurring = false;
    int occurrenceCount = 0;
};

class AnomalyDetector {
public:
    // Actualizar baseline
    void UpdateBaseline(const std::wstring& field, double value, uint64_t timestampNs);
    
    // Detectar anomalías
    std::vector<Anomaly> Detect(const SystemSnapshot& snapshot, uint64_t timestampNs);
    
    // Configuración
    void SetThreshold(EventSeverity severity, double standardDeviations);
    void SetMinSamplesForDetection(int minSamples);
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
    double lowThreshold_ = 2.0;    // 2σ = Low
    double mediumThreshold_ = 3.0; // 3σ = Medium
    double highThreshold_ = 4.0;   // 4σ = High
    int minSamples_ = 30;
    uint64_t anomalyWindowNs_ = 300000000000ULL; // 5 minutes
};
```

### Campos a monitorear

| Campo | Tipo de anomalía | Ejemplo |
|-------|-----------------|---------|
| cpuPct | Spike, trend | CPU sube de 20% a 95% |
| ramUsedBytes | Trend, threshold | RAM crece 500MB/min |
| gpuTempC | Spike | GPU sube 15°C en 10s |
| netUpBytesPerSec | Spike | Upload sube 10x |
| diskReadBytesPerSec | Spike | Lectura sube 5x |
| latencyMs | Spike, trend | Latencia sube 50ms |
| processCount | Trend | Procesos suben 50 en 1min |
| contextSwitchesPerSec | Spike | CS sube 3x |
| pageFaultsDelta | Spike | Page faults suben 10x |

---

## 9. PROCESOS

### Historia por proceso

```cpp
struct ProcessHistoryEntry {
    uint64_t timestampNs;
    double cpuPct;
    uint64_t ramBytes;
    double gpuPct;
    int threadCount;
    int handleCount;
    uint64_t readBytes;
    uint64_t writeBytes;
    int priority;
    std::wstring status;
};

struct ProcessHistory {
    int pid;
    std::wstring name;
    std::wstring executablePath;
    int parentPid;
    uint64_t firstSeenNs;
    uint64_t lastSeenNs;
    uint64_t terminatedNs = 0;
    bool isRunning = true;
    
    std::vector<ProcessHistoryEntry> samples;
    
    // Estadísticas agregadas
    double avgCpu = 0.0;
    double maxCpu = 0.0;
    uint64_t avgRam = 0;
    uint64_t peakRam = 0;
    int totalSamples = 0;
    
    // Eventos del proceso
    std::vector<uint64_t> relatedEventIds;
    
    // Resumen
    std::wstring GetSummary() const;
    std::wstring GetTimeline() const;
};

class ProcessHistoryManager {
public:
    void RecordSample(const ProcessInfo& process, uint64_t timestampNs);
    void RecordTermination(int pid, uint64_t timestampNs);
    
    ProcessHistory GetHistory(int pid) const;
    std::vector<ProcessHistory> GetActiveProcesses() const;
    std::vector<ProcessHistory> GetTerminatedProcesses() const;
    std::vector<ProcessHistory> GetTopCpuProcesses(int count = 10) const;
    std::vector<ProcessHistory> GetTopMemoryProcesses(int count = 10) const;
    
    // Query
    std::vector<ProcessHistory> FindProcesses(const std::wstring& name) const;
    std::vector<ProcessHistory> ProcessesWithEventCorrelation(const std::wstring& correlationId) const;
    
    // Persistencia
    void Save(const std::filesystem::path& path);
    void Load(const std::filesystem::path& path);
    
private:
    std::unordered_map<int, ProcessHistory> histories_;
    int maxSamplesPerProcess_ = 3600; // 1 hour at 1/sec
};
```

---

## 10. RED

### Historial de red

```cpp
struct NetworkInterfaceHistory {
    std::wstring name;
    std::wstring macAddress;
    std::vector<std::wstring> ipAddresses;
    uint64_t linkSpeedBps;
    DWORD operStatus;
    DWORD type;
    
    struct Sample {
        uint64_t timestampNs;
        uint64_t inBytes;
        uint64_t outBytes;
        int connections;
        int errors;
    };
    std::vector<Sample> samples;
    
    // Eventos
    std::vector<uint64_t> relatedEventIds;
};

struct NetworkConnectionHistory {
    int pid;
    std::wstring processName;
    std::wstring localAddress;
    int localPort;
    std::wstring remoteAddress;
    int remotePort;
    std::wstring protocol;
    std::wstring state;
    uint64_t firstSeenNs;
    uint64_t lastSeenNs;
    bool isActive = true;
};

struct NetworkHistory {
    std::vector<NetworkInterfaceHistory> interfaces;
    std::vector<NetworkConnectionHistory> connections;
    
    // DNS history
    struct DnsSample {
        uint64_t timestampNs;
        int resolutionMs;
        bool success;
        std::wstring query;
    };
    std::vector<DnsSample> dnsHistory;
    
    // Latency history
    struct LatencySample {
        uint64_t timestampNs;
        int rttMs;
        std::wstring target;
    };
    std::vector<LatencySample> latencyHistory;
};
```

---

## 11. HARDWARE

### Historial de hardware

```cpp
struct HardwareHistory {
    // CPU history
    struct CpuSample {
        uint64_t timestampNs;
        double frequencyMhz;
        double temperatureC;
        int throttling;
        double powerWatts;
    };
    std::vector<CpuSample> cpuHistory;
    
    // GPU history
    struct GpuSample {
        uint64_t timestampNs;
        double temperatureC;
        double usagePct;
        uint64_t vramUsedBytes;
        double powerWatts;
        int fanRpm;
    };
    std::vector<GpuSample> gpuHistory;
    
    // Storage history
    struct StorageSample {
        uint64_t timestampNs;
        uint64_t readBytesPerSec;
        uint64_t writeBytesPerSec;
        double temperatureC;
        int smartHealthOk;
    };
    std::vector<StorageSample> storageHistory;
    
    // Fan history
    struct FanSample {
        uint64_t timestampNs;
        std::vector<int> speeds;
        int pumpSpeed;
    };
    std::vector<FanSample> fanHistory;
    
    // Power history
    struct PowerSample {
        uint64_t timestampNs;
        int batteryPercent;
        int acLineStatus;
        double voltage12V;
        double voltage5V;
        double voltage33V;
    };
    std::vector<PowerSample> powerHistory;
};
```

---

## 12. DIAGNÓSTICO

### Centro de diagnóstico

```cpp
enum class DiagnosticCategory {
    Cpu, Memory, Gpu, Storage, Network,
    Thermal, Power, Drivers, Windows,
    Security, Hardware, Filesystem, Registry
};

struct DiagnosticResult {
    DiagnosticCategory category;
    std::wstring status;          // "Healthy", "Warning", "Critical"
    std::wstring summary;
    std::vector<std::wstring> evidence;
    std::vector<std::wstring> recommendations;
    std::vector<int> relatedProcessPids;
    std::vector<uint64_t> relatedEventIds;
    int severity = 0;             // 0=Info, 1=Low, 2=Medium, 3=High, 4=Critical
    uint64_t timestampNs;
};

class DiagnosticsCenter {
public:
    // Ejecutar diagnóstico
    DiagnosticResult RunDiagnostic(DiagnosticCategory category, const SystemSnapshot& snapshot);
    std::vector<DiagnosticResult> RunAllDiagnostics(const SystemSnapshot& snapshot);
    
    // Diagnóstico dirigido
    DiagnosticResult InvestigateProcess(int pid, const ProcessHistory& history);
    DiagnosticResult InvestigateNetworkInterface(const std::wstring& interfaceName);
    DiagnosticResult InvestigateStorageDevice(const std::wstring& device);
    
    // Historial de diagnósticos
    std::vector<DiagnosticResult> RecentDiagnostics(int count = 50) const;
    std::vector<DiagnosticResult> DiagnosticsByCategory(DiagnosticCategory category) const;
    
    // Reglas de diagnóstico
    void AddDiagnosticRule(std::unique_ptr<DiagnosticRule> rule);
    
private:
    std::vector<std::unique_ptr<DiagnosticRule>> rules_;
    std::vector<DiagnosticResult> history_;
};
```

---

## 13. LOGGING

### Categorías de logging

```cpp
enum class LogCategory {
    System, Process, Network, Hardware, Gpu,
    Storage, Thermal, Power, Security, Filesystem,
    Registry, Audio, Display, Driver, Scram,
    Ui, User, Renderer, Kernel, Diagnostics,
    Timeline, Session, Correlation, Anomaly
};
```

### LogEntry enriquecido

```cpp
struct LogEntryV2 {
    uint64_t id;
    uint64_t timestampNs;
    std::wstring fullTimestamp;
    LogLevel level;
    LogCategory category;
    std::wstring subsystem;
    std::wstring domain;
    std::wstring message;
    std::wstring detail;
    std::wstring metadata;        // JSON
    
    // Correlación
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

---

## 14. UI

### Pantallas propuestas

```
┌─────────────────────────────────────────────────────────────┐
│ MONIX                                              [_][□][X]│
├──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬─────┤
│Dash  │ Proc │ Net  │ HW   │ Store│ Therm│ Pow  │ Sec  │Time │
├──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴─────┤
│                                                              │
│  [Contenido de la pantalla activa]                          │
│                                                              │
│                                                              │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ Status: CPU 45% | RAM 62% | GPU 78% | Risk: Low | 18:42:17│
└──────────────────────────────────────────────────────────────┘
```

### Pantallas detalladas

| Pantalla | Contenido | Datos fuente |
|----------|-----------|--------------|
| **Dashboard** | Estado general, alertas, eventos recientes, tendencias, SCRAM findings | Snapshot, EventBus, SCRAM, Timeline |
| **Processes** | Lista de procesos con historia, detalles, correlaciones | ProcessHistoryManager |
| **Network** | Interfaces, conexiones, DNS, latencia, historia | NetworkHistory |
| **Hardware** | CPU, GPU, RAM, fans, voltages, historia | HardwareHistory |
| **Storage** | Discos, SMART, I/O, temperatura | Snapshot + HardwareHistory |
| **Thermal** | Temperaturas, fans, throttle, tendencias | ThermalHistory |
| **Power** | Batería, plan de energía, voltages | PowerHistory |
| **Security** | Integridad, drivers, procesos sospechosos | SecuritySnapshot |
| **Diagnostics** | Centro de diagnóstico por categoría | DiagnosticsCenter |
| **Timeline** | Vista cronológica de todos los eventos | Timeline |
| **SCRAM** | Reglas, findings, correlaciones, riesgo | ScramEngine V2 |
| **Logs** | Logs estructurados con filtros | LogManager V2 |
| **Sessions** | Historial de sesiones, comparación | SessionManager |
| **Settings** | Configuración completa | Config |

---

## 15. DASHBOARD

### Modelo del Dashboard

```cpp
struct DashboardState {
    // Estado general
    std::wstring overallStatus;    // "Healthy", "Warning", "Critical"
    int riskScore = 0;
    std::wstring riskTrend;        // "Improving", "Stable", "Degrading"
    
    // Métricas principales
    double cpuUsage = 0.0;
    double ramUsage = 0.0;
    double gpuUsage = 0.0;
    double diskUsage = 0.0;
    int networkConnections = 0;
    double cpuTemp = 0.0;
    int batteryPercent = -1;
    
    // Tendencias (últimos N minutos)
    std::vector<double> cpuTrend;
    std::vector<double> ramTrend;
    std::vector<double> gpuTrend;
    std::vector<double> tempTrend;
    
    // Alertas activas
    std::vector<ScramFindingV2> activeFindings;
    
    // Eventos recientes
    std::vector<SystemEvent> recentEvents;
    
    // Procesos relevantes
    std::vector<ProcessHistory> topProcesses;
    
    // Red resumen
    int activeFlows = 0;
    int inboundConnections = 0;
    int outboundConnections = 0;
    
    // SCRAM resumen
    std::wstring scramHeadline;
    std::wstring scramInsight;
    std::vector<std::wstring> scramDiagnostics;
};
```

---

## 16. SISTEMA DE ACCIONES

### Modelo de acciones

```cpp
enum class ActionType {
    ProcessModify,      // Cambiar prioridad, terminar
    NetworkModify,      // Bloquear conexión
    ConfigurationChange,// Cambiar settings
    DiagnosticAction,   // Ejecutar diagnóstico
    ExportAction,       // Exportar datos
    SessionAction       // Control de sesión
};

enum class ActionRisk {
    Safe,               // Solo lectura
    Low,                // Cambio menor
    Medium,             // Requiere atención
    High,               // Requiere confirmación
    Destructive         // Requiere confirmación explícita
};

struct ActionDefinition {
    std::wstring id;
    std::wstring name;
    std::wstring description;
    ActionType type;
    ActionRisk risk;
    std::wstring confirmationMessage;  // Para acciones risky
    std::wstring icon;
};

struct ActionRequest {
    std::wstring actionId;
    std::map<std::wstring, std::wstring> parameters;
    uint64_t timestampNs;
    std::wstring requestedBy;    // "user", "scram", "diagnostic"
};

struct ActionResult {
    bool success;
    std::wstring message;
    std::wstring error;
    uint64_t timestampNs;
    std::vector<uint64_t> generatedEventIds;
};

class ActionSystem {
public:
    // Definición
    void RegisterAction(ActionDefinition definition);
    std::vector<ActionDefinition> AvailableActions() const;
    std::vector<ActionDefinition> ActionsByType(ActionType type) const;
    
    // Ejecución
    ActionResult Execute(const ActionRequest& request);
    
    // Confirmación
    bool RequiresConfirmation(const std::wstring& actionId) const;
    std::wstring GetConfirmationMessage(const std::wstring& actionId) const;
    
    // Historial
    std::vector<ActionResult> ActionHistory(int count = 100) const;
    
    // Seguridad
    void EnableAction(const std::wstring& actionId, bool enabled);
    bool IsActionEnabled(const std::wstring& actionId) const;
    
private:
    std::vector<ActionDefinition> definitions_;
    std::vector<ActionResult> history_;
    std::unordered_set<std::wstring> disabledActions_;
};
```

---

## 17. EXPORTACIÓN

### Formatos de exportación

```cpp
class ExportSystem {
public:
    // Snapshot
    void ExportSnapshotJson(const SystemSnapshot& snapshot, const std::filesystem::path& path);
    void ExportSnapshotCsv(const SystemSnapshot& snapshot, const std::filesystem::path& path);
    
    // Session
    void ExportSessionJson(const Session& session, const std::filesystem::path& path);
    void ExportSessionReport(const Session& session, const std::filesystem::path& path);
    void ExportSessionHtml(const Session& session, const std::filesystem::path& path);
    
    // Timeline
    void ExportTimelineJson(const Timeline& timeline, const std::filesystem::path& path);
    void ExportTimelineCsv(const Timeline& timeline, const std::filesystem::path& path);
    
    // Events
    void ExportEventsJson(const std::vector<SystemEvent>& events, const std::filesystem::path& path);
    void ExportEventsCsv(const std::vector<SystemEvent>& events, const std::filesystem::path& path);
    
    // SCRAM
    void ExportScramReport(const std::vector<ScramFindingV2>& findings, const std::filesystem::path& path);
    
    // Diagnostics
    void ExportDiagnosticsReport(const std::vector<DiagnosticResult>& results, const std::filesystem::path& path);
    
    // Process history
    void ExportProcessHistory(const ProcessHistory& history, const std::filesystem::path& path);
    
    // Full report
    void GenerateFullReport(const Session& session, const std::filesystem::path& outputDir);
};
```

---

## 18. COMPARACIÓN

### Sistema de comparación

```cpp
class ComparisonSystem {
    // Snapshot comparison
    SnapshotDiff CompareSnapshots(const SystemSnapshot& a, const SystemSnapshot& b);
    
    // Session comparison
    struct SessionDiff {
        std::vector<SnapshotDiffField> metricDiffs;
        std::vector<std::wstring> processChanges;
        std::vector<std::wstring> networkChanges;
        std::vector<std::wstring> eventDifferences;
        std::wstring summary;
        double cpuDiff = 0.0;
        double ramDiff = 0.0;
        double gpuDiff = 0.0;
        int errorDiff = 0;
        int warningDiff = 0;
    };
    SessionDiff CompareSessions(const Session& a, const Session& b);
    
    // Process comparison
    struct ProcessDiff {
        int pid;
        std::wstring name;
        double cpuDiff = 0.0;
        uint64_t ramDiff = 0;
        int threadDiff = 0;
        int handleDiff = 0;
        bool wasCreated = false;
        bool wasTerminated = false;
    };
    std::vector<ProcessDiff> CompareProcessStates(
        const std::vector<ProcessInfo>& a,
        const std::vector<ProcessInfo>& b);
    
    // Hardware comparison
    struct HardwareDiff {
        std::wstring component;
        std::wstring field;
        std::wstring previous;
        std::wstring current;
        bool changed = false;
    };
    std::vector<HardwareDiff> CompareHardwareStates(
        const SystemSnapshot& a,
        const SystemSnapshot& b);
};
```

---

## 19. SISTEMA VISUAL

### Desacoplamiento de UI

```cpp
// Rendering interface (no depende de lógica de negocio)
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void DrawRect(const RECT& rect, COLORREF color) = 0;
    virtual void DrawText(int x, int y, const std::wstring& text, COLORREF color, HFONT font) = 0;
    virtual void DrawLine(int x1, int y1, int x2, int y2, COLORREF color, int width = 1) = 0;
    virtual void DrawCircle(int cx, int cy, int radius, COLORREF color) = 0;
    virtual void DrawGraph(const std::vector<double>& values, double maxValue, const RECT& rect, COLORREF color) = 0;
};

// Theme system
struct Theme {
    std::wstring name;
    std::map<ColorRole, COLORREF> colors;
    std::map<std::wstring, HFONT> fonts;
    // CRT settings
    bool crtEnabled;
    float crtIntensity;
    // Border
    bool borderEnabled;
    std::wstring borderImage;
};

class ThemeManager {
public:
    void LoadTheme(const std::filesystem::path& path);
    void ApplyTheme(const Theme& theme);
    const Theme& CurrentTheme() const;
    std::vector<std::wstring> AvailableThemes() const;
};
```

---

## 20. ARQUITECTURA FINAL PROPUESTA

```
┌─────────────────────────────────────────────────────────────┐
│                        MONIX CORE                           │
│  Config, Paths, Session, Types                              │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────┴────────────────────────────────────┐
│                     COLLECTORS                              │
│  CpuCollector, RamCollector, GpuCollector, NetCollector,   │
│  DiskCollector, ProcessCollector, PowerCollector,           │
│  ThermalCollector, SecurityCollector, AudioCollector,       │
│  FilesystemCollector, RegistryCollector, DisplayCollector   │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────┴────────────────────────────────────┐
│                      SNAPSHOT                               │
│  SystemSnapshot (modelo central)                            │
└────────────────────────┬────────────────────────────────────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
┌─────────┴─────┐ ┌──────┴──────┐ ┌─────┴──────┐
│  SNAPSHOT DIFF │ │  EVENT BUS  │ │  HISTORY   │
│  Comparación   │ │  Eventos    │ │  Timeline  │
│  Cambios       │ │  Emisión    │ │  Sesiones  │
└─────────┬─────┘ └──────┬──────┘ └─────┬──────┘
          │              │              │
          └──────────────┼──────────────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
┌─────────┴─────┐ ┌──────┴──────┐ ┌─────┴──────┐
│   ANALYSIS    │ │    SCRAM    │ │ CORRELATION │
│  Anomalías    │ │  29+ reglas │ │  Multi-señal│
│  Tendencias   │ │  Findings   │ │  Patrones   │
└─────────┬─────┘ └──────┬──────┘ └─────┬──────┘
          │              │              │
          └──────────────┼──────────────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
┌─────────┴─────┐ ┌──────┴──────┐ ┌─────┴──────┐
│  DIAGNOSTICS  │ │   ACTIONS   │ │   EXPORT   │
│  Centro diag  │ │  Controladas│ │  JSON/HTML │
│  Recomendaciones│ │ Confirmación│ │  Reports   │
└─────────┬─────┘ └──────┬──────┘ └─────┬──────┘
          │              │              │
          └──────────────┼──────────────┘
                         │
┌────────────────────────┴────────────────────────────────────┐
│                        UI LAYER                             │
│  Dashboard, Processes, Network, Hardware, Thermal,         │
│  Power, Security, Diagnostics, Timeline, SCRAM, Logs,      │
│  Sessions, Settings                                         │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────┴────────────────────────────────────┐
│                     RENDERING                               │
│  GDI (fallback), OpenGL (post-process), Vulkan (shaders),  │
│  CRT effects, Themes, Fonts, Borders                       │
└─────────────────────────────────────────────────────────────┘
```

### Capas transversales

```
┌─────────────────────────────────────────────────────────────┐
│                    PERSISTENCE                              │
│  Config (INI), Sessions (binary), Timeline (binary),       │
│  Logs (plain + JSONL), Events (binary), Baselines (binary) │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                      LOGGING                                │
│  LogManager V2 (categories, correlation, structured)       │
└─────────────────────────────────────────────────────────────┘
```

### Dependencias permitidas

```
Core → (no depende de nadie)
Collectors → Core
Snapshot → Core
Diff → Snapshot
EventBus → Core
History → Snapshot, EventBus
Analysis → Snapshot, Diff, EventBus, History
SCRAM → Snapshot, Diff, EventBus, History, Analysis
Correlation → Snapshot, EventBus, Analysis
Diagnostics → Snapshot, History, SCRAM
Actions → Core, EventBus, History
Export → Snapshot, Session, Timeline, Events, SCRAM
UI → Core, Snapshot, EventBus, History, SCRAM, Diagnostics, Actions
Rendering → (no depende de lógica de negocio)
Persistence → Core
Logging → Core, EventBus
```

---

## 21. REORGANIZACIÓN DEL CÓDIGO

### Movimientos graduales

| # | Origen | Destino | Motivo | Riesgo | Orden |
|---|--------|---------|--------|--------|-------|
| 1 | `telemetry/Snapshot.hpp` | `core/Snapshot.hpp` | Snapshot es central | Bajo | 1 |
| 2 | `telemetry/Collectors.hpp` | `collectors/Collectors.hpp` | Separar collectors | Bajo | 2 |
| 3 | `telemetry/state/*` | `analysis/DiffEngine.*` | Reutilizar para diff | Bajo | 3 |
| 4 | `telemetry/TelemetryEvents.hpp` | `events/EventBus.hpp` | Centralizar eventos | Medio | 4 |
| 5 | `scram/ScramEngine.*` | `analysis/ScramEngine.*` | Mantener, extender | Bajo | 5 |
| 6 | `scram/RiskRule.hpp` | `analysis/RiskRule.hpp` | Mantener interfaz | Bajo | 6 |
| 7 | `logging/LogManager.*` | `logging/LogManagerV2.*` | Extender categorías | Bajo | 7 |
| 8 | `ui/AppStateGroups.hpp` | `core/SessionState.hpp` | Separar estados | Medio | 8 |
| 9 | Nuevo: `core/Timeline.*` | `core/Timeline.*` | Nuevo sistema | Bajo | 9 |
| 10 | Nuevo: `core/SessionManager.*` | `core/SessionManager.*` | Nuevo sistema | Bajo | 10 |
| 11 | Nuevo: `analysis/CorrelationEngine.*` | `analysis/CorrelationEngine.*` | Nuevo sistema | Bajo | 11 |
| 12 | Nuevo: `analysis/AnomalyDetector.*` | `analysis/AnomalyDetector.*` | Nuevo sistema | Bajo | 12 |
| 13 | Nuevo: `actions/ActionSystem.*` | `actions/ActionSystem.*` | Nuevo sistema | Bajo | 13 |
| 14 | Nuevo: `export/ExportSystem.*` | `export/ExportSystem.*` | Nuevo sistema | Bajo | 14 |
| 15 | `MonixApp.cpp` (métodos grandes) | `ui/Dashboard.cpp`, `ui/ProcessView.cpp`, etc. | Extraer UI | Medio | 15 |

---

## 22. CMAKE

### Organización de targets propuesta

```
monix_core/
├── core/           (Types, Config, Paths, Session)
├── collectors/     (todos los collectors)
├── events/         (EventBus, SystemEvent)
├── analysis/       (DiffEngine, ScramEngine, CorrelationEngine, AnomalyDetector)
├── history/        (Timeline, SessionManager, ProcessHistory)
├── logging/        (LogManager V2)
├── diagnostics/    (DiagnosticsCenter)
├── actions/        (ActionSystem)
└── export/         (ExportSystem)

monix_renderer/
├── renderer_vk/    (Vulkan, OpenGL, GDI)
├── ui/             (Drawing, Widgets, Themes)
└── shaders/        (Library, Runtime, Compiler)

monix_platform/
├── platform/       (DpiSetup, WindowFactory)
├── crash/          (CrashHandler)
├── login/          (AuthManager, MonixKernel)
└── sensors/        (CMake in sensors/)

monix_tests/
└── tests/          (unit + integration tests)

monix/ (main)
└── main.cpp        (entry point)
```

---

## 23. TESTING

### Tests por sistema

| Sistema | Tipo de test | Descripción |
|---------|-------------|-------------|
| **Snapshot** | Unit | Creación, campos, serialización |
| **SnapshotDiff** | Unit + Integration | Diff de dos snapshots, detección de cambios |
| **EventBus** | Unit | Emisión, suscripción, historial |
| **Events** | Unit | Creación, categorías, severidades |
| **Timeline** | Unit + Integration | Inserción, rango de tiempo, búsqueda |
| **Sessions** | Integration | Inicio, guardado, carga, comparación |
| **SCRAM** | Unit + Integration | Regla individual, engine completo, correlación |
| **Correlation** | Unit | Patrones, matching, confianza |
| **Anomaly** | Unit + Integration | Baseline, detección, persistencia |
| **Collectors** | Unit | Cada collector individual |
| **ProcessHistory** | Integration | Muestreo, terminación, query |
| **Persistence** | Integration | Save/Load de todo |
| **Configuration** | Unit | Load, save, validación |
| **Actions** | Unit + Integration | Ejecución, confirmación, historial |
| **Export** | Integration | Generación de reportes |

### Test de integración completo

```
Test: Snapshot → Diff → Event → SCRAM → Finding

1. Crear Snapshot A con valores normales
2. Crear Snapshot B con anomalías
3. Ejecutar SnapshotDiff → detectar cambios
4. Alimentar EventBus con eventos del diff
5. Ejecutar SCRAM → generar findings
6. Verificar que findings contienen evidencia correcta
7. Verificar correlación entre eventos
8. Verificar Timeline tiene todas las entradas
```

---

## 24. PLAN DE IMPLEMENTACIÓN

### FASE 1: Core y Snapshot (2 semanas)

**Objetivo:** Establecer el modelo central

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Refactorizar Snapshot en sub-structs | `core/Snapshot.hpp` | Ninguna |
| Crear SnapshotDiffer | `analysis/SnapshotDiffer.*` | Snapshot |
| Crear EventBus base | `events/EventBus.*` | Core |
| Crear SystemEvent model | `events/SystemEvent.*` | Core |
| Tests unitarios | `tests/test_snapshot.*`, `tests/test_eventbus.*` | Todo |

**Entregable:** Snapshot modularizado + EventBus funcional + tests

### FASE 2: Eventos y Detección (2 semanas)

**Objetivo:** Generar eventos a partir de cambios

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Crear ChangeDetector base | `analysis/ChangeDetector.*` | Snapshot, Diff |
| Crear ProcessChangeDetector V2 | `analysis/ProcessChangeDetector.*` | ChangeDetector |
| Crear NetworkChangeDetector | `analysis/NetworkChangeDetector.*` | ChangeDetector |
| Crear HardwareChangeDetector | `analysis/HardwareChangeDetector.*` | ChangeDetector |
| Crear SecurityChangeDetector | `analysis/SecurityChangeDetector.*` | ChangeDetector |
| Integrar con TelemetryLoop | `telemetry/TelemetryThread.cpp` | Todo |

**Entregable:** Detección de cambios + generación de eventos

### FASE 3: Timeline y Sesiones (2 semanas)

**Objetivo:** Persistencia temporal

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Crear Timeline | `history/Timeline.*` | Events |
| Crear SessionManager | `history/SessionManager.*` | Snapshot, Events, Timeline |
| Crear ProcessHistoryManager | `history/ProcessHistoryManager.*` | Snapshot |
| Integrar con EventBus | `events/EventBus.*` | Timeline |
| Tests de integración | `tests/test_timeline.*`, `tests/test_session.*` | Todo |

**Entregable:** Timeline + Sessions + ProcessHistory

### FASE 4: Análisis (2 semanas)

**Objetivo:** SCRAM 2.0 + correlación + anomalías

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Extender ScramEngine con contexto enriquecido | `analysis/ScramEngineV2.*` | Snapshot, Diff, Events, History |
| Crear CorrelationEngine | `analysis/CorrelationEngine.*` | Events, History |
| Crear AnomalyDetector | `analysis/AnomalyDetector.*` | Snapshot, History |
| Crear reglas de correlación | `analysis/correlation/` | CorrelationEngine |
| Crear reglas de anomalía | `analysis/anomaly/` | AnomalyDetector |
| Tests unitarios | `tests/test_scramv2.*`, `tests/test_correlation.*`, `tests/test_anomaly.*` | Todo |

**Entregable:** SCRAM 2.0 + Correlación + Detección de anomalías

### FASE 5: Diagnóstico y Acciones (1 semana)

**Objetivo:** Centro de diagnóstico + acciones controladas

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Crear DiagnosticsCenter | `diagnostics/DiagnosticsCenter.*` | Snapshot, History, SCRAM |
| Crear ActionSystem | `actions/ActionSystem.*` | Core, EventBus |
| Registrar acciones predefinidas | `actions/actions/` | ActionSystem |
| Tests | `tests/test_diagnostics.*`, `tests/test_actions.*` | Todo |

**Entregable:** Diagnósticos + Acciones

### FASE 6: Export y Persistencia (1 semana)

**Objetivo:** Exportación completa

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Crear ExportSystem | `export/ExportSystem.*` | Snapshot, Session, Timeline, Events |
| Implementar JSON export | `export/JsonExporter.*` | ExportSystem |
| Implementar HTML report | `export/HtmlExporter.*` | ExportSystem |
| Implementar CSV export | `export/CsvExporter.*` | ExportSystem |
| Tests | `tests/test_export.*` | Todo |

**Entregable:** Exportación JSON/HTML/CSV

### FASE 7: UI Core (3 semanas)

**Objetivo:** Pantallas principales

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Crear IRenderer interface | `ui/IRenderer.*` | Ninguna |
| Crear ThemeManager | `ui/ThemeManager.*` | IRenderer |
| Crear Dashboard | `ui/screens/Dashboard.*` | Todo |
| Crear ProcessView | `ui/screens/ProcessView.*` | ProcessHistory |
| Crear NetworkView | `ui/screens/NetworkView.*` | NetworkHistory |
| Crear TimelineView | `ui/screens/TimelineView.*` | Timeline |
| Crear ScramView V2 | `ui/screens/ScramViewV2.*` | SCRAM 2.0 |
| Crear DiagnosticsView | `ui/screens/DiagnosticsView.*` | DiagnosticsCenter |
| Crear SessionsView | `ui/screens/SessionsView.*` | SessionManager |

**Entregable:** UI completa con todas las pantallas

### FASE 8: Refactor Final (2 semanas)

**Objetivo:** Limpieza y optimización

| Tarea | Archivos | Dependencias |
|-------|----------|--------------|
| Extraer MonixApp responsibilities | `MonixApp.cpp` → múltiples archivos | Todo |
| Separar tests de production | CMakeLists.txt | Tests |
| Limpiar dual code tree | Merge src/ y Monix/Monix/ | Todo |
| Optimizar rendimiento | Profiling | Todo |
| Documentación final | docs/ | Todo |

**Entregable:** Código limpio, separado, documentado

---

## 25. RESULTADO

### 1. ¿Qué podemos reutilizar?

| Componente | Reutilización |
|------------|---------------|
| Snapshot (400+ campos) | Base central, reorganizar en sub-structs |
| ScramEngine (29 rules) | Extender con contexto enriquecido |
| RiskRule interface | Mantener, crear nuevas reglas |
| TelemetrySource/HysteresisState | Reutilizar en AnomalyDetector |
| CausalGroup | Reutilizar en CorrelationEngine |
| ProcessChangeDetector | Extender con ProcessHistory |
| UeoCorrelator | Reemplazar con CorrelationEngine |
| LogManager | Extender con categorías |
| TelemetryBaselines | Reutilizar en AnomalyDetector |
| MetricsWindow | Reutilizar en Timeline |
| IncidentTracker | Reutilizar en SCRAM 2.0 |

### 2. ¿Qué debemos reorganizar?

| Componente | Reorganización |
|------------|----------------|
| Snapshot flat → sub-structs | CPU, Memory, GPU, etc. |
| MonixApp God Object | Extraer en servicios |
| WndProc 973 líneas | Separar input/rendering |
| Dual code tree | Merge a un solo árbol |
| AppStateGroups | Reorganizar en módulos |

### 3. ¿Qué sistemas faltan?

| Sistema | Prioridad |
|---------|-----------|
| SnapshotDiff | Alta |
| EventBus centralizado | Alta |
| Timeline | Alta |
| Sessions | Alta |
| ProcessHistory | Alta |
| CorrelationEngine | Media |
| AnomalyDetector | Media |
| DiagnosticsCenter | Media |
| ActionSystem | Media |
| ExportSystem | Media |

### 4. ¿Cómo deben comunicarse?

```
Collectors → Snapshot → SnapshotDiff → EventBus
                                          ↓
                                    History/Timeline
                                          ↓
                              Analysis (SCRAM + Correlation + Anomaly)
                                          ↓
                                    DiagnosticsCenter
                                          ↓
                                      Actions
                                          ↓
                                        UI
```

### 5. ¿Cuál debe ser el modelo de datos?

**Snapshot** como fuente de verdad.
**SystemEvent** como unidad de cambio.
**Timeline** como eje temporal.
**Session** como unidad de persistencia.
**ScramFindingV2** como resultado de análisis.

### 6. ¿Cuál debe ser el flujo de eventos?

```
Collector recopila dato
    ↓
Snapshot actualizado
    ↓
SnapshotDiff comparado con anterior
    ↓
Cambios detectados
    ↓
SystemEvents generados
    ↓
EventBus distribuye
    ↓
Timeline registra
    ↓
SCRAM evalúa
    ↓
Correlación agrupa
    ↓
Anomalías detectadas
    ↓
UI actualiza
```

### 7. ¿Cómo almacenamos histórico?

- **Snapshots**: Ring buffer en memoria + persistencia binaria por sesión
- **Events**: Vector en EventBus + persistencia binaria por sesión
- **Timeline**: Vector ordenado + persistencia binaria
- **Sessions**: Directorio de archivos .monix
- **Baselines**: Archivo binario persistente

### 8. ¿Cómo funciona Timeline?

- Cada snapshot, evento, finding, acción se registra como entrada
- Entradas ordenadas por timestamp
- Consultable por rango, categoría, proceso, severidad
- Búsqueda por texto
- Contexto alrededor de cualquier entrada
- Exportable a JSON/CSV

### 9. ¿Cómo funciona SCRAM?

- Recibe Snapshot + Diff + Events + History
- 29+ reglas evaluadas
- Cada regla produce ScramFinding con evidencia detallada
- CorrelationEngine agrupa findings relacionados
- AnomalyDetector detecta anomalías
- Resultado: findings con confidence, evidence, recommendations

### 10. ¿Cómo correlacionamos información?

- CorrelationEngine con patrones predefinidos
- Señales alimentadas desde EventBus
- Matching por combinación de señales
- Confianza calculada por número y calidad de señales
- Resultado: CorrelationResult con root cause y explanation

### 11. ¿Cómo se integra la UI?

- IRenderer interface para desacoplamiento
- ThemeManager para apariencia
- Cada pantalla consume sistemas centrales
- Dashboard: resumen de todo
- Pantallas especializadas: detail view
- Timeline: vista temporal unificada

### 12. ¿Cómo se mantiene todo desacoplado?

- Interfaces claras entre capas
- EventBus como mediador
- Snapshot como modelo central
- Rendering separado de lógica
- Persistence separado de lógica

### 13. ¿Cómo se prueba?

- Unit tests por componente
- Integration tests por flujo completo
- Snapshot → Diff → Event → SCRAM → Finding (cadena completa)
- Mock de collectors para tests deterministas
- Test de persistencia (save/load roundtrip)

### 14. ¿Cómo lo implementamos por fases?

8 fases, 2-3 semanas cada una, total ~16 semanas.

Primero: Core + Snapshot + EventBus (base)
Después: Detección + Timeline + Sessions (temporalidad)
Después: Análisis + SCRAM 2.0 + Correlación (inteligencia)
Después: UI + Export (presentación)

---

## RESUMEN TÉCNICO PARA FASE 1

### Prompt para comenzar la FASE 1:

```
FASE 1: Core y Snapshot

Objetivo: Establecer el modelo central de datos

1. Refactorizar Snapshot.hpp:
   - Mantener todos los campos existentes
   - Reorganizar en sub-structs (CpuSnapshot, MemorySnapshot, etc.)
   - Crear SystemSnapshot como wrapper
   - Mantener compatibilidad con código existente

2. Crear SnapshotDiffer:
   - Comparar dos SystemSnapshot
   - Producir SnapshotDiff con campos modificados
   - Soporte para thresholds configurables
   - Detección de items añadidos/eliminados en vectores

3. Crear EventBus:
   - Sistema de suscripción por categoría y tipo
   - Emisión de SystemEvent
   - Historial accesible
   - Thread-safe

4. Crear SystemEvent:
   - Modelo de evento con categoría, severidad, tipo
   - Metadata extensible
   - Correlación por ID

5. Tests:
   - Unit tests para Snapshot creation
   - Unit tests para Diff computation
   - Unit tests para EventBus emission/subscription
   - Integration test: Snapshot → Diff → Event
```
