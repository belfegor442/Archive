#pragma once

#include "CollectorRegistry.hpp"
#include "CollectorMetrics.hpp"
#include "CollectorConfig.hpp"

#include <memory>
#include <vector>
#include <mutex>

namespace monix::collectors {

struct CollectorManagerStats {
  std::size_t total_registered = 0;
  std::size_t running = 0;
  std::size_t stopped = 0;
  std::size_t failed = 0;
  std::size_t degraded = 0;
};

class CollectorManager {
public:
  CollectorErrorCode registerCollector(std::shared_ptr<ICollector> collector);
  CollectorErrorCode unregisterCollector(const CollectorId& id);

  CollectorErrorCode startCollector(const CollectorId& id,
                                    const CollectorConfig& config = CollectorConfig{});
  CollectorErrorCode stopCollector(const CollectorId& id);

  CollectorErrorCode startAll(const CollectorConfig& defaultConfig = CollectorConfig{});
  CollectorErrorCode stopAll();

  std::shared_ptr<ICollector> getCollector(const CollectorId& id) const;
  CollectorStatus getCollectorStatus(const CollectorId& id) const;
  CollectorManagerStats stats() const;

  std::vector<CollectorId> list() const;
  std::size_t size() const;

  CollectorRegistry& registry();
  const CollectorRegistry& registry() const;

private:
  CollectorRegistry registry_;
  std::unordered_map<CollectorId, CollectorMetrics> metrics_;
  mutable std::mutex mu_;
};

}  // namespace monix::collectors
