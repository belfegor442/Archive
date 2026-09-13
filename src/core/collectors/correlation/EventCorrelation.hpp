#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::correlation {

using EventIdStr = std::string;
using CorrelationIdStr = std::string;
using ActivityIdStr = std::string;
using SessionIdStr = std::string;
using RequestIdStr = std::string;

struct CorrelatedEvent {
  EventIdStr event_id;
  EventIdStr parent_event_id;
  CorrelationIdStr correlation_id;
  ActivityIdStr activity_id;
  SessionIdStr session_id;
  RequestIdStr request_id;
  std::string event_type;
  std::string source;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

struct CorrelationChain {
  CorrelationIdStr id;
  std::vector<EventIdStr> event_ids;
  std::int64_t started_ms = 0;
  std::int64_t ended_ms = 0;

  bool isValid() const;
  std::size_t size() const;
  std::string summary() const;
};

class CorrelationEngine {
public:
  CorrelationEngine();
  ~CorrelationEngine();

  CorrelationEngine(const CorrelationEngine&) = delete;
  CorrelationEngine& operator=(const CorrelationEngine&) = delete;

  void addEvent(const CorrelatedEvent& event);
  void removeEvent(const EventIdStr& event_id);

  CorrelatedEvent getEvent(const EventIdStr& event_id) const;
  bool hasEvent(const EventIdStr& event_id) const;

  std::vector<CorrelatedEvent> byCorrelationId(const CorrelationIdStr& id) const;
  std::vector<CorrelatedEvent> byParentEvent(const EventIdStr& parent_id) const;
  std::vector<CorrelatedEvent> byActivity(const ActivityIdStr& id) const;
  std::vector<CorrelatedEvent> bySession(const SessionIdStr& id) const;
  std::vector<CorrelatedEvent> byRequest(const RequestIdStr& id) const;

  CorrelationChain buildChain(const CorrelationIdStr& id) const;
  std::vector<CorrelationChain> allChains() const;

  std::size_t eventCount() const;
  std::size_t chainCount() const;
  std::size_t activityCount() const;

  std::vector<CorrelatedEvent> getParents(const EventIdStr& event_id) const;
  std::vector<CorrelatedEvent> getChildren(const EventIdStr& event_id) const;

  void clear();

private:
  mutable std::mutex mu_;

  std::unordered_map<EventIdStr, CorrelatedEvent> events_;
  std::unordered_map<CorrelationIdStr, std::vector<EventIdStr>> by_correlation_;
  std::unordered_map<EventIdStr, std::vector<EventIdStr>> by_parent_;
  std::unordered_map<ActivityIdStr, std::vector<EventIdStr>> by_activity_;
  std::unordered_map<SessionIdStr, std::vector<EventIdStr>> by_session_;
  std::unordered_map<RequestIdStr, std::vector<EventIdStr>> by_request_;
};

}  // namespace monix::collectors::correlation
