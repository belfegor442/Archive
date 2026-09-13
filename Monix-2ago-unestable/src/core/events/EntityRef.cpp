#include "EntityRef.hpp"

#include <cstring>

namespace monix::events {

const char* EntityKindName(EntityKind k) {
  switch (k) {
    case EntityKind::User:            return "user";
    case EntityKind::Process:         return "process";
    case EntityKind::File:            return "file";
    case EntityKind::Directory:       return "directory";
    case EntityKind::Device:          return "device";
    case EntityKind::NetworkEndpoint: return "network_endpoint";
    case EntityKind::Socket:          return "socket";
    case EntityKind::Service:         return "service";
    case EntityKind::Task:            return "task";
    case EntityKind::Host:            return "host";
    case EntityKind::Session:         return "session";
    case EntityKind::Unknown:         return "unknown";
  }
  return "unknown";
}

EntityKind EntityKindFromString(const char* s) {
  if (std::strcmp(s, "user") == 0)            return EntityKind::User;
  if (std::strcmp(s, "process") == 0)         return EntityKind::Process;
  if (std::strcmp(s, "file") == 0)            return EntityKind::File;
  if (std::strcmp(s, "directory") == 0)       return EntityKind::Directory;
  if (std::strcmp(s, "device") == 0)          return EntityKind::Device;
  if (std::strcmp(s, "network_endpoint") == 0) return EntityKind::NetworkEndpoint;
  if (std::strcmp(s, "socket") == 0)          return EntityKind::Socket;
  if (std::strcmp(s, "service") == 0)         return EntityKind::Service;
  if (std::strcmp(s, "task") == 0)            return EntityKind::Task;
  if (std::strcmp(s, "host") == 0)            return EntityKind::Host;
  if (std::strcmp(s, "session") == 0)         return EntityKind::Session;
  return EntityKind::Unknown;
}

}  // namespace monix::events
