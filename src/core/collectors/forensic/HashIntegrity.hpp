#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::forensic {

enum class HashMode : std::uint8_t {
  Disabled,
  OnDemand,
  Suspicious,
  Forensic,
  Always
};

const char* HashModeName(HashMode m);
HashMode HashModeFromName(const std::string& name);

enum class HashEventKind : std::uint8_t {
  Calculated,
  IntegrityChanged
};

const char* HashEventKindName(HashEventKind k);
std::string HashEventKindAction(HashEventKind k);

struct HashConfig {
  HashMode mode = HashMode::Disabled;
  std::size_t max_file_size_mb = 100;
  std::size_t chunk_size_bytes = 1024 * 1024;
  std::size_t max_concurrent_hashes = 4;
  bool log_calculated_events = true;
  bool log_changed_events = true;

  bool isValid() const;
};

struct HashEvent {
  std::string file_path;
  std::string hash_algorithm;
  std::string hash_value;
  HashEventKind kind = HashEventKind::Calculated;
  std::size_t file_size = 0;
  bool integrity_changed = false;
  std::string previous_hash;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
  std::string action() const;
};

struct FileHashResult {
  std::string file_path;
  std::string algorithm;
  std::string hash;
  std::size_t file_size = 0;
  bool success = false;
  std::string error;
  std::int64_t computed_at_ms = 0;

  bool isValid() const;
};

using HashCallback = std::function<void(const HashEvent&)>;

class HashIntegrity {
public:
  HashIntegrity();
  explicit HashIntegrity(const HashConfig& cfg);
  ~HashIntegrity();

  HashIntegrity(const HashIntegrity&) = delete;
  HashIntegrity& operator=(const HashIntegrity&) = delete;

  void setCallback(HashCallback cb);
  void setConfig(const HashConfig& cfg);
  HashConfig getConfig() const;

  bool shouldHash(const std::string& file_path, std::size_t file_size) const;

  HashEvent computeHash(const std::string& file_path, const std::vector<std::uint8_t>& data);
  HashEvent computeHashChunked(const std::string& file_path, const std::vector<std::vector<std::uint8_t>>& chunks);

  bool storeBaseline(const std::string& file_path, const std::string& hash);
  bool verifyIntegrity(const std::string& file_path, const std::string& current_hash);

  HashMode currentMode() const;
  bool isDisabled() const;
  bool isForensic() const;

  std::size_t eventsEmitted() const;
  std::size_t baselineCount() const;

  void clearBaselines();
  void clear();

private:
  void emit(const HashEvent& event);
  static std::string computeSHA256(const std::vector<std::uint8_t>& data);
  static std::string computeSHA256Chunked(const std::vector<std::vector<std::uint8_t>>& chunks);

  HashCallback callback_;
  mutable std::mutex mu_;
  HashConfig config_;
  std::unordered_map<std::string, std::string> baselines_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::forensic
