#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monix::collectors::activityview {

struct ActivityEvent {
  std::string event_id;
  std::int64_t timestamp_ms = 0;
  std::string event_type;
  std::string summary;
  std::string severity;
  std::string source;
  std::string actor;

  bool isValid() const;
  std::string timeFormatted() const;
};

struct ActivityTimeline {
  std::string activity_id;
  std::string name;
  std::string description;
  std::int64_t started_ms = 0;
  std::int64_t ended_ms = 0;
  std::vector<ActivityEvent> events;

  bool isValid() const;
  bool isComplete() const;
  std::size_t eventCount() const;
  std::int64_t durationMs() const;
  std::string summary() const;
  std::string timeline() const;
};

class ActivityView {
public:
  ActivityView();
  ~ActivityView();

  ActivityView(const ActivityView&) = delete;
  ActivityView& operator=(const ActivityView&) = delete;

  void addActivity(const ActivityTimeline& activity);
  void addEvent(const std::string& activity_id, const ActivityEvent& event);

  ActivityTimeline getActivity(const std::string& activity_id) const;
  std::vector<ActivityTimeline> allActivities() const;
  std::vector<ActivityTimeline> activeActivities() const;
  std::vector<ActivityTimeline> completedActivities() const;

  bool hasActivity(const std::string& activity_id) const;
  std::size_t activityCount() const;

  std::string renderTimeline(const std::string& activity_id) const;

  void clear();

private:
  std::vector<ActivityTimeline> activities_;
};

}  // namespace monix::collectors::activityview
