#pragma once

#include <cstdint>
#include <string>

namespace monix::events {

using SourceId = std::string;

enum class SourceKind : std::uint8_t {
  OperatingSystem,
  MonixCollector,
  MonixComponent,
  ExternalDevice,
  RemoteHost,
  Application,
  Synthetic,
  Unknown
};

const char* SourceKindName(SourceKind k);
SourceKind SourceKindFromString(const char* s);

struct SourceRef {
  SourceId id;
  std::string name;
  std::string version;
  SourceKind kind = SourceKind::Unknown;

  bool isValid() const { return !id.empty(); }
};

}  // namespace monix::events
