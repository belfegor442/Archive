#pragma once

#include "ValidationPolicy.hpp"
#include "EventSchemaRegistry.hpp"
#include "SourceRegistry.hpp"
#include "../events/EventTime.hpp"

namespace monix::validation {

struct ValidationContext {
  ValidationProfile profile = ValidationProfile::Standard;
  ValidationPolicy policy;
  events::Timestamp now = 0;
  const EventSchemaRegistry* schemas = nullptr;
  const SourceRegistry* sources = nullptr;
};

}  // namespace monix::validation
