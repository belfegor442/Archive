#pragma once

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "../../scram/ScramEngine.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

struct ScramEvent {
  uint64_t timestampMs = 0;
  int riskScore = 0;
  std::wstring headline;
  std::wstring insight;
  std::vector<std::wstring> diagnostics;
};

class ScramHistoryRing {
 public:
  explicit ScramHistoryRing(int capacity = 128) : capacity_(capacity) {
    events_.resize(capacity_);
  }

  void Record(const ScramResult& result, uint64_t timestampMs) {
    std::lock_guard lock(mutex_);
    ScramEvent ev;
    ev.timestampMs = timestampMs;
    ev.riskScore = result.riskScore;
    ev.headline = result.headline;
    ev.insight = result.insight;
    ev.diagnostics = result.diagnostics;
    events_[head_] = std::move(ev);
    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) ++count_;
  }

  std::vector<ScramEvent> GetRecent(int n) const {
    std::lock_guard lock(mutex_);
    int toReturn = std::min(n, count_);
    std::vector<ScramEvent> result;
    result.reserve(toReturn);
    int start = (head_ - toReturn + capacity_) % capacity_;
    for (int i = 0; i < toReturn; ++i)
      result.push_back(events_[(start + i) % capacity_]);
    return result;
  }

  ScramEvent GetLatest() const {
    std::lock_guard lock(mutex_);
    if (count_ == 0) return {};
    return events_[(head_ - 1 + capacity_) % capacity_];
  }

  std::vector<ScramEvent> GetByRiskThreshold(int minRisk) const {
    std::lock_guard lock(mutex_);
    std::vector<ScramEvent> result;
    for (int i = 0; i < count_; ++i) {
      int idx = (head_ - count_ + i + capacity_) % capacity_;
      if (events_[idx].riskScore >= minRisk)
        result.push_back(events_[idx]);
    }
    return result;
  }

  int Count() const {
    std::lock_guard lock(mutex_);
    return count_;
  }

  int Capacity() const { return capacity_; }

  void Clear() {
    std::lock_guard lock(mutex_);
    head_ = 0;
    count_ = 0;
  }

  std::string SerializeRecent(int n = 20) const {
    auto events = GetRecent(n);
    std::string json = R"({"type":"scram_history","count":)" + std::to_string(events.size()) + R"(,"events":[)";
    bool first = true;
    for (const auto& ev : events) {
      if (!first) json += ',';
      first = false;
      json += R"({"ts":)" + std::to_string(ev.timestampMs)
            + R"(,"risk":)" + std::to_string(ev.riskScore)
            + R"(,"headline":")" + EscapeJsonW(ev.headline) + R"(")"
            + R"(,"insight":")" + EscapeJsonW(ev.insight) + R"(")"
            + R"(,"diagnostics":[)";
      bool firstDiag = true;
      for (const auto& d : ev.diagnostics) {
        if (!firstDiag) json += ',';
        firstDiag = false;
        json += '"' + EscapeJsonW(d) + '"';
      }
      json += R"(]})";
    }
    json += R"(]})";
    return json;
  }

 private:
  static std::string EscapeJsonW(const std::wstring& ws) {
    std::string out;
    std::string utf8 = WideToUtf8(ws);
    for (char c : utf8) {
      switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default:   out += c; break;
      }
    }
    return out;
  }

  int capacity_;
  std::vector<ScramEvent> events_;
  int head_ = 0;
  int count_ = 0;
  mutable std::mutex mutex_;
};

} // namespace monix
