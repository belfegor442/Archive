#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monix {

struct ProcessInfo {
  std::wstring name;
  int pid = 0;
  int parentPid = 0;
  int sessionId = 0;
  double cpuPct = 0.0;
  std::uint64_t ramBytes = 0;
  double gpuPct = 0.0;
  std::uint64_t createTime100ns = 0;
  std::wstring status;
  std::wstring priority;
  std::wstring processGuid;
};

struct ProcessTable {
  std::vector<ProcessInfo> list;
  int count = 0;
  int threadCount = 0;
  int handleCount = 0;
};

}
