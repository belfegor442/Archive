#pragma once

#include "EventId.hpp"
#include "EventTime.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <unordered_map>

namespace monix::events {

using PayloadValue = std::variant<
  std::string,
  std::int64_t,
  std::uint64_t,
  double,
  bool
>;

struct EventPayload {
  std::vector<std::pair<std::string, PayloadValue>> entries;

  bool empty() const { return entries.empty(); }
  std::size_t size() const { return entries.size(); }

  void set(const std::string& key, PayloadValue value);
  const PayloadValue* get(const std::string& key) const;

  bool has(const std::string& key) const { return get(key) != nullptr; }

  std::string getString(const std::string& key, const std::string& def = "") const;
  std::int64_t getInt(const std::string& key, std::int64_t def = 0) const;
  std::uint64_t getUInt(const std::string& key, std::uint64_t def = 0) const;
  double getDouble(const std::string& key, double def = 0.0) const;
  bool getBool(const std::string& key, bool def = false) const;

  void clear();
};

struct EventMetadata {
  std::vector<std::pair<std::string, PayloadValue>> entries;

  static constexpr std::size_t kMaxEntries = 64;
  static constexpr std::size_t kMaxValueSize = 4096;

  bool empty() const { return entries.empty(); }
  std::size_t size() const { return entries.size(); }

  bool set(const std::string& key, PayloadValue value);
  const PayloadValue* get(const std::string& key) const;

  bool has(const std::string& key) const { return get(key) != nullptr; }
  void clear();
};

}  // namespace monix::events
