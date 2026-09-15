#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../events/EventBus.hpp"
#include "../events/SystemEvent.hpp"
#include "../telemetry/state/ChangeSet.hpp"
#include "../telemetry/state/EntityTracker.hpp"
#include "../telemetry/Snapshot.hpp"

namespace monix {

class TelemetryBridge {
public:
  explicit TelemetryBridge(EventBus& bus) : bus_(bus) {}

  void OnChangeSet(const telemetry::ChangeSet& changes, uint64_t timestampNs) {
    for (const auto& evt : changes.events) {
      SystemEvent event;
      event.timestampNs = timestampNs;
      event.correlationId = L"CHANGE-" + std::to_wstring(evt.entity.idLow);

      switch (evt.entity.kind) {
        case telemetry::EntityIdentity::Kind::Process:
          event.category = EventCategory::Process;
          event.subsystem = L"process";
          switch (evt.kind) {
            case telemetry::ChangeEvent::Kind::Created:
              event.type = L"ProcessStarted";
              event.severity = EventSeverity::Info;
              event.processId = static_cast<int>(evt.entity.idLow);
              event.uiActionable = true;
              break;
            case telemetry::ChangeEvent::Kind::Terminated:
              event.type = L"ProcessStopped";
              event.severity = EventSeverity::Info;
              event.processId = static_cast<int>(evt.entity.idLow);
              event.uiActionable = true;
              break;
            case telemetry::ChangeEvent::Kind::Modified:
              event.type = L"ProcessModified";
              event.severity = EventSeverity::Low;
              event.processId = static_cast<int>(evt.entity.idLow);
              break;
            default:
              event.type = L"ProcessChanged";
              event.severity = EventSeverity::Low;
              break;
          }
          break;

        case telemetry::EntityIdentity::Kind::Driver:
          event.category = EventCategory::Driver;
          event.subsystem = L"driver";
          switch (evt.kind) {
            case telemetry::ChangeEvent::Kind::Created:
              event.type = L"DriverLoaded";
              event.severity = EventSeverity::Medium;
              event.uiActionable = true;
              break;
            case telemetry::ChangeEvent::Kind::Terminated:
              event.type = L"DriverUnloaded";
              event.severity = EventSeverity::Medium;
              event.uiActionable = true;
              break;
            default:
              event.type = L"DriverChanged";
              event.severity = EventSeverity::Low;
              break;
          }
          break;

        case telemetry::EntityIdentity::Kind::Thread:
          event.category = EventCategory::Process;
          event.subsystem = L"thread";
          event.type = L"ThreadChanged";
          event.severity = EventSeverity::Low;
          break;

        case telemetry::EntityIdentity::Kind::Module:
          event.category = EventCategory::Security;
          event.subsystem = L"module";
          event.type = L"ModuleChanged";
          event.severity = EventSeverity::Low;
          break;

        default:
          event.category = EventCategory::System;
          event.subsystem = L"system";
          event.type = L"EntityChanged";
          event.severity = EventSeverity::Low;
          break;
      }

      event.description = std::wstring(event.type) +
        L" [" + std::to_wstring(evt.entity.idLow) + L"]";
      if (evt.attribute) {
        event.description += L" attr=" + std::wstring(evt.attribute, evt.attribute + strlen(evt.attribute));
      }

      bus_.Emit(std::move(event));
    }
  }

  void OnSnapshotCollected(const Snapshot& snap, uint64_t timestampNs) {
    SystemEvent event;
    event.timestampNs = timestampNs;
    event.category = EventCategory::System;
    event.subsystem = L"telemetry";
    event.type = L"SnapshotCollected";
    event.severity = EventSeverity::Info;
    event.snapshotIdAfter = snap.snapshotId;
    event.description = L"Telemetry sample #" + std::to_wstring(snap.snapshotId);
    bus_.Emit(std::move(event));
  }

  void OnScramFinding(const std::wstring& headline, const std::wstring& insight,
                       int riskScore, uint64_t timestampNs) {
    SystemEvent event;
    event.timestampNs = timestampNs;
    event.category = EventCategory::Scram;
    event.subsystem = L"scram";
    event.type = L"ScramFindingEmitted";
    event.severity = (riskScore > 50) ? EventSeverity::High : EventSeverity::Medium;
    event.description = headline;
    event.metadata = insight;
    event.uiActionable = true;
    bus_.Emit(std::move(event));
  }

  void OnCollectorStateChange(const std::wstring& collectorName,
                               const std::wstring& newState,
                               uint64_t timestampNs) {
    SystemEvent event;
    event.timestampNs = timestampNs;
    event.category = EventCategory::System;
    event.subsystem = L"collector";
    event.type = L"CollectorStateChanged";
    event.severity = (newState == L"HEALTHY") ? EventSeverity::Info : EventSeverity::Medium;
    event.description = collectorName + L" → " + newState;
    bus_.Emit(std::move(event));
  }

private:
  EventBus& bus_;
};

}
