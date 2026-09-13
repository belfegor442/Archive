#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace monix::collectors::forensic {

enum class ForensicLevel : std::uint8_t {
  Disabled = 0,
  Minimal = 1,
  Standard = 2,
  Enhanced = 3,
  Full = 4
};

const char* ForensicLevelName(ForensicLevel l);
ForensicLevel ForensicLevelFromName(const std::string& name);

struct ForensicConfig {
  ForensicLevel level = ForensicLevel::Disabled;

  bool filesystem_events = false;
  bool process_metadata = false;
  bool hash_files = false;
  bool additional_snapshots = false;
  bool extended_provenance = false;
  bool diagnostic_info = false;

  std::size_t memory_limit_mb = 256;
  std::size_t queue_limit = 10000;
  std::size_t rate_limit_per_second = 1000;
  std::size_t snapshot_interval_ms = 60000;

  bool isValid() const;
  std::string summary() const;

  static ForensicConfig defaultForLevel(ForensicLevel level);
};

struct ForensicState {
  bool active = false;
  ForensicLevel current_level = ForensicLevel::Disabled;
  std::int64_t activated_at_ms = 0;
  std::size_t events_generated = 0;
  std::size_t snapshots_taken = 0;
  std::size_t hashes_computed = 0;

  bool isActive() const;
  std::string summary() const;
};

class ForensicMode {
public:
  ForensicMode();
  explicit ForensicMode(const ForensicConfig& cfg);
  ~ForensicMode();

  ForensicMode(const ForensicMode&) = delete;
  ForensicMode& operator=(const ForensicMode&) = delete;

  void setConfig(const ForensicConfig& cfg);
  ForensicConfig getConfig() const;

  bool activate(ForensicLevel level = ForensicLevel::Standard);
  bool deactivate();

  ForensicLevel currentLevel() const;
  bool isActive() const;
  bool isDisabled() const;

  bool shouldCollectFilesystem() const;
  bool shouldCollectProcessMetadata() const;
  bool shouldHash() const;
  bool shouldTakeSnapshots() const;
  bool shouldExtendProvenance() const;
  bool shouldCollectDiagnostics() const;

  void recordEvent();
  void recordSnapshot();
  void recordHash();

  ForensicState getState() const;

  bool withinMemoryLimit(std::size_t current_bytes) const;
  bool withinQueueLimit(std::size_t current_depth) const;
  bool withinRateLimit(std::size_t current_rate) const;

private:
  ForensicConfig config_;
  ForensicState state_;
  mutable std::mutex mu_;
};

}  // namespace monix::collectors::forensic
