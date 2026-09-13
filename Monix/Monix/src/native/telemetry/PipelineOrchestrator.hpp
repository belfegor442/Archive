#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Snapshot.hpp"
#include "normalization/Normalizer.hpp"
#include "normalization/Validator.hpp"
#include "correlation/CorrelationEngine.hpp"
#include "state/StateStore.hpp"
#include "export/TelemetryRelay.hpp"
#include "export/ScramHistoryRing.hpp"
#include "export/HttpBridge.hpp"
#include "../scram/ScramEngine.hpp"
#include "../core/TextUtils.hpp"

namespace monix::telemetry {

struct PipelineResult {
  bool normalized = false;
  bool validated = false;
  int normalizeEvents = 0;
  int validationIssues = 0;
  bool hasCorrelation = false;
  int correlationEvents = 0;
};

class PipelineOrchestrator {
 public:
  PipelineOrchestrator() {
    correlationEngine_.AddDefaultRules();
  }

  void InitializeRelay(WsConfig wsConfig, int historyCapacity = 128) {
    relay_ = std::make_unique<TelemetryRelay>(wsConfig);
    history_ = std::make_unique<ScramHistoryRing>(historyCapacity);
    httpBridge_ = std::make_unique<HttpBridge>(HttpBridge::Config{wsConfig.port + 1});
  }

  bool StartRelay() {
    bool ok = relay_ && relay_->Start();
    if (httpBridge_) {
      httpBridge_->SetStatusProvider([this]() -> std::string {
        if (!relay_) return R"({"status":"no_relay"})";
        std::string json = R"({"status":"running","wsPort":)" + std::to_string(relay_->Port())
          + R"(,"wsClients":)" + std::to_string(relay_->ClientCount())
          + R"(,"historyEvents":)" + std::to_string(history_ ? history_->Count() : 0)
          + R"(,"httpPort":)" + std::to_string(httpBridge_->Port()) + "}";
        return json;
      });
      httpBridge_->SetHistoryProvider([this]() -> std::string {
        return history_ ? history_->SerializeRecent(50) : R"({"events":[]})";
      });
      httpBridge_->Start();
    }
    return ok;
  }

  void StopRelay() {
    if (relay_) relay_->Stop();
    if (httpBridge_) httpBridge_->Stop();
  }

  PipelineResult ProcessSnapshot(Snapshot& snap, const Snapshot* prev,
                                  ScramEngine& scramEngine,
                                  ScramResult& outScram,
                                  std::uint64_t timestampMs,
                                  int sampleIndex) {
    PipelineResult result;

    normalizer_.Normalize(snap);
    result.normalized = true;
    result.normalizeEvents = static_cast<int>(normalizer_.Events().size());
    normalizer_.ClearEvents();

    validator_.Validate(snap, prev);
    result.validated = true;
    result.validationIssues = static_cast<int>(validator_.Issues().size());

    correlationEngine_.Observe(snap, prev, timestampMs * 1000000ULL);
    const auto& corrEvents = correlationEngine_.Events();
    result.hasCorrelation = !corrEvents.empty();
    result.correlationEvents = static_cast<int>(corrEvents.size());
    correlationEngine_.ClearEvents();

    stateStore_.Update(snap);

    if (relay_ && relay_->IsRunning()) {
      relay_->BroadcastSnapshot(snap, outScram, sampleIndex, timestampMs);
    }

    return result;
  }

  void RecordScramEvent(const ScramResult& scram, std::uint64_t timestampMs) {
    if (history_) history_->Record(scram, timestampMs);
    if (relay_ && relay_->IsRunning()) {
      relay_->BroadcastScramUpdate(scram, timestampMs);
    }
  }

  void BroadcastStatus(const std::string& status) {
    if (relay_) relay_->BroadcastStatus(status);
  }

  const Normalizer& GetNormalizer() const { return normalizer_; }
  const Validator& GetValidator() const { return validator_; }
  const CorrelationEngine& GetCorrelationEngine() const { return correlationEngine_; }
  const StateStore& GetStateStore() const { return stateStore_; }
  TelemetryRelay* GetRelay() const { return relay_.get(); }
  ScramHistoryRing* GetHistory() const { return history_.get(); }
  HttpBridge* GetHttpBridge() const { return httpBridge_.get(); }

  int ActiveCorrelationCount() const {
    int active = 0;
    for (const auto& r : correlationEngine_.Results())
      if (r.active) ++active;
    return active;
  }

 private:
  Normalizer normalizer_;
  Validator validator_;
  CorrelationEngine correlationEngine_;
  StateStore stateStore_;
  std::unique_ptr<TelemetryRelay> relay_;
  std::unique_ptr<ScramHistoryRing> history_;
  std::unique_ptr<HttpBridge> httpBridge_;
};

} // namespace monix::telemetry
