#pragma once

#include "EventId.hpp"

#include <cstdint>
#include <string>
#include <optional>

namespace monix::events {

using CorrelationId = std::string;
using ActivityId = std::string;
using RequestId = std::string;

struct Correlation {
  std::optional<CorrelationId> correlation_id;
  std::optional<EventId> parent_event_id;
  std::optional<ActivityId> activity_id;
  std::optional<std::string> session_id;
  std::optional<RequestId> request_id;

  bool hasAny() const {
    return correlation_id.has_value() || parent_event_id.has_value() ||
           activity_id.has_value() || session_id.has_value() || request_id.has_value();
  }
};

}  // namespace monix::events
