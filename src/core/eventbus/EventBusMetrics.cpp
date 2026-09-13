#include "EventBusMetrics.hpp"

namespace monix::eventbus {

void EventBusMetrics::recordReceived(const std::string& source, events::Timestamp now) {
  std::lock_guard<std::mutex> lock(mu_);
  totalReceived_++;
  rateWindowReceived_++;
  if (!source.empty()) sourceMap_[source].received++;
  if (now > 0 && rateWindowStart_ == 0) rateWindowStart_ = now;
}

void EventBusMetrics::recordAccepted(const std::string& source, events::Timestamp now) {
  std::lock_guard<std::mutex> lock(mu_);
  totalAccepted_++;
  rateWindowAccepted_++;
  if (!source.empty()) sourceMap_[source].accepted++;
}

void EventBusMetrics::recordRejected(const std::string& source, events::Timestamp now) {
  std::lock_guard<std::mutex> lock(mu_);
  totalRejected_++;
  if (!source.empty()) {
    auto& sm = sourceMap_[source];
    sm.received++;
  }
}

void EventBusMetrics::recordDropped(const std::string& source, const std::string& reason, events::Timestamp now) {
  std::lock_guard<std::mutex> lock(mu_);
  totalDropped_++;
  if (!source.empty()) sourceMap_[source].dropped++;

  LossRecord rec;
  rec.source = source;
  rec.reason = reason;
  rec.count = 1;
  rec.first_occurrence = now;
  rec.last_occurrence = now;

  for (auto& lr : losses_) {
    if (lr.source == source && lr.reason == reason) {
      lr.count++;
      lr.last_occurrence = now;
      return;
    }
  }
  losses_.push_back(rec);
}

void EventBusMetrics::recordConsumerFailure() {
  std::lock_guard<std::mutex> lock(mu_);
  consumerFailures_++;
}

void EventBusMetrics::recordRetry() {
  std::lock_guard<std::mutex> lock(mu_);
  retryCount_++;
}

void EventBusMetrics::updateQueueDepth(std::size_t depth) {
  std::lock_guard<std::mutex> lock(mu_);
  queueDepth_ = depth;
}

QueueMetrics EventBusMetrics::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  QueueMetrics m;
  m.events_received = totalReceived_;
  m.events_accepted = totalAccepted_;
  m.events_rejected = totalRejected_;
  m.events_dropped = totalDropped_;
  m.queue_depth = queueDepth_;
  m.consumer_failures = consumerFailures_;
  m.retry_count = retryCount_;
  m.ingestion_rate = currentIngestionRate_;
  m.dispatch_rate = currentDispatchRate_;
  return m;
}

std::unordered_map<std::string, SourceMetrics> EventBusMetrics::sourceMetrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return sourceMap_;
}

std::vector<LossRecord> EventBusMetrics::lossRecords() const {
  std::lock_guard<std::mutex> lock(mu_);
  return losses_;
}

void EventBusMetrics::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  totalReceived_ = 0;
  totalAccepted_ = 0;
  totalRejected_ = 0;
  totalDropped_ = 0;
  consumerFailures_ = 0;
  retryCount_ = 0;
  queueDepth_ = 0;
  sourceMap_.clear();
  losses_.clear();
  rateWindowReceived_ = 0;
  rateWindowAccepted_ = 0;
  rateWindowStart_ = 0;
  currentIngestionRate_ = 0;
  currentDispatchRate_ = 0;
}

}  // namespace monix::eventbus
