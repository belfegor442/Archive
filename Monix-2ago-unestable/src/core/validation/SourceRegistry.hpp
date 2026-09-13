#pragma once

#include "../events/SourceRef.hpp"

#include <unordered_set>
#include <mutex>
#include <string>

namespace monix::validation {

class SourceRegistry {
public:
  bool registerSource(const events::SourceId& id);
  bool contains(const events::SourceId& id) const;
  std::size_t size() const;

private:
  mutable std::mutex mu_;
  std::unordered_set<std::string> sources_;
};

}  // namespace monix::validation
