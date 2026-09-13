#include "EventCorrelation.hpp"

#include <algorithm>

namespace monix::collectors::correlation {

bool CorrelatedEvent::isValid() const {
  return !event_id.empty();
}

std::string CorrelatedEvent::summary() const {
  std::string result = event_id;
  if (!event_type.empty()) result += " [" + event_type + "]";
  if (!correlation_id.empty()) result += " corr=" + correlation_id;
  if (!parent_event_id.empty()) result += " parent=" + parent_event_id;
  if (!activity_id.empty()) result += " act=" + activity_id;
  return result;
}

bool CorrelationChain::isValid() const {
  return !id.empty() && !event_ids.empty();
}

std::size_t CorrelationChain::size() const {
  return event_ids.size();
}

std::string CorrelationChain::summary() const {
  return "Chain " + id + " (" + std::to_string(event_ids.size()) + " events)";
}

CorrelationEngine::CorrelationEngine() {}
CorrelationEngine::~CorrelationEngine() {}

void CorrelationEngine::addEvent(const CorrelatedEvent& event) {
  if (!event.isValid()) return;

  std::lock_guard<std::mutex> lock(mu_);
  events_[event.event_id] = event;

  if (!event.correlation_id.empty()) {
    by_correlation_[event.correlation_id].push_back(event.event_id);
  }
  if (!event.parent_event_id.empty()) {
    by_parent_[event.parent_event_id].push_back(event.event_id);
  }
  if (!event.activity_id.empty()) {
    by_activity_[event.activity_id].push_back(event.event_id);
  }
  if (!event.session_id.empty()) {
    by_session_[event.session_id].push_back(event.event_id);
  }
  if (!event.request_id.empty()) {
    by_request_[event.request_id].push_back(event.event_id);
  }
}

void CorrelationEngine::removeEvent(const EventIdStr& event_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = events_.find(event_id);
  if (it == events_.end()) return;

  const auto& event = it->second;

  auto remove_from = [&](auto& map, const auto& key) {
    auto map_it = map.find(key);
    if (map_it != map.end()) {
      auto& vec = map_it->second;
      vec.erase(std::remove(vec.begin(), vec.end(), event_id), vec.end());
      if (vec.empty()) map.erase(map_it);
    }
  };

  if (!event.correlation_id.empty()) remove_from(by_correlation_, event.correlation_id);
  if (!event.parent_event_id.empty()) remove_from(by_parent_, event.parent_event_id);
  if (!event.activity_id.empty()) remove_from(by_activity_, event.activity_id);
  if (!event.session_id.empty()) remove_from(by_session_, event.session_id);
  if (!event.request_id.empty()) remove_from(by_request_, event.request_id);

  events_.erase(it);
}

CorrelatedEvent CorrelationEngine::getEvent(const EventIdStr& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = events_.find(event_id);
  if (it == events_.end()) return CorrelatedEvent{};
  return it->second;
}

bool CorrelationEngine::hasEvent(const EventIdStr& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  return events_.count(event_id) > 0;
}

std::vector<CorrelatedEvent> CorrelationEngine::byCorrelationId(const CorrelationIdStr& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = by_correlation_.find(id);
  if (it == by_correlation_.end()) return result;
  for (const auto& eid : it->second) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) result.push_back(eit->second);
  }
  return result;
}

std::vector<CorrelatedEvent> CorrelationEngine::byParentEvent(const EventIdStr& parent_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = by_parent_.find(parent_id);
  if (it == by_parent_.end()) return result;
  for (const auto& eid : it->second) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) result.push_back(eit->second);
  }
  return result;
}

std::vector<CorrelatedEvent> CorrelationEngine::byActivity(const ActivityIdStr& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = by_activity_.find(id);
  if (it == by_activity_.end()) return result;
  for (const auto& eid : it->second) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) result.push_back(eit->second);
  }
  return result;
}

std::vector<CorrelatedEvent> CorrelationEngine::bySession(const SessionIdStr& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = by_session_.find(id);
  if (it == by_session_.end()) return result;
  for (const auto& eid : it->second) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) result.push_back(eit->second);
  }
  return result;
}

std::vector<CorrelatedEvent> CorrelationEngine::byRequest(const RequestIdStr& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = by_request_.find(id);
  if (it == by_request_.end()) return result;
  for (const auto& eid : it->second) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) result.push_back(eit->second);
  }
  return result;
}

CorrelationChain CorrelationEngine::buildChain(const CorrelationIdStr& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  CorrelationChain chain;
  chain.id = id;

  auto it = by_correlation_.find(id);
  if (it == by_correlation_.end()) return chain;

  chain.event_ids = it->second;

  std::int64_t min_ms = std::numeric_limits<std::int64_t>::max();
  std::int64_t max_ms = std::numeric_limits<std::int64_t>::min();
  for (const auto& eid : chain.event_ids) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) {
      if (eit->second.timestamp_ms > 0 && eit->second.timestamp_ms < min_ms) min_ms = eit->second.timestamp_ms;
      if (eit->second.timestamp_ms > max_ms) max_ms = eit->second.timestamp_ms;
    }
  }
  chain.started_ms = (min_ms == std::numeric_limits<std::int64_t>::max()) ? 0 : min_ms;
  chain.ended_ms = (max_ms == std::numeric_limits<std::int64_t>::min()) ? 0 : max_ms;

  return chain;
}

std::vector<CorrelationChain> CorrelationEngine::allChains() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelationChain> result;
  for (const auto& [cid, ids] : by_correlation_) {
    CorrelationChain chain;
    chain.id = cid;
    chain.event_ids = ids;
    for (const auto& eid : ids) {
      auto eit = events_.find(eid);
      if (eit != events_.end()) {
        if (eit->second.timestamp_ms > 0) {
          if (chain.started_ms == 0 || eit->second.timestamp_ms < chain.started_ms)
            chain.started_ms = eit->second.timestamp_ms;
          if (eit->second.timestamp_ms > chain.ended_ms)
            chain.ended_ms = eit->second.timestamp_ms;
        }
      }
    }
    result.push_back(chain);
  }
  return result;
}

std::size_t CorrelationEngine::eventCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return events_.size();
}

std::size_t CorrelationEngine::chainCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return by_correlation_.size();
}

std::size_t CorrelationEngine::activityCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return by_activity_.size();
}

std::vector<CorrelatedEvent> CorrelationEngine::getParents(const EventIdStr& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = events_.find(event_id);
  if (it == events_.end()) return result;
  if (it->second.parent_event_id.empty()) return result;
  auto pit = events_.find(it->second.parent_event_id);
  if (pit != events_.end()) result.push_back(pit->second);
  return result;
}

std::vector<CorrelatedEvent> CorrelationEngine::getChildren(const EventIdStr& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CorrelatedEvent> result;
  auto it = by_parent_.find(event_id);
  if (it == by_parent_.end()) return result;
  for (const auto& eid : it->second) {
    auto eit = events_.find(eid);
    if (eit != events_.end()) result.push_back(eit->second);
  }
  return result;
}

void CorrelationEngine::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  events_.clear();
  by_correlation_.clear();
  by_parent_.clear();
  by_activity_.clear();
  by_session_.clear();
  by_request_.clear();
}

}  // namespace monix::collectors::correlation
