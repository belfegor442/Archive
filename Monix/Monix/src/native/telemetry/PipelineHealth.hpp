#pragma once

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace monix::telemetry {

struct HealthCheckpoint {
  bool normalizerOk = true;
  bool validatorOk = true;
  bool correlationOk = true;
  bool stateStoreOk = true;
  bool relayOk = false;
  bool httpBridgeOk = false;
  int totalSnapshots = 0;
  int totalErrors = 0;
  int consecutiveErrors = 0;
  int relayMessagesSent = 0;
  int relayClientsServed = 0;
  std::uint64_t lastSnapshotMs = 0;
  std::uint64_t lastErrorMs = 0;
  std::uint64_t startedAtMs = 0;
};

class PipelineHealth {
 public:
  PipelineHealth() {
    startedAtMs_ = NowMs();
    checkpoint_.startedAtMs = startedAtMs_;
  }

  void RecordSnapshot() {
    std::lock_guard lock(mutex_);
    checkpoint_.totalSnapshots++;
    checkpoint_.lastSnapshotMs = NowMs();
    checkpoint_.consecutiveErrors = 0;
  }

  void RecordError(const char* phase, const char* detail) {
    std::lock_guard lock(mutex_);
    checkpoint_.totalErrors++;
    checkpoint_.consecutiveErrors++;
    checkpoint_.lastErrorMs = NowMs();
    lastErrorPhase_ = phase;
    lastErrorDetail_ = detail;
  }

  void SetRelayStatus(bool running) {
    std::lock_guard lock(mutex_);
    checkpoint_.relayOk = running;
  }

  void SetHttpBridgeStatus(bool running) {
    std::lock_guard lock(mutex_);
    checkpoint_.httpBridgeOk = running;
  }

  void RecordRelaySent(int count) {
    std::lock_guard lock(mutex_);
    checkpoint_.relayMessagesSent += count;
  }

  void RecordRelayClientsServed(int count) {
    std::lock_guard lock(mutex_);
    checkpoint_.relayClientsServed += count;
  }

  HealthCheckpoint GetCheckpoint() const {
    std::lock_guard lock(mutex_);
    return checkpoint_;
  }

  bool IsHealthy() const {
    std::lock_guard lock(mutex_);
    return checkpoint_.consecutiveErrors < 5;
  }

  std::string SerializeJson() const {
    std::lock_guard lock(mutex_);
    std::string json = R"({)"
      R"("healthy":)" + std::string(IsHealthy() ? "true" : "false") +
      R"(,"snapshots":)" + std::to_string(checkpoint_.totalSnapshots) +
      R"(,"errors":)" + std::to_string(checkpoint_.totalErrors) +
      R"(,"consecutiveErrors":)" + std::to_string(checkpoint_.consecutiveErrors) +
      R"(,"uptimeMs":)" + std::to_string(NowMs() - startedAtMs_) +
      R"(,"relayOk":)" + std::string(checkpoint_.relayOk ? "true" : "false") +
      R"(,"httpBridgeOk":)" + std::string(checkpoint_.httpBridgeOk ? "true" : "false") +
      R"(,"relaySent":)" + std::to_string(checkpoint_.relayMessagesSent) +
      R"(,"relayClients":)" + std::to_string(checkpoint_.relayClientsServed);
    if (!lastErrorPhase_.empty()) {
      json += R"(,"lastError":{"phase":")" + lastErrorPhase_
        + R"(","detail":")" + lastErrorDetail_
        + R"(","atMs":)" + std::to_string(checkpoint_.lastErrorMs) + "}";
    }
    json += "}";
    return json;
  }

 private:
  static std::uint64_t NowMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    auto t = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return t / 10000 - 11644473600000ULL;
  }

  mutable std::mutex mutex_;
  HealthCheckpoint checkpoint_;
  std::uint64_t startedAtMs_ = 0;
  std::string lastErrorPhase_;
  std::string lastErrorDetail_;
};

} // namespace monix::telemetry
