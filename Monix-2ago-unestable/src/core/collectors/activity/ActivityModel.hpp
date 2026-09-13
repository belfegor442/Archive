#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::activity {

using ActivityIdStr = std::string;

enum class ActivityState : std::uint8_t {
  Started,
  Active,
  Completed,
  Failed
};

const char* ActivityStateName(ActivityState s);

enum class ActivityEventKind : std::uint8_t {
  Created,
  EventAdded,
  Completed,
  Failed
};

const char* ActivityEventKindName(ActivityEventKind k);
std::string ActivityEventKindAction(ActivityEventKind k);

struct ActivityEventRecord {
  std::string event_id;
  std::string event_type;
  std::string source;
  std::string description;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
};

struct ActivityRecord {
  ActivityIdStr id;
  ActivityState state = ActivityState::Started;
  std::string name;
  std::string description;
  std::int64_t created_ms = 0;
  std::int64_t completed_ms = 0;
  std::vector<ActivityEventRecord> events;

  bool isValid() const;
  bool isActive() const;
  bool isComplete() const;
  std::size_t eventCount() const;
  std::string summary() const;
};

struct ActivityLifecycleEvent {
  ActivityIdStr activity_id;
  ActivityEventKind kind = ActivityEventKind::Created;
  ActivityState state = ActivityState::Started;
  std::string event_id;
  std::string reason;
  std::size_t total_events = 0;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

using ActivityCallback = std::function<void(const ActivityLifecycleEvent&)>;

class ActivityTracker {
public:
  ActivityTracker();
  ~ActivityTracker();

  ActivityTracker(const ActivityTracker&) = delete;
  ActivityTracker& operator=(const ActivityTracker&) = delete;

  void setCallback(ActivityCallback cb);

  ActivityIdStr createActivity(const std::string& name, const std::string& description = "");

  bool addEvent(const ActivityIdStr& activity_id, const ActivityEventRecord& event);

  bool completeActivity(const ActivityIdStr& activity_id);
  bool failActivity(const ActivityIdStr& activity_id, const std::string& reason = "");

  ActivityRecord getActivity(const ActivityIdStr& activity_id) const;
  std::vector<ActivityRecord> activeActivities() const;
  std::vector<ActivityRecord> allActivities() const;

  bool isActive(const ActivityIdStr& activity_id) const;
  std::size_t activeCount() const;
  std::size_t totalCount() const;
  std::size_t eventsEmitted() const;

  void clear();

private:
  void emit(ActivityEventKind kind, const ActivityRecord& activity,
            const std::string& event_id = "", const std::string& reason = "");

  ActivityCallback callback_;
  mutable std::mutex mu_;
  std::unordered_map<ActivityIdStr, ActivityRecord> activities_;
  std::size_t next_id_{1};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::activity
