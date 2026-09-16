#pragma once

#include "../../telemetry/Snapshot.hpp"

namespace monix {

struct ProcessTable {
  std::vector<ProcessInfo> list;
  int count = 0;
  int threadCount = 0;
  int handleCount = 0;
};

}
