#include "ActivityView.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace monix::collectors::activityview {

bool ActivityEvent::isValid() const {
  return !event_id.empty();
}

std::string ActivityEvent::timeFormatted() const {
  auto total_sec = timestamp_ms / 1000;
  auto hours = total_sec / 3600;
  auto minutes = (total_sec % 3600) / 60;
  auto seconds = total_sec % 60;

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << hours << ":"
      << std::setfill('0') << std::setw(2) << minutes << ":"
      << std::setfill('0') << std::setw(2) << seconds;
  return oss.str();
}

bool ActivityTimeline::isValid() const {
  return !activity_id.empty();
}

bool ActivityTimeline::isComplete() const {
  return ended_ms > 0;
}

std::size_t ActivityTimeline::eventCount() const {
  return events.size();
}

std::int64_t ActivityTimeline::durationMs() const {
  if (ended_ms == 0) return 0;
  return ended_ms - started_ms;
}

std::string ActivityTimeline::summary() const {
  std::string result = activity_id;
  if (!name.empty()) result += " (" + name + ")";
  result += " " + std::to_string(events.size()) + " events";
  return result;
}

std::string ActivityTimeline::timeline() const {
  std::ostringstream oss;
  oss << "Activity " << activity_id;
  if (!name.empty()) oss << " (" << name << ")";
  oss << "\n";

  for (const auto& event : events) {
    oss << "  " << event.timeFormatted() << " ";
    oss << event.event_type;
    if (!event.summary.empty()) oss << " " << event.summary;
    if (!event.severity.empty()) oss << " [" << event.severity << "]";
    oss << "\n";
  }

  return oss.str();
}

ActivityView::ActivityView() {}
ActivityView::~ActivityView() {}

void ActivityView::addActivity(const ActivityTimeline& activity) {
  activities_.push_back(activity);
}

void ActivityView::addEvent(const std::string& activity_id, const ActivityEvent& event) {
  for (auto& activity : activities_) {
    if (activity.activity_id == activity_id) {
      activity.events.push_back(event);
      std::sort(activity.events.begin(), activity.events.end(),
        [](const ActivityEvent& a, const ActivityEvent& b) {
          return a.timestamp_ms < b.timestamp_ms;
        });
      if (!activity.started_ms || event.timestamp_ms < activity.started_ms) {
        activity.started_ms = event.timestamp_ms;
      }
      if (event.timestamp_ms > activity.ended_ms) {
        activity.ended_ms = event.timestamp_ms;
      }
      return;
    }
  }

  ActivityTimeline new_activity;
  new_activity.activity_id = activity_id;
  new_activity.events.push_back(event);
  new_activity.started_ms = event.timestamp_ms;
  new_activity.ended_ms = event.timestamp_ms;
  activities_.push_back(new_activity);
}

ActivityTimeline ActivityView::getActivity(const std::string& activity_id) const {
  for (const auto& activity : activities_) {
    if (activity.activity_id == activity_id) return activity;
  }
  return ActivityTimeline{};
}

std::vector<ActivityTimeline> ActivityView::allActivities() const {
  return activities_;
}

std::vector<ActivityTimeline> ActivityView::activeActivities() const {
  std::vector<ActivityTimeline> result;
  for (const auto& activity : activities_) {
    if (!activity.isComplete()) result.push_back(activity);
  }
  return result;
}

std::vector<ActivityTimeline> ActivityView::completedActivities() const {
  std::vector<ActivityTimeline> result;
  for (const auto& activity : activities_) {
    if (activity.isComplete()) result.push_back(activity);
  }
  return result;
}

bool ActivityView::hasActivity(const std::string& activity_id) const {
  for (const auto& activity : activities_) {
    if (activity.activity_id == activity_id) return true;
  }
  return false;
}

std::size_t ActivityView::activityCount() const {
  return activities_.size();
}

std::string ActivityView::renderTimeline(const std::string& activity_id) const {
  for (const auto& activity : activities_) {
    if (activity.activity_id == activity_id) {
      return activity.timeline();
    }
  }
  return "";
}

void ActivityView::clear() {
  activities_.clear();
}

}  // namespace monix::collectors::activityview
