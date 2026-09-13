#include "ActivityModel.hpp"

#include <chrono>
#include <sstream>

namespace monix::collectors::activity {

const char* ActivityStateName(ActivityState s) {
  switch (s) {
    case ActivityState::Started:   return "Started";
    case ActivityState::Active:    return "Active";
    case ActivityState::Completed: return "Completed";
    case ActivityState::Failed:    return "Failed";
  }
  return "Unknown";
}

const char* ActivityEventKindName(ActivityEventKind k) {
  switch (k) {
    case ActivityEventKind::Created:     return "Created";
    case ActivityEventKind::EventAdded:  return "EventAdded";
    case ActivityEventKind::Completed:   return "Completed";
    case ActivityEventKind::Failed:      return "Failed";
  }
  return "Unknown";
}

std::string ActivityEventKindAction(ActivityEventKind k) {
  switch (k) {
    case ActivityEventKind::Created:     return "activity.created";
    case ActivityEventKind::EventAdded:  return "activity.event_added";
    case ActivityEventKind::Completed:   return "activity.completed";
    case ActivityEventKind::Failed:      return "activity.failed";
  }
  return "unknown";
}

bool ActivityEventRecord::isValid() const {
  return !event_id.empty();
}

bool ActivityRecord::isValid() const {
  return !id.empty();
}

bool ActivityRecord::isActive() const {
  return state == ActivityState::Started || state == ActivityState::Active;
}

bool ActivityRecord::isComplete() const {
  return state == ActivityState::Completed || state == ActivityState::Failed;
}

std::size_t ActivityRecord::eventCount() const {
  return events.size();
}

std::string ActivityRecord::summary() const {
  std::string result = id;
  result.reserve(256);
  if (!name.empty()) result += " (" + name + ")";
  result += " " + std::string(ActivityStateName(state));
  result += " " + std::to_string(events.size()) + " events";
  return result;
}

bool ActivityLifecycleEvent::isValid() const {
  return !activity_id.empty();
}

std::string ActivityLifecycleEvent::summary() const {
  std::string result;
  result.reserve(256);
  result = activity_id + " " + std::string(ActivityEventKindName(kind)) +
    " " + std::to_string(total_events) + " events";
  return result;
}

ActivityTracker::ActivityTracker() {}
ActivityTracker::~ActivityTracker() {}

void ActivityTracker::setCallback(ActivityCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

ActivityIdStr ActivityTracker::createActivity(const std::string& name, const std::string& description) {
  ActivityRecord activity;
  {
    std::lock_guard<std::mutex> lock(mu_);
    activity.id = "ACT-" + std::to_string(next_id_++);
    activity.name = name;
    activity.description = description;
    activity.state = ActivityState::Started;
    activity.created_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    activities_[activity.id] = activity;
  }
  emit(ActivityEventKind::Created, activity);
  return activity.id;
}

bool ActivityTracker::addEvent(const ActivityIdStr& activity_id, const ActivityEventRecord& event) {
  if (!event.isValid()) return false;

  ActivityRecord activity;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = activities_.find(activity_id);
    if (it == activities_.end()) return false;
    if (it->second.isComplete()) return false;
    it->second.state = ActivityState::Active;
    it->second.events.push_back(event);
    activity = it->second;
  }
  emit(ActivityEventKind::EventAdded, activity, event.event_id);
  return true;
}

bool ActivityTracker::completeActivity(const ActivityIdStr& activity_id) {
  ActivityRecord activity;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = activities_.find(activity_id);
    if (it == activities_.end()) return false;
    if (it->second.isComplete()) return false;
    it->second.state = ActivityState::Completed;
    it->second.completed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    activity = it->second;
  }
  emit(ActivityEventKind::Completed, activity);
  return true;
}

bool ActivityTracker::failActivity(const ActivityIdStr& activity_id, const std::string& reason) {
  ActivityRecord activity;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = activities_.find(activity_id);
    if (it == activities_.end()) return false;
    if (it->second.isComplete()) return false;
    it->second.state = ActivityState::Failed;
    it->second.completed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    activity = it->second;
  }
  emit(ActivityEventKind::Failed, activity, "", reason);
  return true;
}

ActivityRecord ActivityTracker::getActivity(const ActivityIdStr& activity_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = activities_.find(activity_id);
  if (it == activities_.end()) return ActivityRecord{};
  return it->second;
}

std::vector<ActivityRecord> ActivityTracker::activeActivities() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<ActivityRecord> result;
  for (const auto& [id, activity] : activities_) {
    if (activity.isActive()) result.push_back(activity);
  }
  return result;
}

std::vector<ActivityRecord> ActivityTracker::allActivities() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<ActivityRecord> result;
  result.reserve(activities_.size());
  for (const auto& [id, activity] : activities_) {
    result.push_back(activity);
  }
  return result;
}

bool ActivityTracker::isActive(const ActivityIdStr& activity_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = activities_.find(activity_id);
  if (it == activities_.end()) return false;
  return it->second.isActive();
}

std::size_t ActivityTracker::activeCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t count = 0;
  for (const auto& [id, activity] : activities_) {
    if (activity.isActive()) count++;
  }
  return count;
}

std::size_t ActivityTracker::totalCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return activities_.size();
}

std::size_t ActivityTracker::eventsEmitted() const {
  return events_emitted_.load();
}

void ActivityTracker::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  activities_.clear();
}

void ActivityTracker::emit(ActivityEventKind kind, const ActivityRecord& activity,
                           const std::string& event_id, const std::string& reason) {
  events_emitted_++;
  ActivityCallback callback;
  {
    std::lock_guard<std::mutex> lock(mu_);
    callback = callback_;
  }
  if (callback) {
    ActivityLifecycleEvent le;
    le.activity_id = activity.id;
    le.kind = kind;
    le.state = activity.state;
    le.event_id = event_id;
    le.reason = reason;
    le.total_events = activity.events.size();
    le.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    callback(le);
  }
}

}  // namespace monix::collectors::activity
