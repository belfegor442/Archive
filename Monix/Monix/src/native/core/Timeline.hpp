#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "../events/SystemEvent.hpp"

namespace monix {

struct TimelineEntry {
  uint64_t id = 0;
  uint64_t timestampNs = 0;

  enum class Type : uint8_t {
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
  void AddSnapshot(uint64_t tsNs, uint64_t snapshotId, const std::wstring& label) {
    std::lock_guard<std::mutex> lock(mutex_);
    TimelineEntry e;
    e.id = nextId_++;
    e.timestampNs = tsNs;
    e.type = TimelineEntry::Type::Snapshot;
    e.snapshotId = snapshotId;
    e.label = label;
    e.severity = EventSeverity::Info;
    e.category = EventCategory::System;
    entries_.push_back(std::move(e));
    TrimLocked();
  }

  void AddEvent(const SystemEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    TimelineEntry e;
    e.id = nextId_++;
    e.timestampNs = event.timestampNs;
    e.type = TimelineEntry::Type::Event;
    e.eventId = event.id;
    e.label = event.type;
    e.detail = event.description;
    e.severity = event.severity;
    e.category = event.category;
    e.correlationId = event.correlationId;
    e.processId = event.processId;
    e.processName = event.processName;
    entries_.push_back(std::move(e));
    TrimLocked();
  }

  void AddFinding(uint64_t findingId, const std::wstring& headline,
                   int riskScore, uint64_t tsNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    TimelineEntry e;
    e.id = nextId_++;
    e.timestampNs = tsNs;
    e.type = TimelineEntry::Type::Finding;
    e.findingId = findingId;
    e.label = headline;
    e.detail = L"Risk score: " + std::to_wstring(riskScore);
    e.severity = (riskScore > 50) ? EventSeverity::High : EventSeverity::Medium;
    e.category = EventCategory::Scram;
    entries_.push_back(std::move(e));
    TrimLocked();
  }

  void AddUserAction(const std::wstring& action, uint64_t tsNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    TimelineEntry e;
    e.id = nextId_++;
    e.timestampNs = tsNs;
    e.type = TimelineEntry::Type::UserAction;
    e.label = action;
    e.severity = EventSeverity::Info;
    e.category = EventCategory::User;
    entries_.push_back(std::move(e));
    TrimLocked();
  }

  void AddDiagnostic(const std::wstring& category,
                      const std::wstring& summary,
                      int severity, uint64_t tsNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    TimelineEntry e;
    e.id = nextId_++;
    e.timestampNs = tsNs;
    e.type = TimelineEntry::Type::Diagnostic;
    e.label = summary;
    e.severity = static_cast<EventSeverity>(severity);
    e.category = EventCategory::Diagnostic;
    entries_.push_back(std::move(e));
    TrimLocked();
  }

  std::vector<TimelineEntry> EntriesInRange(uint64_t startNs, uint64_t endNs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (const auto& e : entries_) {
      if (e.timestampNs >= startNs && e.timestampNs <= endNs) {
        result.push_back(e);
      }
    }
    return result;
  }

  std::vector<TimelineEntry> EntriesByCategory(EventCategory cat) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (const auto& e : entries_) {
      if (e.category == cat) result.push_back(e);
    }
    return result;
  }

  std::vector<TimelineEntry> EntriesBySeverity(EventSeverity min) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (const auto& e : entries_) {
      if (e.severity >= min) result.push_back(e);
    }
    return result;
  }

  std::vector<TimelineEntry> EntriesByProcess(int pid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (const auto& e : entries_) {
      if (e.processId == pid) result.push_back(e);
    }
    return result;
  }

  std::vector<TimelineEntry> EntriesByCorrelation(const std::wstring& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (const auto& e : entries_) {
      if (e.correlationId == id) result.push_back(e);
    }
    return result;
  }

  std::vector<TimelineEntry> Search(const std::wstring& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (const auto& e : entries_) {
      if (e.label.find(query) != std::wstring::npos ||
          e.detail.find(query) != std::wstring::npos) {
        result.push_back(e);
      }
    }
    return result;
  }

  std::vector<TimelineEntry> ContextAround(uint64_t entryId,
                                            int before = 5, int after = 5) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      if (entries_[i].id == entryId) {
        int start = static_cast<int>(i) - before;
        if (start < 0) start = 0;
        int end = static_cast<int>(i) + after + 1;
        if (end > static_cast<int>(entries_.size())) {
          end = static_cast<int>(entries_.size());
        }
        for (int j = start; j < end; ++j) {
          result.push_back(entries_[j]);
        }
        break;
      }
    }
    return result;
  }

  int TotalEntries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(entries_.size());
  }

  std::map<EventCategory, int> EntriesByCategoryCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<EventCategory, int> counts;
    for (const auto& e : entries_) {
      counts[e.category]++;
    }
    return counts;
  }

  const TimelineEntry& Entry(uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    static const TimelineEntry empty;
    for (const auto& e : entries_) {
      if (e.id == id) return e;
    }
    return empty;
  }

  std::vector<TimelineEntry> RecentEntries(int count = 50) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TimelineEntry> result;
    int n = std::min(count, static_cast<int>(entries_.size()));
    for (int i = static_cast<int>(entries_.size()) - n;
         i < static_cast<int>(entries_.size()); ++i) {
      result.push_back(entries_[i]);
    }
    return result;
  }

  void TrimBefore(uint64_t timestampNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(
      std::remove_if(entries_.begin(), entries_.end(),
        [timestampNs](const TimelineEntry& e) {
          return e.timestampNs < timestampNs;
        }),
      entries_.end());
  }

  void SetMaxEntries(int max) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxEntries_ = max;
    TrimLocked();
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
  }

  bool Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.empty();
  }

private:
  void TrimLocked() {
    while (static_cast<int>(entries_.size()) > maxEntries_) {
      entries_.erase(entries_.begin());
    }
  }

  std::vector<TimelineEntry> entries_;
  uint64_t nextId_ = 1;
  int maxEntries_ = 50000;
  mutable std::mutex mutex_;
};

}
