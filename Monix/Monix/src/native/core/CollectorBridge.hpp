#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../telemetry/Snapshot.hpp"
#include "../events/EventBus.hpp"
#include "../events/SystemEvent.hpp"
#include "../core/snapshot/SystemSnapshot.hpp"
#include "../core/snapshot/SnapshotAdapter.hpp"

namespace monix {

class CollectorBridge {
public:
  explicit CollectorBridge(EventBus& bus) : bus_(bus) {}

  using SnapshotCallback = std::function<void(telemetry::Snapshot&)>;

  void RegisterCollector(const std::wstring& name, SnapshotCallback cb) {
    collectors_.push_back({name, std::move(cb)});
  }

  void CollectAll(telemetry::Snapshot& snap) {
    for (auto& collector : collectors_) {
      collector.callback(snap);

      SystemEvent event;
      event.timestampNs = MonotonicNs();
      event.category = EventCategory::System;
      event.subsystem = collector.name;
      event.type = L"CollectorComplete";
      event.severity = EventSeverity::Info;
      event.description = collector.name + L" collected";
      bus_.Emit(std::move(event));
    }
  }

  void NotifyCollectorState(const std::wstring& name, const std::wstring& state) {
    SystemEvent event;
    event.timestampNs = MonotonicNs();
    event.category = EventCategory::System;
    event.subsystem = name;
    event.type = L"CollectorStateChange";
    event.severity = (state == L"HEALTHY") ? EventSeverity::Info : EventSeverity::Medium;
    event.description = name + L" → " + state;
    bus_.Emit(std::move(event));
  }

  void NotifyError(const std::wstring& collector, const std::wstring& error) {
    SystemEvent event;
    event.timestampNs = MonotonicNs();
    event.category = EventCategory::System;
    event.subsystem = collector;
    event.type = L"CollectorError";
    event.severity = EventSeverity::High;
    event.description = collector + L" error: " + error;
    bus_.Emit(std::move(event));
  }

  std::vector<std::wstring> CollectorNames() const {
    std::vector<std::wstring> names;
    for (const auto& c : collectors_) names.push_back(c.name);
    return names;
  }

  std::size_t CollectorCount() const { return collectors_.size(); }

private:
  static uint64_t MonotonicNs() {
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart) * 1000000000ULL /
      static_cast<uint64_t>(freq.QuadPart);
  }

  struct CollectorEntry {
    std::wstring name;
    SnapshotCallback callback;
  };

  EventBus& bus_;
  std::vector<CollectorEntry> collectors_;
};

}
