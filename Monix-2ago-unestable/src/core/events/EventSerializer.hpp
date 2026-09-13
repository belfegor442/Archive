#pragma once

#include "Event.hpp"
#include "EventErrors.hpp"

#include <string>
#include <variant>

namespace monix::events {

class EventSerializer {
public:
  static std::string serialize(const Event& event);
  static EventErrorDetail serializeTo(const Event& event, std::string& output);

  static std::string escapeJsonString(const std::string& s);
  static std::string timestampToJson(Timestamp ts);
};

}  // namespace monix::events
