#include "EventSchemaRegistry.hpp"

namespace monix::validation {

bool EventSchemaRegistry::registerSchema(EventSchema schema) {
  std::lock_guard<std::mutex> lock(mu_);
  std::string key = schema.type.qualifiedName() + ":v" + std::to_string(schema.version);
  schemas_[std::move(key)] = std::move(schema);
  return true;
}

const EventSchema* EventSchemaRegistry::find(const events::EventType& type) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& [key, schema] : schemas_) {
    if (schema.type == type) return &schema;
  }
  return nullptr;
}

bool EventSchemaRegistry::contains(const events::EventType& type) const {
  return find(type) != nullptr;
}

std::size_t EventSchemaRegistry::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return schemas_.size();
}

void EventSchemaRegistry::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  schemas_.clear();
}

}  // namespace monix::validation
