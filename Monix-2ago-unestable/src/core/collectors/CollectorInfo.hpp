#pragma once

#include <cstdint>
#include <string>

namespace monix::collectors {

using CollectorId = std::string;

struct CollectorInfo {
  CollectorId id;
  std::string name;
  std::string version;
  std::string description;
  std::string author;
};

}  // namespace monix::collectors
