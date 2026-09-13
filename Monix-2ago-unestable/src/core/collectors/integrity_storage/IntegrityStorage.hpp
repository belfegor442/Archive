#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::integritystore {

enum class IntegrityStatus : std::uint8_t {
  Unknown,
  Verified,
  Corrupted,
  Missing,
  Unchecked
};

const char* IntegrityStatusName(IntegrityStatus s);

struct IntegrityRecord {
  std::string event_id;
  std::string hash;
  std::string canonical;
  std::int64_t verified_at_ms = 0;
  IntegrityStatus status = IntegrityStatus::Unchecked;

  bool isValid() const;
};

struct IntegrityCheckResult {
  std::size_t total_checked = 0;
  std::size_t verified = 0;
  std::size_t corrupted = 0;
  std::size_t missing = 0;

  bool allValid() const;
  std::string summary() const;
};

class IntegrityStorage {
public:
  IntegrityStorage();
  ~IntegrityStorage();

  IntegrityStorage(const IntegrityStorage&) = delete;
  IntegrityStorage& operator=(const IntegrityStorage&) = delete;

  void storeRecord(const IntegrityRecord& record);
  void removeRecord(const std::string& event_id);

  IntegrityRecord getRecord(const std::string& event_id) const;
  bool hasRecord(const std::string& event_id) const;
  IntegrityStatus getStatus(const std::string& event_id) const;

  bool verifyEvent(const std::string& event_id, const std::string& current_hash);
  IntegrityCheckResult verifyAll(const std::unordered_map<std::string, std::string>& current_hashes);

  std::size_t recordCount() const;
  std::vector<IntegrityRecord> corruptedRecords() const;

  void clear();

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, IntegrityRecord> records_;
};

}  // namespace monix::collectors::integritystore
