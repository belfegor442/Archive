#include "SourceRef.hpp"

#include <cstring>

namespace monix::events {

const char* SourceKindName(SourceKind k) {
  switch (k) {
    case SourceKind::OperatingSystem:  return "operating_system";
    case SourceKind::MonixCollector:   return "monix_collector";
    case SourceKind::MonixComponent:   return "monix_component";
    case SourceKind::ExternalDevice:   return "external_device";
    case SourceKind::RemoteHost:       return "remote_host";
    case SourceKind::Application:      return "application";
    case SourceKind::Synthetic:        return "synthetic";
    case SourceKind::Unknown:          return "unknown";
  }
  return "unknown";
}

SourceKind SourceKindFromString(const char* s) {
  if (std::strcmp(s, "operating_system") == 0) return SourceKind::OperatingSystem;
  if (std::strcmp(s, "monix_collector") == 0)  return SourceKind::MonixCollector;
  if (std::strcmp(s, "monix_component") == 0)  return SourceKind::MonixComponent;
  if (std::strcmp(s, "external_device") == 0)  return SourceKind::ExternalDevice;
  if (std::strcmp(s, "remote_host") == 0)      return SourceKind::RemoteHost;
  if (std::strcmp(s, "application") == 0)      return SourceKind::Application;
  if (std::strcmp(s, "synthetic") == 0)        return SourceKind::Synthetic;
  return SourceKind::Unknown;
}

}  // namespace monix::events
