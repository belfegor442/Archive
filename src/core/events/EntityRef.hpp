#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace monix::events {

using EntityId = std::string;

enum class EntityKind : std::uint8_t {
  User,
  Process,
  File,
  Directory,
  Device,
  NetworkEndpoint,
  Socket,
  Service,
  Task,
  Host,
  Session,
  Unknown
};

const char* EntityKindName(EntityKind k);
EntityKind EntityKindFromString(const char* s);

struct EntityRef {
  EntityId id;
  EntityKind kind = EntityKind::Unknown;
  std::optional<std::string> name;

  bool isValid() const { return !id.empty(); }
};

}  // namespace monix::events
