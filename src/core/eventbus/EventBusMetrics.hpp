#pragma once

#include "BackpressureController.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace monix::eventbus {

struct QueueMetrics {
  std::uint64_t events_received = 0;
  std::uint64_t events_accepted = 0;
  std::uint64_t events_rejected = 0;
  std::uint64_t events_dropped = 0;
  std::uint64_t queue_depth = 0;
  std::uint64_t queue_capacity = 0;
  QueuePressure pressure = QueuePressure::Normal;
  double ingestion_rate = 0.0;
  double dispatch_rate = 0.0;
  std::uint64_t consumer_failures = 0;
  std::uint64_t retry_count = 0;
};

struct LossRecord {
  std::string source;
  std::string reason;
  std::uint64_t count = 0;
  events::Timestamp first_occurrence = 0;
  events::Timestamp last_occurrence = 0;
};

struct SourceMetrics {
  std::uint64_t received = 0;
  std::uint64_t accepted = 0;
  std::uint64_t dropped = 0;
};

class EventBusMetrics {
public:
  void recordReceived(const std::string& source = "", events::Timestamp now = 0);
  void recordAccepted(const std::string& source = "", events::Timestamp now = 0);
  void recordRejected(const std::string& source = "", events::Timestamp now = 0);
  void recordDropped(const std::string& source = "", const std::string& reason = "", events::Timestamp now = 0);
  void recordConsumerFailure();
  void recordRetry();
  void updateQueueDepth(std::size_t depth);

  QueueMetrics snapshot() const;
  std::unordered_map<std::string, SourceMetrics> sourceMetrics() const;
  std::vector<LossRecord> lossRecords() const;

  void reset();

private:
  mutable std::mutex mu_;
  std::uint64_t totalReceived_ = 0;
  std::uint64_t totalAccepted_ = 0;
  std::uint64_t totalRejected_ = 0;
  std::uint64_t totalDropped_ = 0;
  std::uint64_t consumerFailures_ = 0;
  std::uint64_t retryCount_ = 0;
  std::uint64_t queueDepth_ = 0;

  std::unordered_map<std::string, SourceMetrics> sourceMap_;
  std::vector<LossRecord> losses_;

  std::uint64_t rateWindowReceived_ = 0;
  std::uint64_t rateWindowAccepted_ = 0;
  events::Timestamp rateWindowStart_ = 0;
  double currentIngestionRate_ = 0.0;
  double currentDispatchRate_ = 0.0;
};

}  // namespace monix::eventbus
