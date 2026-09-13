#pragma once

class MonixApp;
namespace monix { struct Snapshot; }

namespace monix::history {

void AppendHistoryPoint(MonixApp* app, const Snapshot& snapshot);

}  // namespace monix::history
