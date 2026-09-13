#include "CollectorRegistry.hpp"

namespace monix::collectors {

CollectorErrorCode CollectorRegistry::registerCollector(std::shared_ptr<ICollector> collector) {
  if (!collector) return CollectorErrorCode::InvalidConfig;

  std::lock_guard<std::mutex> lock(mu_);
  const auto& id = collector->id();
  if (collectors_.count(id) > 0) {
    return CollectorErrorCode::AlreadyRegistered;
  }
  collectors_[id] = std::move(collector);
  return CollectorErrorCode::None;
}

CollectorErrorCode CollectorRegistry::unregisterCollector(const CollectorId& id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = collectors_.find(id);
  if (it == collectors_.end()) {
    return CollectorErrorCode::NotRegistered;
  }
  collectors_.erase(it);
  return CollectorErrorCode::None;
}

std::shared_ptr<ICollector> CollectorRegistry::get(const CollectorId& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = collectors_.find(id);
  if (it != collectors_.end()) return it->second;
  return nullptr;
}

bool CollectorRegistry::contains(const CollectorId& id) const {
  std::lock_guard<std::mutex> lock(mu_);
  return collectors_.count(id) > 0;
}

std::vector<CollectorId> CollectorRegistry::list() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<CollectorId> ids;
  ids.reserve(collectors_.size());
  for (const auto& [id, c] : collectors_) {
    ids.push_back(id);
  }
  return ids;
}

std::size_t CollectorRegistry::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return collectors_.size();
}

void CollectorRegistry::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  collectors_.clear();
}

}  // namespace monix::collectors
