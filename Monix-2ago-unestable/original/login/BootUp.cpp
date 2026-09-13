#include "BootUp.hpp"

namespace Monix {
namespace Boot {

static constexpr int MEM_TOTAL_KB = 3670016;
static constexpr int MEM_STEP_KB = 65536;
static constexpr ULONGLONG MEM_INTERVAL_MS = 12;

static constexpr ULONGLONG PHASE_DELAY_MS[] = {
  500,  // Header
  400,  // PostInfo
  0,    // MemoryCount (no delay, handled by MEM_INTERVAL_MS)
  300,  // MemoryDone
  500,  // Devices
  300   // Copyright
};

void BootUp::Start() {
  phase_ = Phase::Header;
  memoryKb_ = 0;
  memStep_ = 0;
  timer_ = GetTickCount64();
}

bool BootUp::Update() {
  if (phase_ == Phase::Done) return true;
  ULONGLONG now = GetTickCount64();

  switch (phase_) {
    case Phase::Header:
      if (now - timer_ >= PHASE_DELAY_MS[0]) {
        phase_ = Phase::PostInfo;
        timer_ = now;
      }
      break;

    case Phase::PostInfo:
      if (now - timer_ >= PHASE_DELAY_MS[1]) {
        phase_ = Phase::MemoryCount;
        memoryKb_ = 0;
        memStep_ = 0;
        timer_ = now;
      }
      break;

    case Phase::MemoryCount:
      if (now - timer_ >= MEM_INTERVAL_MS) {
        memoryKb_ += MEM_STEP_KB;
        memStep_++;
        if (memoryKb_ >= MEM_TOTAL_KB) {
          memoryKb_ = MEM_TOTAL_KB;
          phase_ = Phase::MemoryDone;
          timer_ = now;
        }
        timer_ = now;
      }
      break;

    case Phase::MemoryDone:
      if (now - timer_ >= PHASE_DELAY_MS[3]) {
        phase_ = Phase::Devices;
        timer_ = now;
      }
      break;

    case Phase::Devices:
      if (now - timer_ >= PHASE_DELAY_MS[4]) {
        phase_ = Phase::Copyright;
        timer_ = now;
      }
      break;

    case Phase::Copyright:
      if (now - timer_ >= PHASE_DELAY_MS[5]) {
        phase_ = Phase::Done;
      }
      break;

    default:
      phase_ = Phase::Done;
      break;
  }
  return phase_ == Phase::Done;
}

} // namespace Boot
} // namespace Monix
