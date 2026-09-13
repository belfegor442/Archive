# Monix Native Architecture

## Data Flow Pipeline

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SENSOR PROVIDERS                             │
│  SnapshotPoller (CPU/RAM/GPU/Disk/Net/Thermal/Power/Process/...)   │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ raw Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     NORMALIZATION + VALIDATION                      │
│  Normalizer.hpp          Validator.hpp                              │
│  - Clamps CPU 0-100%    - Validates CPU/RAM/GPU/Disk/Net/Thermal   │
│  - Clamps RAM within    - Detects suspicious/invalid values         │
│    total                - Reports ValidationLevel per field         │
│  - Converts units       - IsValid()/HasSuspicious()/HasInvalid()   │
│  - NormalizeEvent log                                          │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ normalized Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         STATE STORE                                 │
│  StateStore.hpp                                                    │
│  - TimelineRing (64-snapshot circular buffer)                      │
│  - EntityTracker (process/driver lifecycle detection)              │
│  - ChangeSet (field-level change events)                           │
│  - Update() → DetectChanges() → Push to timeline                   │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ current + previous Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     CHANGE DETECTION                                │
│  IChangeDetector.hpp (interface)                                    │
│  ├── ThresholdDetector     (warn/error/critical thresholds)        │
│  ├── SnapshotChangeDetector (multi-field delta comparison)         │
│  └── ProcessChangeDetector (EntityTracker lifecycle wrapper)       │
│                                                                      │
│  Output: ChangeSet with ChangeEvent vector                          │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ ChangeSet + Snapshot
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      CORRELATION ENGINE                             │
│  CorrelationEngine.hpp + CausalChain.hpp                            │
│  - 16 pre-configured rules (CPU↔Temp, GPU↔FrameTime, etc.)       │
│  - 6 correlation types (Positive/Negative/Lagging/Leading/         │
│    Threshold/Inverse)                                               │
│  - EMA-smoothed confidence (0.8 decay)                             │
│  - CausalChain: cause→effect with lag detection                    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ CorrelationEvent[]
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    SCRAM RISK EVALUATION                            │
│  ScramEngine (29 RiskRules)                                         │
│  ├── RiskScorer     (budget-based scoring, 60pts max, EMA α=0.3) │
│  ├── SeverityTracker (hysteresis: Info→Warning→Error→Critical)    │
│  └── HeadlineGenerator (formats diagnostic headlines)              │
│                                                                      │
│  Output: ScramResult { headline, insight, diagnostics, riskScore } │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ ScramResult
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      EXPORT + HISTORY                               │
│  TelemetryRelay (WebSocket port 8421)                               │
│  - Broadcasts JSON snapshots + SCRAM updates to connected clients  │
│  - Zero-dependency RFC6455 implementation (SHA-1 + Base64)        │
│                                                                      │
│  ScramHistoryRing (128-event circular buffer)                       │
│  - Records timestamp, riskScore, headline, insight, diagnostics   │
│  - Query: GetRecent(), GetByRiskThreshold(), SerializeRecent()    │
│                                                                      │
│  HttpBridge (HTTP port 8422)                                        │
│  - GET /status   → JSON system status                               │
│  - GET /history  → SCRAM event history                              │
│  - GET /health   → health check                                     │
│  - CORS-enabled for browser clients                                 │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                       EXTERNAL CLIENTS                              │
│  - Web dashboard (WebSocket + HTTP)                                 │
│  - Python monitoring scripts                                        │
│  - C# managed bridge (WMI/ETW)                                      │
│  - Rust sensor crates (cpu/memory/disk/net/thermal/process)        │
└─────────────────────────────────────────────────────────────────────┘
```

## Component Inventory

### Phase 0-3: Core Pipeline
| File | Purpose |
|------|---------|
| `telemetry/Snapshot.hpp` | 413-line struct with ~400 telemetry fields |
| `telemetry/contract/*.hpp` | SensorState, Measurement, EntityIdentity, TemporalModel, SensorProvider |
| `telemetry/state/*.hpp/.cpp` | StateStore, TimelineRing, EntityTracker, ChangeSet |
| `telemetry/state/IChangeDetector.hpp` | Abstract change detection interface |
| `telemetry/state/ThresholdDetector.hpp/.cpp` | Numeric threshold detection |
| `telemetry/state/SnapshotChangeDetector.hpp/.cpp` | Multi-field delta comparison |
| `telemetry/state/ProcessChangeDetector.hpp` | Process lifecycle detection |

### Phase 4-5: Analysis
| File | Purpose |
|------|---------|
| `telemetry/normalization/Normalizer.hpp` | Clamps/converts raw telemetry to valid ranges |
| `telemetry/normalization/Validator.hpp` | Validates telemetry within bounds, detects anomalies |
| `telemetry/correlation/CorrelationEngine.hpp/.cpp` | 16-rule correlation with EMA confidence |
| `telemetry/correlation/CausalChain.hpp` | Cause-effect chain with lag detection |

### Phase 6-7: SCRAM + ETW
| File | Purpose |
|------|---------|
| `scram/risk/RiskScorer.hpp` | Budget-based risk scoring with diminishing returns |
| `scram/risk/SeverityTracker.hpp` | Hysteresis severity state machine |
| `scram/risk/HeadlineGenerator.hpp` | Formats SCRAM headlines with context |
| `telemetry/etw/EtwSession.hpp` | ETW session management |
| `telemetry/etw/EtwProvider.hpp` | ETW event provider |
| `telemetry/etw/KernelTraceCollector.hpp` | Kernel ETW event collection |

### Phase 8-9: Multi-language
| File | Purpose |
|------|---------|
| `src/rust/monix-sensors/` | Rust CPU/memory/disk/net/thermal/process sensors |
| `src/rust/monix-state/` | Rust timeline/entity/change state store |
| `src/managed/Monix.Managed/` | C# WMI, EventLog, PnP monitoring |

### Phase 10-11: Export + History
| File | Purpose |
|------|---------|
| `telemetry/export/WebSocketServer.hpp` | RFC6455 WebSocket server (SHA-1, Base64, non-blocking) |
| `telemetry/export/TelemetryRelay.hpp` | JSON serialization + broadcast to WS clients |
| `telemetry/export/ScramHistoryRing.hpp` | 128-event SCRAM history ring buffer |
| `telemetry/export/HttpBridge.hpp` | HTTP status/history endpoint (CORS) |
| `telemetry/export/TelemetryExport.hpp` | Umbrella header |

### Phase 12-13: Integration + Hardening
| File | Purpose |
|------|---------|
| `telemetry/PipelineOrchestrator.hpp` | Wires Normalizer→Validator→Correlation→StateStore→Relay |
| `telemetry/PipelineHealth.hpp` | Error tracking, uptime, relay status, JSON health endpoint |

### Phase 14: Tests
| File | Purpose |
|------|---------|
| `tests/PipelineTest_unity.cpp` | 16 unit tests for all new components |

## Dashboard Connection Guide

### WebSocket (port 8421)

Connect via JavaScript:
```javascript
const ws = new WebSocket('ws://localhost:8421');
ws.onmessage = (e) => {
  const data = JSON.parse(e.data);
  if (data.type === 'snapshot') {
    updateDashboard(data.cpu, data.ram, data.gpu, data.scram);
  } else if (data.type === 'scram') {
    updateScramPanel(data.risk, data.headline, data.diagnostics);
  }
};
```

### HTTP API (port 8422)

```bash
# System status
curl http://localhost:8422/status

# SCRAM history (last 50 events)
curl http://localhost:8422/history

# Health check
curl http://localhost:8422/health
```

### JSON Message Types

**snapshot** (streamed every telemetry tick):
```json
{
  "type": "snapshot",
  "ts": 1234567890,
  "sample": 42,
  "cpu": {"pct": 67.3, "cores": 16, "freqMhz": 3600, "tempC": 72.1, "thermalThrottle": false},
  "ram": {"pct": 54.2, "usedMb": 8672, "totalMb": 16384, "availMb": 7712},
  "gpu": {"pct": 23.1, "tempC": 58.0, "vramUsedMb": 2048, "vramTotalMb": 12288},
  "disk": {"readMb": 45.2, "writeMb": 12.8, "queueLen": 0.3, "pctUsed": 62.1},
  "net": {"upMb": 1.2, "downMb": 8.4},
  "power": {"batteryPct": 87, "batteryLife": 85},
  "uptime": {"ms": 345600000},
  "processes": {"count": 284},
  "security": {"unsignedDrivers": 0, "debugPort": 0},
  "scram": {"risk": 25, "headline": "...", "insight": "...", "diagnostics": []}
}
```

**scram** (emitted on severity change or periodic update):
```json
{
  "type": "scram",
  "ts": 1234567890,
  "risk": 65,
  "headline": "CPU pressure sustained above threshold",
  "insight": "CPU usage has been above 80% for 30+ seconds with elevated context switches.",
  "diagnostics": ["cpu=87% contextSwitches=45000 thermal=78C"]
}
```

**status** (broadcast on demand):
```json
{
  "type": "status",
  "message": "Pipeline running: 1247 samples, 0 errors, 2 WS clients"
}
```

## Integration into SnapshotConsumer

To enable the full pipeline in `ConsumeSnapshot()`:

```cpp
// In MonixApp constructor or Run():
telemetry::PipelineOrchestrator pipeline;
pipeline.InitializeRelay(WsConfig{8421}, 128);
pipeline.StartRelay();

// In ConsumeSnapshot(), after health checks:
telemetry::PipelineResult pResult = pipeline.ProcessSnapshot(
  snapshot, hadPrevious ? &previous : nullptr,
  scramEngine_, outScram, timestampMs, sampleIndex);

// After SCRAM evaluation:
pipeline.RecordScramEvent(outScram, timestampMs);

// Health monitoring:
pipeline.GetHealth().RecordSnapshot();
```
