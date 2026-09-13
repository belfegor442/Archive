#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::inspector {

struct EventDetail {
  std::string event_id;
  std::string event_type;
  std::int64_t timestamp_ms = 0;
  std::string severity;
  std::string priority;

  std::string actor;
  std::string actor_type;
  std::string actor_pid;

  std::string entity;
  std::string entity_type;
  std::string entity_name;

  std::string source;
  std::string source_type;

  std::string correlation_id;
  std::string parent_event_id;
  std::string activity_id;
  std::string session_id;
  std::string request_id;

  std::string provenance;

  std::string payload;
  std::string validation_status;
  std::string integrity_hash;
  std::string integrity_status;

  std::vector<std::string> tags;

  bool isValid() const;
  std::string summary() const;
};

struct NavigationLink {
  std::string label;
  std::string event_id;
  std::string link_type;

  bool isValid() const;
};

class EventInspector {
public:
  EventInspector();
  ~EventInspector();

  EventInspector(const EventInspector&) = delete;
  EventInspector& operator=(const EventInspector&) = delete;

  void inspect(const EventDetail& event);
  void clear();

  EventDetail currentEvent() const;
  bool hasEvent() const;

  std::vector<NavigationLink> navigationLinks() const;

  std::vector<NavigationLink> parentLinks() const;
  std::vector<NavigationLink> childLinks() const;
  std::vector<NavigationLink> activityLinks() const;
  std::vector<NavigationLink> processLinks() const;
  std::vector<NavigationLink> sessionLinks() const;
  std::vector<NavigationLink> deviceLinks() const;

  void setChildren(const std::string& parent_id, const std::vector<std::string>& child_ids);
  void setRelatedEvents(const std::string& activity_id, const std::vector<std::string>& event_ids);

private:
  EventDetail current_;
  bool has_event_ = false;
  std::vector<NavigationLink> links_;
  std::unordered_map<std::string, std::vector<std::string>> children_;
  std::unordered_map<std::string, std::vector<std::string>> related_;
};

}  // namespace monix::collectors::inspector
