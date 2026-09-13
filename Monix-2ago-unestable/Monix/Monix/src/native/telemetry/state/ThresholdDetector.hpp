#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "IChangeDetector.hpp"

namespace monix::telemetry {

struct ThresholdViolation {
  const char* field = nullptr;
  double value = 0.0;
  double threshold = 0.0;
  int severity = 0;
  std::string message;
};

using ThresholdCallback = std::function<void(const ThresholdViolation&)>;

class ThresholdDetector : public IChangeDetector {
public:
  explicit ThresholdDetector(const char* name) : name_(name) {}

  void AddField(
    const char* field,
    double warnThreshold,
    double errorThreshold,
    double criticalThreshold = 0.0)
  {
    fields_.push_back({field, warnThreshold, errorThreshold, criticalThreshold});
  }

  const char* Name() const override { return name_.c_str(); }

  void OnViolation(ThresholdCallback cb) { callback_ = std::move(cb); }

  void Detect(
    const Snapshot& current,
    const Snapshot* previous,
    std::uint64_t timestampNs,
    ChangeSet& out) override
  {
    for (const auto& f : fields_) {
      double val = GetField(current, f.field);
      if (val < 0) continue;

      int sev = 0;
      if (f.criticalThreshold > 0 && val >= f.criticalThreshold) sev = 3;
      else if (f.errorThreshold > 0 && val >= f.errorThreshold) sev = 2;
      else if (f.warnThreshold > 0 && val >= f.warnThreshold) sev = 1;

      if (sev > 0) {
        ThresholdViolation v;
        v.field = f.field;
        v.value = val;
        v.threshold = sev == 3 ? f.criticalThreshold : sev == 2 ? f.errorThreshold : f.warnThreshold;
        v.severity = sev;

        EntityIdentity id;
        id.kind = EntityIdentity::Kind::Memory;

        out.AddValueChanged(id, f.field, timestampNs);

        if (callback_) callback_(v);
      }
    }
  }

private:
  struct FieldConfig {
    const char* field;
    double warnThreshold;
    double errorThreshold;
    double criticalThreshold;
  };

  static double GetField(const Snapshot& s, const char* name);

  std::string name_;
  std::vector<FieldConfig> fields_;
  ThresholdCallback callback_;
};

}