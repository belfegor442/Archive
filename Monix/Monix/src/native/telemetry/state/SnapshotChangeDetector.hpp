#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "IChangeDetector.hpp"

namespace monix::telemetry {

struct FieldDelta {
  const char* name = nullptr;
  double currentValue = 0.0;
  double previousValue = 0.0;
  double delta = 0.0;
  double deltaPercent = 0.0;
  bool significant = false;
};

using DeltaCallback = std::function<void(const FieldDelta&)>;

class SnapshotChangeDetector : public IChangeDetector {
public:
  explicit SnapshotChangeDetector(const char* name) : name_(name) {}

  void AddField(const char* name, double minDelta, double minDeltaPercent = 0.0) {
    fields_.push_back({name, minDelta, minDeltaPercent});
  }

  const char* Name() const override { return name_.c_str(); }

  void OnSignificantChange(DeltaCallback cb) { callback_ = std::move(cb); }

  void Detect(
    const Snapshot& current,
    const Snapshot* previous,
    std::uint64_t timestampNs,
    ChangeSet& out) override
  {
    if (!previous) return;

    for (const auto& f : fields_) {
      double cur = GetField(current, f.name);
      double prev = GetField(*previous, f.name);

      if (cur < 0 || prev < 0) continue;

      FieldDelta fd;
      fd.name = f.name;
      fd.currentValue = cur;
      fd.previousValue = prev;
      fd.delta = cur - prev;

      if (prev != 0.0) {
        fd.deltaPercent = (fd.delta / prev) * 100.0;
      }

      fd.significant = std::abs(fd.delta) >= f.minDelta ||
                       std::abs(fd.deltaPercent) >= f.minDeltaPercent;

      if (fd.significant) {
        EntityIdentity id;
        id.kind = EntityIdentity::Kind::Memory;

        out.AddValueChanged(id, f.name, timestampNs);

        if (callback_) callback_(fd);
      }
    }
  }

private:
  struct FieldConfig {
    const char* name;
    double minDelta;
    double minDeltaPercent;
  };

  static double GetField(const Snapshot& s, const char* name);

  std::string name_;
  std::vector<FieldConfig> fields_;
  DeltaCallback callback_;
};

}