#include "EventInspector.hpp"

#include <algorithm>
#include <unordered_map>

namespace monix::collectors::inspector {

bool EventDetail::isValid() const {
  return !event_id.empty();
}

std::string EventDetail::summary() const {
  std::string result = event_id + " [" + event_type + "]";
  if (!severity.empty()) result += " " + severity;
  if (!source.empty()) result += " src=" + source;
  return result;
}

bool NavigationLink::isValid() const {
  return !event_id.empty() && !link_type.empty();
}

EventInspector::EventInspector() {}
EventInspector::~EventInspector() {}

void EventInspector::inspect(const EventDetail& event) {
  current_ = event;
  has_event_ = true;
  links_.clear();

  if (!event.parent_event_id.empty()) {
    links_.push_back({"Parent", event.parent_event_id, "parent"});
  }

  auto child_it = children_.find(event.event_id);
  if (child_it != children_.end()) {
    for (const auto& child_id : child_it->second) {
      links_.push_back({"Child", child_id, "child"});
    }
  }

  if (!event.activity_id.empty()) {
    auto rel_it = related_.find(event.activity_id);
    if (rel_it != related_.end()) {
      for (const auto& rel_id : rel_it->second) {
        if (rel_id != event.event_id) {
          links_.push_back({"Activity", rel_id, "activity"});
        }
      }
    }
  }

  if (!event.actor.empty()) {
    links_.push_back({"Same Process", event.actor, "process"});
  }
  if (!event.session_id.empty()) {
    links_.push_back({"Same Session", event.session_id, "session"});
  }
  if (!event.source.empty()) {
    links_.push_back({"Same Device", event.source, "device"});
  }
}

void EventInspector::clear() {
  current_ = EventDetail{};
  has_event_ = false;
  links_.clear();
}

EventDetail EventInspector::currentEvent() const {
  return current_;
}

bool EventInspector::hasEvent() const {
  return has_event_;
}

std::vector<NavigationLink> EventInspector::navigationLinks() const {
  return links_;
}

std::vector<NavigationLink> EventInspector::parentLinks() const {
  std::vector<NavigationLink> result;
  for (const auto& link : links_) {
    if (link.link_type == "parent") result.push_back(link);
  }
  return result;
}

std::vector<NavigationLink> EventInspector::childLinks() const {
  std::vector<NavigationLink> result;
  for (const auto& link : links_) {
    if (link.link_type == "child") result.push_back(link);
  }
  return result;
}

std::vector<NavigationLink> EventInspector::activityLinks() const {
  std::vector<NavigationLink> result;
  for (const auto& link : links_) {
    if (link.link_type == "activity") result.push_back(link);
  }
  return result;
}

std::vector<NavigationLink> EventInspector::processLinks() const {
  std::vector<NavigationLink> result;
  for (const auto& link : links_) {
    if (link.link_type == "process") result.push_back(link);
  }
  return result;
}

std::vector<NavigationLink> EventInspector::sessionLinks() const {
  std::vector<NavigationLink> result;
  for (const auto& link : links_) {
    if (link.link_type == "session") result.push_back(link);
  }
  return result;
}

std::vector<NavigationLink> EventInspector::deviceLinks() const {
  std::vector<NavigationLink> result;
  for (const auto& link : links_) {
    if (link.link_type == "device") result.push_back(link);
  }
  return result;
}

void EventInspector::setChildren(const std::string& parent_id, const std::vector<std::string>& child_ids) {
  children_[parent_id] = child_ids;
}

void EventInspector::setRelatedEvents(const std::string& activity_id, const std::vector<std::string>& event_ids) {
  related_[activity_id] = event_ids;
}

}  // namespace monix::collectors::inspector
