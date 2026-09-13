#pragma once

#include "Event.hpp"
#include "EventErrors.hpp"

#include <string>
#include <variant>

namespace monix::events {

class EventDeserializer {
public:
  static std::variant<Event, EventErrorDetail> deserialize(const std::string& json);
  static std::variant<Event, EventErrorDetail> deserialize(const char* data, std::size_t len);
};

}  // namespace monix::events
