#pragma once

#include <cstdint>

namespace monix {

struct MemoryState {
  uint64_t usedBytes = 0;
  uint64_t totalBytes = 0;
  uint64_t availBytes = 0;
  uint64_t commitUsedBytes = 0;
  uint64_t commitLimitBytes = 0;
  uint64_t commitPeakBytes = 0;
  uint64_t kernelPoolPagedBytes = 0;
  uint64_t kernelPoolNonpagedBytes = 0;
  uint64_t systemCacheBytes = 0;
  uint64_t pageFaultsDelta = 0;
  uint64_t hardPageFaultsDelta = 0;
  uint64_t standbyListBytes = 0;
  uint64_t modifiedListBytes = 0;
  uint64_t freeListBytes = 0;
  uint64_t zeroListBytes = 0;
  uint64_t workingSetTotalBytes = 0;
  uint64_t totalWorkingSetBytes = 0;
  uint64_t pageFileUsedBytes = 0;
  uint64_t pageFileTotalBytes = 0;
};

}
