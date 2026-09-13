#include "ForensicMode.hpp"

#include <chrono>
#include <mutex>
#include <sstream>

namespace monix::collectors::forensic {

const char* ForensicLevelName(ForensicLevel l) {
  switch (l) {
    case ForensicLevel::Disabled:  return "Disabled";
    case ForensicLevel::Minimal:   return "Minimal";
    case ForensicLevel::Standard:  return "Standard";
    case ForensicLevel::Enhanced:  return "Enhanced";
    case ForensicLevel::Full:      return "Full";
  }
  return "Unknown";
}

ForensicLevel ForensicLevelFromName(const std::string& name) {
  if (name == "Disabled")  return ForensicLevel::Disabled;
  if (name == "Minimal")   return ForensicLevel::Minimal;
  if (name == "Standard")  return ForensicLevel::Standard;
  if (name == "Enhanced")  return ForensicLevel::Enhanced;
  if (name == "Full")      return ForensicLevel::Full;
  return ForensicLevel::Disabled;
}

bool ForensicConfig::isValid() const {
  if (memory_limit_mb == 0) return false;
  if (queue_limit == 0) return false;
  if (rate_limit_per_second == 0) return false;
  return true;
}

std::string ForensicConfig::summary() const {
  std::ostringstream oss;
  oss << "ForensicConfig level=" << ForensicLevelName(level);
  oss << " fs=" << filesystem_events;
  oss << " proc=" << process_metadata;
  oss << " hash=" << hash_files;
  oss << " snap=" << additional_snapshots;
  oss << " prov=" << extended_provenance;
  oss << " diag=" << diagnostic_info;
  oss << " mem=" << memory_limit_mb << "MB";
  oss << " q=" << queue_limit;
  return oss.str();
}

ForensicConfig ForensicConfig::defaultForLevel(ForensicLevel level) {
  ForensicConfig cfg;
  cfg.level = level;

  switch (level) {
    case ForensicLevel::Disabled:
      break;
    case ForensicLevel::Minimal:
      cfg.filesystem_events = true;
      cfg.memory_limit_mb = 128;
      cfg.queue_limit = 5000;
      cfg.rate_limit_per_second = 500;
      break;
    case ForensicLevel::Standard:
      cfg.filesystem_events = true;
      cfg.process_metadata = true;
      cfg.hash_files = true;
      cfg.memory_limit_mb = 256;
      cfg.queue_limit = 10000;
      cfg.rate_limit_per_second = 1000;
      cfg.snapshot_interval_ms = 60000;
      break;
    case ForensicLevel::Enhanced:
      cfg.filesystem_events = true;
      cfg.process_metadata = true;
      cfg.hash_files = true;
      cfg.additional_snapshots = true;
      cfg.extended_provenance = true;
      cfg.memory_limit_mb = 512;
      cfg.queue_limit = 25000;
      cfg.rate_limit_per_second = 2000;
      cfg.snapshot_interval_ms = 30000;
      break;
    case ForensicLevel::Full:
      cfg.filesystem_events = true;
      cfg.process_metadata = true;
      cfg.hash_files = true;
      cfg.additional_snapshots = true;
      cfg.extended_provenance = true;
      cfg.diagnostic_info = true;
      cfg.memory_limit_mb = 1024;
      cfg.queue_limit = 50000;
      cfg.rate_limit_per_second = 5000;
      cfg.snapshot_interval_ms = 10000;
      break;
  }

  return cfg;
}

bool ForensicState::isActive() const {
  return active;
}

std::string ForensicState::summary() const {
  std::ostringstream oss;
  oss << "ForensicState level=" << ForensicLevelName(current_level);
  oss << " active=" << active;
  oss << " events=" << events_generated;
  oss << " snaps=" << snapshots_taken;
  oss << " hashes=" << hashes_computed;
  return oss.str();
}

ForensicMode::ForensicMode() {}
ForensicMode::ForensicMode(const ForensicConfig& cfg) : config_(cfg) {}
ForensicMode::~ForensicMode() {}

void ForensicMode::setConfig(const ForensicConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

ForensicConfig ForensicMode::getConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

bool ForensicMode::activate(ForensicLevel level) {
  std::lock_guard<std::mutex> lock(mu_);
  if (state_.active) return false;

  config_ = ForensicConfig::defaultForLevel(level);
  state_.active = true;
  state_.current_level = level;
  state_.activated_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  return true;
}

bool ForensicMode::deactivate() {
  std::lock_guard<std::mutex> lock(mu_);
  if (!state_.active) return false;
  state_.active = false;
  state_.current_level = ForensicLevel::Disabled;
  config_.level = ForensicLevel::Disabled;
  config_.filesystem_events = false;
  config_.process_metadata = false;
  config_.hash_files = false;
  config_.additional_snapshots = false;
  config_.extended_provenance = false;
  config_.diagnostic_info = false;
  return true;
}

ForensicLevel ForensicMode::currentLevel() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.current_level;
}

bool ForensicMode::isActive() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active;
}

bool ForensicMode::isDisabled() const {
  return !isActive();
}

bool ForensicMode::shouldCollectFilesystem() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active && config_.filesystem_events;
}

bool ForensicMode::shouldCollectProcessMetadata() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active && config_.process_metadata;
}

bool ForensicMode::shouldHash() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active && config_.hash_files;
}

bool ForensicMode::shouldTakeSnapshots() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active && config_.additional_snapshots;
}

bool ForensicMode::shouldExtendProvenance() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active && config_.extended_provenance;
}

bool ForensicMode::shouldCollectDiagnostics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_.active && config_.diagnostic_info;
}

void ForensicMode::recordEvent() {
  std::lock_guard<std::mutex> lock(mu_);
  state_.events_generated++;
}

void ForensicMode::recordSnapshot() {
  std::lock_guard<std::mutex> lock(mu_);
  state_.snapshots_taken++;
}

void ForensicMode::recordHash() {
  std::lock_guard<std::mutex> lock(mu_);
  state_.hashes_computed++;
}

ForensicState ForensicMode::getState() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_;
}

bool ForensicMode::withinMemoryLimit(std::size_t current_bytes) const {
  std::lock_guard<std::mutex> lock(mu_);
  return current_bytes <= config_.memory_limit_mb * 1024 * 1024;
}

bool ForensicMode::withinQueueLimit(std::size_t current_depth) const {
  std::lock_guard<std::mutex> lock(mu_);
  return current_depth <= config_.queue_limit;
}

bool ForensicMode::withinRateLimit(std::size_t current_rate) const {
  std::lock_guard<std::mutex> lock(mu_);
  return current_rate <= config_.rate_limit_per_second;
}

}  // namespace monix::collectors::forensic
