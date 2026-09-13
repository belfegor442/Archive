#pragma once

#include <cstdint>
#include <string>

namespace monix::events {

struct EventType {
  std::string namespace_name;
  std::string name;
  std::uint32_t numeric_id = 0;

  bool isValid() const { return !namespace_name.empty() && !name.empty(); }

  std::string qualifiedName() const {
    return namespace_name + "." + name;
  }

  bool operator==(const EventType& o) const noexcept {
    return namespace_name == o.namespace_name && name == o.name;
  }
  bool operator!=(const EventType& o) const noexcept {
    return !(*this == o);
  }
};

}  // namespace monix::events
