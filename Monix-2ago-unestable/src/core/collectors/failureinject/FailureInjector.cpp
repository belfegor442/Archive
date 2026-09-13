#include "FailureInjector.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::failureinject {

const char* FailureTypeName(FailureType ft) {
  switch (ft) {
    case FailureType::CollectorCrash:       return "CollectorCrash";
    case FailureType::CollectorTimeout:     return "CollectorTimeout";
    case FailureType::EventBusCongestion:   return "EventBusCongestion";
    case FailureType::EventBusFull:         return "EventBusFull";
    case FailureType::StorageFailure:       return "StorageFailure";
    case FailureType::StorageCorruption:    return "StorageCorruption";
    case FailureType::PermissionDenied:     return "PermissionDenied";
    case FailureType::InvalidSchema:        return "InvalidSchema";
    case FailureType::CorruptedEvent:       return "CorruptedEvent";
    case FailureType::AdapterDisconnect:    return "AdapterDisconnect";
    case FailureType::NotificationFailure:  return "NotificationFailure";
    case FailureType::ResourceExhaustion:   return "ResourceExhaustion";
    case FailureType::NetworkTimeout:       return "NetworkTimeout";
    case FailureType::DiskFull:             return "DiskFull";
  }
  return "Unknown";
}

const char* FailureSeverityName(FailureSeverity fs) {
  switch (fs) {
    case FailureSeverity::Transient:  return "Transient";
    case FailureSeverity::Persistent: return "Persistent";
    case FailureSeverity::Fatal:      return "Fatal";
  }
  return "Unknown";
}

const char* RecoveryStatusName(RecoveryStatus rs) {
  switch (rs) {
    case RecoveryStatus::NotAttempted: return "NotAttempted";
    case RecoveryStatus::InProgress:   return "InProgress";
    case RecoveryStatus::Recovered:    return "Recovered";
    case RecoveryStatus::Failed:       return "Failed";
    case RecoveryStatus::Degraded:     return "Degraded";
  }
  return "Unknown";
}

bool FailureEvent::isValid() const {
  return !source.empty() && timestamp_ms > 0;
}

bool RecoveryPlan::isValid() const {
  return max_retries > 0 && max_wait_ms > 0;
}

std::string InjectionResult::summary() const {
  return "injected=" + std::string(injected ? "yes" : "no") +
    " survived=" + std::string(system_survived ? "yes" : "no") +
    " errors=" + std::to_string(error_events_generated) +
    " recovery=" + std::string(RecoveryStatusName(recovery));
}

std::string SystemState::summary() const {
  return "operational=" + std::string(operational ? "yes" : "no") +
    " failures=" + std::to_string(total_failures) +
    " recoveries=" + std::to_string(total_recoveries) +
    " errors=" + std::to_string(total_error_events);
}

FailureInjector::FailureInjector() {}
FailureInjector::~FailureInjector() {}

void FailureInjector::addRecoveryPlan(const RecoveryPlan& plan) {
  std::lock_guard<std::mutex> lock(mu_);
  plans_.push_back(plan);
}

InjectionResult FailureInjector::injectCollectorFailure(const std::string& collector, FailureSeverity sev) {
  return executeInjection(FailureType::CollectorCrash, sev, collector, "Collector failure on " + collector);
}

InjectionResult FailureInjector::injectEventBusCongestion(std::size_t queue_pressure) {
  return executeInjection(FailureType::EventBusCongestion, FailureSeverity::Transient,
    "EventBus", "Queue pressure: " + std::to_string(queue_pressure));
}

InjectionResult FailureInjector::injectStorageFailure(const std::string& reason) {
  return executeInjection(FailureType::StorageFailure, FailureSeverity::Persistent,
    "Storage", "Storage failure: " + reason);
}

InjectionResult FailureInjector::injectPermissionDenied(const std::string& resource) {
  return executeInjection(FailureType::PermissionDenied, FailureSeverity::Persistent,
    resource, "Permission denied on " + resource);
}

InjectionResult FailureInjector::injectInvalidSchema(const std::string& schema_error) {
  return executeInjection(FailureType::InvalidSchema, FailureSeverity::Transient,
    "Schema", "Invalid schema: " + schema_error);
}

InjectionResult FailureInjector::injectCorruptedEvent(const std::string& event_id) {
  return executeInjection(FailureType::CorruptedEvent, FailureSeverity::Transient,
    "Event:" + event_id, "Corrupted event data");
}

InjectionResult FailureInjector::injectAdapterDisconnect(const std::string& adapter) {
  return executeInjection(FailureType::AdapterDisconnect, FailureSeverity::Persistent,
    adapter, "External adapter disconnected");
}

InjectionResult FailureInjector::injectNotificationFailure() {
  return executeInjection(FailureType::NotificationFailure, FailureSeverity::Transient,
    "Notification", "OS notification failed");
}

SystemState FailureInjector::getState() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_;
}

std::vector<FailureEvent> FailureInjector::recentFailures(std::size_t count) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t start = failures_.size() > count ? failures_.size() - count : 0;
  return std::vector<FailureEvent>(failures_.begin() + start, failures_.end());
}

void FailureInjector::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  failures_.clear();
  plans_.clear();
}

void FailureInjector::resetState() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = SystemState{};
}

std::size_t FailureInjector::totalInjections() const {
  return total_injections_;
}

std::size_t FailureInjector::totalErrorEvents() const {
  return total_error_events_;
}

void FailureInjector::setErrorCallback(ErrorCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  error_callback_ = std::move(cb);
}

InjectionResult FailureInjector::executeInjection(FailureType type, FailureSeverity sev,
                                                   const std::string& source, const std::string& desc) {
  std::lock_guard<std::mutex> lock(mu_);
  total_injections_++;

  FailureEvent event;
  event.type = type;
  event.severity = sev;
  event.source = source;
  event.description = desc;
  event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  event.user_visible = (sev == FailureSeverity::Fatal) || (type == FailureType::PermissionDenied);

  failures_.push_back(event);
  state_.total_failures++;
  state_.active_failures++;
  total_error_events_++;
  state_.total_error_events++;

  InjectionResult result;
  result.injected = true;
  result.error_events_generated = 1;

  RecoveryStatus recovery = attemptRecovery(type);
  result.recovery = recovery;

  if (recovery == RecoveryStatus::Recovered) {
    state_.total_recoveries++;
    state_.active_failures--;
    result.system_survived = true;
  } else if (recovery == RecoveryStatus::Failed && sev == FailureSeverity::Fatal) {
    result.system_survived = false;
    state_.operational = false;
  } else {
    result.system_survived = true;
  }

  if (error_callback_) {
    error_callback_(event);
  }

  return result;
}

RecoveryStatus FailureInjector::attemptRecovery(FailureType type) {
  for (const auto& plan : plans_) {
    if (plan.for_type == type) {
      return RecoveryStatus::Recovered;
    }
  }
  if (type == FailureType::PermissionDenied || type == FailureType::DiskFull) {
    return RecoveryStatus::Failed;
  }
  return RecoveryStatus::Recovered;
}

}  // namespace monix::collectors::failureinject
