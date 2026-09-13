#include "CollectorManager.hpp"

namespace monix::collectors {

CollectorErrorCode CollectorManager::registerCollector(std::shared_ptr<ICollector> collector) {
  if (!collector) return CollectorErrorCode::InvalidConfig;
  return registry_.registerCollector(std::move(collector));
}

CollectorErrorCode CollectorManager::unregisterCollector(const CollectorId& id) {
  {
    auto collector = registry_.get(id);
    if (!collector) return CollectorErrorCode::NotRegistered;
    if (collector->status().isOperational()) {
      auto stopResult = stopCollector(id);
      if (stopResult != CollectorErrorCode::None && stopResult != CollectorErrorCode::NotRunning) {
        return stopResult;
      }
    }
  }
  return registry_.unregisterCollector(id);
}

CollectorErrorCode CollectorManager::startCollector(const CollectorId& id,
                                                    const CollectorConfig& config) {
  auto collector = registry_.get(id);
  if (!collector) return CollectorErrorCode::NotRegistered;

  auto st = collector->status();
  if (st.lifecycle == CollectorLifecycle::Running) {
    return CollectorErrorCode::AlreadyRunning;
  }
  if (!st.canStart()) {
    return CollectorErrorCode::InvalidTransition;
  }

  bool success = collector->start(config);
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& m = metrics_[id];
    if (success) {
      m.recordStart();
    } else {
      m.recordError();
    }
  }
  return success ? CollectorErrorCode::None : CollectorErrorCode::StartFailed;
}

CollectorErrorCode CollectorManager::stopCollector(const CollectorId& id) {
  auto collector = registry_.get(id);
  if (!collector) return CollectorErrorCode::NotRegistered;

  auto st = collector->status();
  if (st.lifecycle == CollectorLifecycle::Stopped ||
      st.lifecycle == CollectorLifecycle::Created) {
    return CollectorErrorCode::NotRunning;
  }
  if (!st.canStop()) {
    return CollectorErrorCode::InvalidTransition;
  }

  bool success = collector->stop();
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& m = metrics_[id];
    if (success) {
      m.recordStop();
    } else {
      m.recordError();
    }
  }
  return success ? CollectorErrorCode::None : CollectorErrorCode::StopFailed;
}

CollectorErrorCode CollectorManager::startAll(const CollectorConfig& defaultConfig) {
  auto ids = registry_.list();
  CollectorErrorCode worst = CollectorErrorCode::None;
  for (const auto& id : ids) {
    auto result = startCollector(id, defaultConfig);
    if (result != CollectorErrorCode::None && result != CollectorErrorCode::AlreadyRunning) {
      if (worst == CollectorErrorCode::None) worst = result;
    }
  }
  return worst;
}

CollectorErrorCode CollectorManager::stopAll() {
  auto ids = registry_.list();
  CollectorErrorCode worst = CollectorErrorCode::None;
  for (const auto& id : ids) {
    auto result = stopCollector(id);
    if (result != CollectorErrorCode::None && result != CollectorErrorCode::NotRunning) {
      if (worst == CollectorErrorCode::None) worst = result;
    }
  }
  return worst;
}

std::shared_ptr<ICollector> CollectorManager::getCollector(const CollectorId& id) const {
  return registry_.get(id);
}

CollectorStatus CollectorManager::getCollectorStatus(const CollectorId& id) const {
  auto collector = registry_.get(id);
  if (collector) return collector->status();
  return CollectorStatus{};
}

CollectorManagerStats CollectorManager::stats() const {
  CollectorManagerStats s;
  auto ids = registry_.list();
  s.total_registered = ids.size();
  for (const auto& id : ids) {
    auto collector = registry_.get(id);
    if (!collector) continue;
    auto st = collector->status();
    switch (st.lifecycle) {
      case CollectorLifecycle::Running:  s.running++; break;
      case CollectorLifecycle::Stopped:  s.stopped++; break;
      case CollectorLifecycle::Failed:   s.failed++; break;
      case CollectorLifecycle::Degraded: s.degraded++; break;
      default: break;
    }
  }
  return s;
}

std::vector<CollectorId> CollectorManager::list() const {
  return registry_.list();
}

std::size_t CollectorManager::size() const {
  return registry_.size();
}

CollectorRegistry& CollectorManager::registry() {
  return registry_;
}

const CollectorRegistry& CollectorManager::registry() const {
  return registry_;
}

}  // namespace monix::collectors
