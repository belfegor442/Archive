#pragma once

#include "../Snapshot.hpp"

class MonixApp;

namespace monix::snapshot {

void ConsumeSnapshot(MonixApp* app, Snapshot snapshot);
void SeedReferenceState(MonixApp* app);

}  // namespace monix::snapshot
