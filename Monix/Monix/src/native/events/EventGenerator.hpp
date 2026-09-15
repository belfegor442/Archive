#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../events/SystemEvent.hpp"
#include "../analysis/SnapshotDiffer.hpp"

namespace monix {

class EventGenerator {
public:
  std::vector<SystemEvent> FromDiff(const SnapshotDiff& diff,
                                     uint64_t snapshotIdBefore,
                                     uint64_t snapshotIdAfter) {
    std::vector<SystemEvent> events;

    for (const auto& field : diff.fields) {
      if (!field.isSignificant) continue;

      SystemEvent event;
      event.timestampNs = diff.timestampNs;
      event.snapshotIdBefore = snapshotIdBefore;
      event.snapshotIdAfter = snapshotIdAfter;
      event.severity = field.severity;

      MapFieldToEvent(field, event);
      events.push_back(std::move(event));
    }

    return events;
  }

private:
  void MapFieldToEvent(const DiffField& field, SystemEvent& event) {
    std::string path(field.path);

    if (path.find("cpu.") == 0) {
      event.category = EventCategory::Hardware;
      event.subsystem = L"cpu";
      if (path == "cpu.pct") {
        event.type = L"CpuLoadChanged";
        event.description = L"CPU load changed";
      } else if (path.find("cpu.contextSwitchesPerSec") != std::string::npos) {
        event.type = L"ContextSwitchesChanged";
        event.description = L"Context switches per second changed";
      } else {
        event.type = L"CpuMetricChanged";
        event.description = L"CPU metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("memory.") == 0) {
      event.category = EventCategory::Hardware;
      event.subsystem = L"memory";
      if (path == "memory.usedBytes") {
        event.type = L"MemoryUsageChanged";
        event.description = L"Memory usage changed";
        if (field.severity >= EventSeverity::High) {
          event.type = L"HighMemoryUsage";
          event.description = L"High memory usage detected";
        }
      } else {
        event.type = L"MemoryMetricChanged";
        event.description = L"Memory metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("gpu.") == 0) {
      event.category = EventCategory::Hardware;
      event.subsystem = L"gpu";
      if (path == "gpu.tempC") {
        event.type = L"GpuTemperatureChanged";
        event.description = L"GPU temperature changed";
        if (field.severity >= EventSeverity::High) {
          event.type = L"GpuTemperatureWarning";
          event.description = L"GPU temperature high";
        }
      } else if (path == "gpu.model") {
        event.type = L"GpuModelChanged";
        event.description = L"GPU model changed";
      } else {
        event.type = L"GpuMetricChanged";
        event.description = L"GPU metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("storage.") == 0) {
      event.category = EventCategory::Storage;
      event.subsystem = L"storage";
      if (path == "storage.smartHealthOk") {
        event.type = L"SmartHealthChanged";
        event.description = L"SMART health status changed";
        event.severity = EventSeverity::Critical;
        event.uiActionable = true;
      } else if (path == "storage.queueLength") {
        event.type = L"DiskQueueChanged";
        event.description = L"Disk queue length changed";
      } else {
        event.type = L"StorageMetricChanged";
        event.description = L"Storage metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("network.") == 0) {
      event.category = EventCategory::Network;
      event.subsystem = L"network";
      if (path == "network.dnsResolutionOk") {
        event.type = L"DnsStatusChanged";
        event.description = L"DNS resolution status changed";
        event.uiActionable = true;
      } else if (path == "network.adapters") {
        event.type = L"NetworkAdapterChanged";
        event.description = L"Network adapters changed";
        event.uiActionable = true;
      } else if (path == "network.latencyMs") {
        event.type = L"LatencyChanged";
        event.description = L"Network latency changed";
      } else {
        event.type = L"NetworkMetricChanged";
        event.description = L"Network metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("processes.") == 0) {
      event.category = EventCategory::Process;
      event.subsystem = L"process";
      if (path == "processes.started") {
        event.type = L"ProcessStarted";
        event.description = field.currentValue;
        event.uiActionable = true;
      } else if (path == "processes.stopped") {
        event.type = L"ProcessStopped";
        event.description = field.previousValue;
        event.uiActionable = true;
      } else {
        event.type = L"ProcessMetricChanged";
        event.description = L"Process metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("power.") == 0) {
      event.category = EventCategory::Power;
      event.subsystem = L"power";
      if (path == "power.acLineStatus") {
        event.type = L"PowerStateChanged";
        event.description = L"AC/battery state changed";
        event.uiActionable = true;
      } else if (path == "power.batteryLifePercent") {
        event.type = L"BatteryLevelChanged";
        event.description = L"Battery level changed";
        if (field.severity >= EventSeverity::High) {
          event.type = L"BatteryLow";
          event.uiActionable = true;
        }
      } else {
        event.type = L"PowerMetricChanged";
        event.description = L"Power metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("thermal.") == 0) {
      event.category = EventCategory::Thermal;
      event.subsystem = L"thermal";
      if (path == "thermal.cpuCoreTempC") {
        event.type = L"CpuTemperatureChanged";
        event.description = L"CPU temperature changed";
        if (field.severity >= EventSeverity::High) {
          event.type = L"ThermalWarning";
          event.uiActionable = true;
        }
      } else if (path == "thermal.cpuThrottling") {
        event.type = L"ThrottlingStateChanged";
        event.description = L"CPU throttling state changed";
        event.severity = EventSeverity::High;
        event.uiActionable = true;
      } else {
        event.type = L"ThermalMetricChanged";
        event.description = L"Thermal metric changed: " + ToWstring(path);
      }
    }
    else if (path.find("security.") == 0) {
      event.category = EventCategory::Security;
      event.subsystem = L"security";
      if (path == "security.unsignedDriverNames") {
        event.type = L"UnsignedDriverChanged";
        event.description = L"Unsigned driver list changed";
        event.uiActionable = true;
      } else if (path == "security.debugPortActive") {
        event.type = L"DebugPortStateChanged";
        event.description = L"Debug port state changed";
      } else if (path == "security.hookModulesDetected") {
        event.type = L"HookModuleDetected";
        event.description = L"Hook module detection changed";
        event.uiActionable = true;
      } else {
        event.type = L"SecurityMetricChanged";
        event.description = L"Security metric changed: " + ToWstring(path);
      }
    }
    else {
      event.category = EventCategory::System;
      event.subsystem = L"system";
      event.type = L"MetricChanged";
      event.description = L"Metric changed: " + ToWstring(path);
    }
  }

  static std::wstring ToWstring(const std::string& s) {
    return std::wstring(s.begin(), s.end());
  }
};

}
