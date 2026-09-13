#pragma once

#include "EventSchema.hpp"

#include <unordered_map>
#include <mutex>

namespace monix::validation {

class EventSchemaRegistry {
public:
  bool registerSchema(EventSchema schema);
  const EventSchema* find(const events::EventType& type) const;
  bool contains(const events::EventType& type) const;
  std::size_t size() const;
  void clear();

private:
  mutable std::mutex mu_;
  std::unordered_map<std::string, EventSchema> schemas_;
};

}  // namespace monix::validation
