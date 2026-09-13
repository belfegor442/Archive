#include "Provenance.hpp"

#include <cstring>

namespace monix::events {

const char* ProvenanceKindName(ProvenanceKind k) {
  switch (k) {
    case ProvenanceKind::NativeObservation:     return "native_observation";
    case ProvenanceKind::CollectorObservation:  return "collector_observation";
    case ProvenanceKind::Derived:               return "derived";
    case ProvenanceKind::Inferred:              return "inferred";
    case ProvenanceKind::Predicted:             return "predicted";
  }
  return "native_observation";
}

ProvenanceKind ProvenanceKindFromString(const char* s) {
  if (std::strcmp(s, "native_observation") == 0)     return ProvenanceKind::NativeObservation;
  if (std::strcmp(s, "collector_observation") == 0)  return ProvenanceKind::CollectorObservation;
  if (std::strcmp(s, "derived") == 0)               return ProvenanceKind::Derived;
  if (std::strcmp(s, "inferred") == 0)              return ProvenanceKind::Inferred;
  if (std::strcmp(s, "predicted") == 0)             return ProvenanceKind::Predicted;
  return ProvenanceKind::NativeObservation;
}

}  // namespace monix::events
