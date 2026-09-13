#include "SourceRegistry.hpp"

namespace monix::validation {

bool SourceRegistry::registerSource(const events::SourceId& id) {
  std::lock_guard<std::mutex> lock(mu_);
  sources_.insert(id);
  return true;
}

bool SourceRegistry::contains(const events::SourceId& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  return sources_.count(id) > 0;
}

std::size_t SourceRegistry::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return sources_.size();
}

}  // namespace monix::validation
