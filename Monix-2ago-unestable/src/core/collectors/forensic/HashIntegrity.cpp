#include "HashIntegrity.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace monix::collectors::forensic {

const char* HashModeName(HashMode m) {
  switch (m) {
    case HashMode::Disabled:  return "Disabled";
    case HashMode::OnDemand:  return "OnDemand";
    case HashMode::Suspicious: return "Suspicious";
    case HashMode::Forensic:  return "Forensic";
    case HashMode::Always:    return "Always";
  }
  return "Unknown";
}

HashMode HashModeFromName(const std::string& name) {
  if (name == "Disabled")  return HashMode::Disabled;
  if (name == "OnDemand")  return HashMode::OnDemand;
  if (name == "Suspicious") return HashMode::Suspicious;
  if (name == "Forensic")  return HashMode::Forensic;
  if (name == "Always")    return HashMode::Always;
  return HashMode::Disabled;
}

const char* HashEventKindName(HashEventKind k) {
  switch (k) {
    case HashEventKind::Calculated:       return "Calculated";
    case HashEventKind::IntegrityChanged: return "IntegrityChanged";
  }
  return "Unknown";
}

std::string HashEventKindAction(HashEventKind k) {
  switch (k) {
    case HashEventKind::Calculated:       return "file.hash.calculated";
    case HashEventKind::IntegrityChanged: return "file.integrity.changed";
  }
  return "unknown";
}

bool HashConfig::isValid() const {
  if (max_file_size_mb == 0) return false;
  if (chunk_size_bytes == 0) return false;
  return true;
}

bool HashEvent::isValid() const {
  return !file_path.empty();
}

std::string HashEvent::summary() const {
  std::string result = file_path + " " + std::string(HashEventKindName(kind));
  result += " algo=" + hash_algorithm;
  if (!hash_value.empty()) result += " hash=" + hash_value.substr(0, 16) + "...";
  if (integrity_changed) result += " CHANGED";
  return result;
}

std::string HashEvent::action() const {
  return HashEventKindAction(kind);
}

bool FileHashResult::isValid() const {
  return !file_path.empty() && success;
}

HashIntegrity::HashIntegrity() {}
HashIntegrity::HashIntegrity(const HashConfig& cfg) : config_(cfg) {}
HashIntegrity::~HashIntegrity() {}

void HashIntegrity::setCallback(HashCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void HashIntegrity::setConfig(const HashConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

HashConfig HashIntegrity::getConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

bool HashIntegrity::shouldHash(const std::string& file_path, std::size_t file_size) const {
  std::lock_guard<std::mutex> lock(mu_);
  switch (config_.mode) {
    case HashMode::Disabled: return false;
    case HashMode::Always: return file_size <= config_.max_file_size_mb * 1024 * 1024;
    case HashMode::OnDemand: return false;
    case HashMode::Suspicious: return false;
    case HashMode::Forensic: return file_size <= config_.max_file_size_mb * 1024 * 1024;
  }
  return false;
}

std::string HashIntegrity::computeSHA256(const std::vector<std::uint8_t>& data) {
  BCRYPT_ALG_HANDLE algHandle = nullptr;
  BCRYPT_HASH_HANDLE hashHandle = nullptr;

  NTSTATUS status = BCryptOpenAlgorithmProvider(&algHandle, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (!BCRYPT_SUCCESS(status)) return "";

  status = BCryptCreateHash(algHandle, &hashHandle, nullptr, 0, nullptr, 0, 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return "";
  }

  status = BCryptHashData(hashHandle, const_cast<PUCHAR>(data.data()),
    static_cast<ULONG>(data.size()), 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptDestroyHash(hashHandle);
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return "";
  }

  std::vector<std::uint8_t> hash(32);
  status = BCryptFinishHash(hashHandle, hash.data(), 32, 0);
  BCryptDestroyHash(hashHandle);
  BCryptCloseAlgorithmProvider(algHandle, 0);

  if (!BCRYPT_SUCCESS(status)) return "";

  std::ostringstream oss;
  for (auto b : hash) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
  }
  return oss.str();
}

std::string HashIntegrity::computeSHA256Chunked(const std::vector<std::vector<std::uint8_t>>& chunks) {
  BCRYPT_ALG_HANDLE algHandle = nullptr;
  BCRYPT_HASH_HANDLE hashHandle = nullptr;

  NTSTATUS status = BCryptOpenAlgorithmProvider(&algHandle, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (!BCRYPT_SUCCESS(status)) return "";

  status = BCryptCreateHash(algHandle, &hashHandle, nullptr, 0, nullptr, 0, 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return "";
  }

  for (const auto& chunk : chunks) {
    status = BCryptHashData(hashHandle, const_cast<PUCHAR>(chunk.data()),
      static_cast<ULONG>(chunk.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
      BCryptDestroyHash(hashHandle);
      BCryptCloseAlgorithmProvider(algHandle, 0);
      return "";
    }
  }

  std::vector<std::uint8_t> hash(32);
  status = BCryptFinishHash(hashHandle, hash.data(), 32, 0);
  BCryptDestroyHash(hashHandle);
  BCryptCloseAlgorithmProvider(algHandle, 0);

  if (!BCRYPT_SUCCESS(status)) return "";

  std::ostringstream oss;
  for (auto b : hash) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
  }
  return oss.str();
}

HashEvent HashIntegrity::computeHash(const std::string& file_path, const std::vector<std::uint8_t>& data) {
  HashEvent event;
  event.file_path = file_path;
  event.hash_algorithm = "SHA-256";
  event.file_size = data.size();
  event.hash_value = computeSHA256(data);
  event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  if (event.hash_value.empty()) {
    return event;
  }

  std::lock_guard<std::mutex> lock(mu_);
  auto it = baselines_.find(file_path);
  if (it != baselines_.end()) {
    event.integrity_changed = (it->second != event.hash_value);
    event.previous_hash = it->second;
    if (event.integrity_changed) {
      event.kind = HashEventKind::IntegrityChanged;
      emit(event);
      return event;
    }
  }

  event.kind = HashEventKind::Calculated;
  if (config_.log_calculated_events) {
    emit(event);
  }
  return event;
}

HashEvent HashIntegrity::computeHashChunked(const std::string& file_path,
    const std::vector<std::vector<std::uint8_t>>& chunks) {
  HashEvent event;
  event.file_path = file_path;
  event.hash_algorithm = "SHA-256";
  event.hash_value = computeSHA256Chunked(chunks);

  for (const auto& chunk : chunks) {
    event.file_size += chunk.size();
  }

  event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  if (event.hash_value.empty()) return event;

  std::lock_guard<std::mutex> lock(mu_);
  auto it = baselines_.find(file_path);
  if (it != baselines_.end()) {
    event.integrity_changed = (it->second != event.hash_value);
    event.previous_hash = it->second;
    if (event.integrity_changed) {
      event.kind = HashEventKind::IntegrityChanged;
      emit(event);
      return event;
    }
  }

  event.kind = HashEventKind::Calculated;
  if (config_.log_calculated_events) {
    emit(event);
  }
  return event;
}

bool HashIntegrity::storeBaseline(const std::string& file_path, const std::string& hash) {
  std::lock_guard<std::mutex> lock(mu_);
  baselines_[file_path] = hash;
  return true;
}

bool HashIntegrity::verifyIntegrity(const std::string& file_path, const std::string& current_hash) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = baselines_.find(file_path);
  if (it == baselines_.end()) return true;
  return it->second == current_hash;
}

HashMode HashIntegrity::currentMode() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_.mode;
}

bool HashIntegrity::isDisabled() const {
  return currentMode() == HashMode::Disabled;
}

bool HashIntegrity::isForensic() const {
  return currentMode() == HashMode::Forensic;
}

std::size_t HashIntegrity::eventsEmitted() const {
  return events_emitted_.load();
}

std::size_t HashIntegrity::baselineCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return baselines_.size();
}

void HashIntegrity::clearBaselines() {
  std::lock_guard<std::mutex> lock(mu_);
  baselines_.clear();
}

void HashIntegrity::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  baselines_.clear();
}

void HashIntegrity::emit(const HashEvent& event) {
  events_emitted_++;
  if (callback_) {
    callback_(event);
  }
}

}  // namespace monix::collectors::forensic
