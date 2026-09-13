#pragma once

#include "SourceRef.hpp"

#include <cstdint>
#include <string>

namespace monix::events {

enum class ProvenanceKind : std::uint8_t {
  NativeObservation,
  CollectorObservation,
  Derived,
  Inferred,
  Predicted
};

const char* ProvenanceKindName(ProvenanceKind k);
ProvenanceKind ProvenanceKindFromString(const char* s);

struct Provenance {
  SourceId source_id;
  std::string collector_name;
  std::string collector_version;
  std::string schema_name = "monix.event";
  std::uint32_t schema_version = 1;
  ProvenanceKind kind = ProvenanceKind::NativeObservation;

  bool isValid() const { return !source_id.empty(); }
};

}  // namespace monix::events
