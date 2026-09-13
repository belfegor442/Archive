#pragma once

#include "../Snapshot.hpp"
#include <vector>

class MonixApp;

namespace monix::snapshot {

Snapshot PollSnapshot(MonixApp* app);
Snapshot PollNativeSnapshot(MonixApp* app);
std::vector<ProcessInfo> BuildFallbackProcesses(MonixApp* app, const Snapshot& snapshot);

}  // namespace monix::snapshot
