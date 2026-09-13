#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace monix::events {

using ActorId = std::string;
using ProcessId = std::uint32_t;
using SessionId = std::string;

enum class ActorKind : std::uint8_t {
  User,
  Process,
  System,
  Service,
  Script,
  Automation,
  Remote,
  Device,
  Unknown
};

const char* ActorKindName(ActorKind k);
ActorKind ActorKindFromString(const char* s);

struct ActorRef {
  ActorId id;
  ActorKind kind = ActorKind::Unknown;
  std::optional<std::string> name;
  std::optional<std::string> username;
  std::optional<ProcessId> process_id;
  std::optional<SessionId> session_id;

  bool isValid() const { return !id.empty(); }
};

}  // namespace monix::events
