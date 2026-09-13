#pragma once

#include "ICollector.hpp"
#include "CollectorErrors.hpp"

#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace monix::collectors {

class CollectorRegistry {
public:
  CollectorErrorCode registerCollector(std::shared_ptr<ICollector> collector);
  CollectorErrorCode unregisterCollector(const CollectorId& id);

  std::shared_ptr<ICollector> get(const CollectorId& id) const;
  bool contains(const CollectorId& id) const;
  std::vector<CollectorId> list() const;
  std::size_t size() const;

  void clear();

private:
  mutable std::mutex mu_;
  std::unordered_map<CollectorId, std::shared_ptr<ICollector>> collectors_;
};

}  // namespace monix::collectors
