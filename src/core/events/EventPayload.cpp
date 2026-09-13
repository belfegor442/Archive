#include "EventPayload.hpp"

#include <algorithm>

namespace monix::events {

void EventPayload::set(const std::string& key, PayloadValue value) {
  for (auto& [k, v] : entries) {
    if (k == key) {
      v = std::move(value);
      return;
    }
  }
  entries.emplace_back(key, std::move(value));
}

const PayloadValue* EventPayload::get(const std::string& key) const {
  for (const auto& [k, v] : entries) {
    if (k == key) return &v;
  }
  return nullptr;
}

std::string EventPayload::getString(const std::string& key, const std::string& def) const {
  const auto* v = get(key);
  if (!v) return def;
  if (auto* s = std::get_if<std::string>(v)) return *s;
  return def;
}

std::int64_t EventPayload::getInt(const std::string& key, std::int64_t def) const {
  const auto* v = get(key);
  if (!v) return def;
  if (auto* i = std::get_if<std::int64_t>(v)) return *i;
  if (auto* u = std::get_if<std::uint64_t>(v)) return static_cast<std::int64_t>(*u);
  return def;
}

std::uint64_t EventPayload::getUInt(const std::string& key, std::uint64_t def) const {
  const auto* v = get(key);
  if (!v) return def;
  if (auto* u = std::get_if<std::uint64_t>(v)) return *u;
  if (auto* i = std::get_if<std::int64_t>(v)) return static_cast<std::uint64_t>(*i);
  return def;
}

double EventPayload::getDouble(const std::string& key, double def) const {
  const auto* v = get(key);
  if (!v) return def;
  if (auto* d = std::get_if<double>(v)) return *d;
  return def;
}

bool EventPayload::getBool(const std::string& key, bool def) const {
  const auto* v = get(key);
  if (!v) return def;
  if (auto* b = std::get_if<bool>(v)) return *b;
  return def;
}

void EventPayload::clear() {
  entries.clear();
}

bool EventMetadata::set(const std::string& key, PayloadValue value) {
  for (auto& [k, v] : entries) {
    if (k == key) {
      v = std::move(value);
      return true;
    }
  }
  if (entries.size() >= kMaxEntries) return false;
  entries.emplace_back(key, std::move(value));
  return true;
}

const PayloadValue* EventMetadata::get(const std::string& key) const {
  for (const auto& [k, v] : entries) {
    if (k == key) return &v;
  }
  return nullptr;
}

void EventMetadata::clear() {
  entries.clear();
}

}  // namespace monix::events
