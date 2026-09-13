#pragma once

#include <cstdint>
#include <string>
#include <optional>

namespace monix::events {

struct Action {
  std::string name;
  std::optional<std::uint32_t> numeric_id;

  bool isValid() const { return !name.empty(); }
};

}  // namespace monix::events
