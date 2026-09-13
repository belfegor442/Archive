#include "IntegrityStorage.hpp"

#include <chrono>

namespace monix::collectors::integritystore {

const char* IntegrityStatusName(IntegrityStatus s) {
  switch (s) {
    case IntegrityStatus::Unknown:   return "Unknown";
    case IntegrityStatus::Verified:  return "Verified";
    case IntegrityStatus::Corrupted: return "Corrupted";
    case IntegrityStatus::Missing:   return "Missing";
    case IntegrityStatus::Unchecked: return "Unchecked";
  }
  return "Unknown";
}

bool IntegrityRecord::isValid() const {
  return !event_id.empty() && !hash.empty();
}

bool IntegrityCheckResult::allValid() const {
  return corrupted == 0 && missing == 0;
}

std::string IntegrityCheckResult::summary() const {
  return "checked=" + std::to_string(total_checked) +
    " verified=" + std::to_string(verified) +
    " corrupted=" + std::to_string(corrupted) +
    " missing=" + std::to_string(missing);
}

IntegrityStorage::IntegrityStorage() {}
IntegrityStorage::~IntegrityStorage() {}

void IntegrityStorage::storeRecord(const IntegrityRecord& record) {
  std::lock_guard<std::mutex> lock(mu_);
  records_[record.event_id] = record;
}

void IntegrityStorage::removeRecord(const std::string& event_id) {
  std::lock_guard<std::mutex> lock(mu_);
  records_.erase(event_id);
}

IntegrityRecord IntegrityStorage::getRecord(const std::string& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = records_.find(event_id);
  if (it == records_.end()) return IntegrityRecord{};
  return it->second;
}

bool IntegrityStorage::hasRecord(const std::string& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  return records_.count(event_id) > 0;
}

IntegrityStatus IntegrityStorage::getStatus(const std::string& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = records_.find(event_id);
  if (it == records_.end()) return IntegrityStatus::Missing;
  return it->second.status;
}

bool IntegrityStorage::verifyEvent(const std::string& event_id, const std::string& current_hash) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = records_.find(event_id);
  if (it == records_.end()) return false;

  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  if (it->second.hash == current_hash) {
    it->second.status = IntegrityStatus::Verified;
    it->second.verified_at_ms = now_ms;
    return true;
  } else {
    it->second.status = IntegrityStatus::Corrupted;
    it->second.verified_at_ms = now_ms;
    return false;
  }
}

IntegrityCheckResult IntegrityStorage::verifyAll(
    const std::unordered_map<std::string, std::string>& current_hashes) {
  std::lock_guard<std::mutex> lock(mu_);
  IntegrityCheckResult result;
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  for (auto& [event_id, record] : records_) {
    result.total_checked++;
    auto it = current_hashes.find(event_id);
    if (it == current_hashes.end()) {
      record.status = IntegrityStatus::Missing;
      result.missing++;
      continue;
    }
    if (record.hash == it->second) {
      record.status = IntegrityStatus::Verified;
      record.verified_at_ms = now_ms;
      result.verified++;
    } else {
      record.status = IntegrityStatus::Corrupted;
      record.verified_at_ms = now_ms;
      result.corrupted++;
    }
  }

  return result;
}

std::size_t IntegrityStorage::recordCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return records_.size();
}

std::vector<IntegrityRecord> IntegrityStorage::corruptedRecords() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<IntegrityRecord> result;
  for (const auto& [id, record] : records_) {
    if (record.status == IntegrityStatus::Corrupted) {
      result.push_back(record);
    }
  }
  return result;
}

void IntegrityStorage::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  records_.clear();
}

}  // namespace monix::collectors::integritystore
