#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::failureinject {

enum class FailureType : std::uint8_t {
  CollectorCrash,
  CollectorTimeout,
  EventBusCongestion,
  EventBusFull,
  StorageFailure,
  StorageCorruption,
  PermissionDenied,
  InvalidSchema,
  CorruptedEvent,
  AdapterDisconnect,
  NotificationFailure,
  ResourceExhaustion,
  NetworkTimeout,
  DiskFull
};

const char* FailureTypeName(FailureType ft);

enum class FailureSeverity : std::uint8_t {
  Transient,
  Persistent,
  Fatal
};

const char* FailureSeverityName(FailureSeverity fs);

enum class RecoveryStatus : std::uint8_t {
  NotAttempted,
  InProgress,
  Recovered,
  Failed,
  Degraded
};

const char* RecoveryStatusName(RecoveryStatus rs);

struct FailureEvent {
  FailureType type = FailureType::CollectorCrash;
  FailureSeverity severity = FailureSeverity::Transient;
  std::string source;
  std::string description;
  std::int64_t timestamp_ms = 0;
  std::int64_t duration_ms = 0;
  bool user_visible = false;

  bool isValid() const;
};

struct RecoveryPlan {
  FailureType for_type = FailureType::CollectorCrash;
  std::string action;
  std::int64_t max_wait_ms = 5000;
  std::size_t max_retries = 3;
  bool escalate_on_fail = true;

  bool isValid() const;
};

struct InjectionResult {
  bool injected = false;
  bool system_survived = true;
  std::size_t error_events_generated = 0;
  RecoveryStatus recovery = RecoveryStatus::NotAttempted;
  std::string summary() const;
};

struct SystemState {
  bool operational = true;
  std::size_t total_failures = 0;
  std::size_t total_recoveries = 0;
  std::size_t total_error_events = 0;
  std::size_t active_failures = 0;
  bool has_critical_loss = false;

  std::string summary() const;
};

class FailureInjector {
public:
  FailureInjector();
  ~FailureInjector();

  FailureInjector(const FailureInjector&) = delete;
  FailureInjector& operator=(const FailureInjector&) = delete;

  void addRecoveryPlan(const RecoveryPlan& plan);

  InjectionResult injectCollectorFailure(const std::string& collector, FailureSeverity sev);
  InjectionResult injectEventBusCongestion(std::size_t queue_pressure);
  InjectionResult injectStorageFailure(const std::string& reason);
  InjectionResult injectPermissionDenied(const std::string& resource);
  InjectionResult injectInvalidSchema(const std::string& schema_error);
  InjectionResult injectCorruptedEvent(const std::string& event_id);
  InjectionResult injectAdapterDisconnect(const std::string& adapter);
  InjectionResult injectNotificationFailure();

  SystemState getState() const;
  std::vector<FailureEvent> recentFailures(std::size_t count = 10) const;

  void clear();
  void resetState();

  std::size_t totalInjections() const;
  std::size_t totalErrorEvents() const;

  using ErrorCallback = std::function<void(const FailureEvent&)>;
  void setErrorCallback(ErrorCallback cb);

private:
  InjectionResult executeInjection(FailureType type, FailureSeverity sev,
                                   const std::string& source, const std::string& desc);
  RecoveryStatus attemptRecovery(FailureType type);

  mutable std::mutex mu_;
  std::vector<RecoveryPlan> plans_;
  std::vector<FailureEvent> failures_;
  SystemState state_;
  std::atomic<std::size_t> total_injections_{0};
  std::atomic<std::size_t> total_error_events_{0};
  ErrorCallback error_callback_;
};

}  // namespace monix::collectors::failureinject
