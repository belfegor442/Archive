#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Monix {
namespace Boot {

class BootUp {
public:
  BootUp() = default;

  void Start();
  bool Update();
  bool IsComplete() const { return phase_ == Phase::Done; }

  int GetPhase() const { return static_cast<int>(phase_); }
  int GetMemoryKb() const { return memoryKb_; }
  int GetMemStep() const { return memStep_; }
  bool IsMemCounting() const { return phase_ == Phase::MemoryCount; }

  enum class Phase {
    Header,
    PostInfo,
    MemoryCount,
    MemoryDone,
    Devices,
    Copyright,
    Done
  };

private:
  Phase phase_ = Phase::Done;
  int memoryKb_ = 0;
  int memStep_ = 0;
  ULONGLONG timer_ = 0;
};

} // namespace Boot
} // namespace Monix
