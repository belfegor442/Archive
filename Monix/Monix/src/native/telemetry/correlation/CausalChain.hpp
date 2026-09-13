#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monix::telemetry {

enum class CausalRelation {
  CauseEffect,      // A causes B
  Correlated,       // A and B tend to occur together
  Sequential,       // A tends to follow B
  AntiCorrelated,   // A tends to occur when B does not
  Independent       // No observable relationship
};

struct CausalLink {
  const char* fromField = nullptr;
  const char* toField = nullptr;
  CausalRelation relation = CausalRelation::Independent;
  double confidence = 0.0;
  int sampleCount = 0;
  double avgLagMs = 0.0;
};

struct CausalEvent {
  const char* trigger = nullptr;
  const char* effect = nullptr;
  CausalRelation relation = CausalRelation::CauseEffect;
  double confidence = 0.0;
  std::uint64_t timestampNs = 0;
  double lagMs = 0.0;
  std::string description;
};

class CausalChain {
public:
  CausalChain() = default;

  void AddLink(CausalLink link) {
    links_.push_back(std::move(link));
  }

  void Observe(
    const char* field,
    double value,
    bool changed,
    std::uint64_t timestampNs)
  {
    Observation obs;
    obs.field = field;
    obs.value = value;
    obs.changed = changed;
    obs.timestampNs = timestampNs;
    observations_.push_back(obs);

    if (observations_.size() > kMaxObservations) {
      observations_.erase(observations_.begin());
    }

    DetectCausality(field, changed, timestampNs);
  }

  const std::vector<CausalEvent>& Events() const { return events_; }
  const std::vector<CausalLink>& Links() const { return links_; }
  void ClearEvents() { events_.clear(); }

  std::vector<CausalLink> FindLinks(const char* field) const {
    std::vector<CausalLink> result;
    for (const auto& l : links_) {
      if (l.fromField == field || l.toField == field) {
        result.push_back(l);
      }
    }
    return result;
  }

  CausalLink* FindLink(const char* from, const char* to) {
    for (auto& l : links_) {
      if (l.fromField == from && l.toField == to) {
        return &l;
      }
    }
    return nullptr;
  }

  void UpdateConfidence(const char* from, const char* to, bool confirmed) {
    CausalLink* link = FindLink(from, to);
    if (link) {
      link->sampleCount++;
      if (confirmed) {
        link->confidence = link->confidence * 0.9 + 0.1;
      } else {
        link->confidence = link->confidence * 0.95;
      }
    }
  }

private:
  struct Observation {
    const char* field = nullptr;
    double value = 0.0;
    bool changed = false;
    std::uint64_t timestampNs = 0;
  };

  void DetectCausality(const char* field, bool changed, std::uint64_t timestampNs) {
    if (!changed) return;

    for (const auto& link : links_) {
      if (link.toField == field) {
        for (auto it = observations_.rbegin(); it != observations_.rend(); ++it) {
          if (it->field == link.fromField && it->changed) {
            const double lagMs = static_cast<double>(timestampNs - it->timestampNs) / 1000000.0;
            if (lagMs >= 0.0 && lagMs <= 5000.0) {
              CausalEvent evt;
              evt.trigger = link.fromField;
              evt.effect = field;
              evt.relation = link.relation;
              evt.confidence = link.confidence;
              evt.timestampNs = timestampNs;
              evt.lagMs = lagMs;
              events_.push_back(std::move(evt));
            }
            break;
          }
        }
      }
    }
  }

  static constexpr std::size_t kMaxObservations = 1024;

  std::vector<CausalLink> links_;
  std::vector<Observation> observations_;
  std::vector<CausalEvent> events_;
};

}