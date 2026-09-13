#include "ExternalObservation.hpp"

#include <sstream>

namespace monix::collectors::external {

const char* AdapterStatusName(AdapterStatus s) {
  switch (s) {
    case AdapterStatus::Disconnected: return "Disconnected";
    case AdapterStatus::Connecting:   return "Connecting";
    case AdapterStatus::Connected:    return "Connected";
    case AdapterStatus::Error:        return "Error";
    case AdapterStatus::Shutdown:     return "Shutdown";
  }
  return "Unknown";
}

const char* AdapterKindName(AdapterKind k) {
  switch (k) {
    case AdapterKind::Camera:  return "Camera";
    case AdapterKind::NVR:     return "NVR";
    case AdapterKind::Router:  return "Router";
    case AdapterKind::NAS:     return "NAS";
    case AdapterKind::UPS:     return "UPS";
    case AdapterKind::Sensor:  return "Sensor";
    case AdapterKind::Server:  return "Server";
    case AdapterKind::IoT:     return "IoT";
    case AdapterKind::Custom:  return "Custom";
  }
  return "Unknown";
}

bool AdapterConfig::isValid() const {
  return !id.empty() && !name.empty();
}

bool NormalizedEvent::isValid() const {
  return !source_id.empty() && !event_type.empty();
}

std::string NormalizedEvent::summary() const {
  std::string result = source_id + " [" + event_type + "]";
  if (!description.empty()) result += " " + description;
  return result;
}

std::string AdapterMetrics::summary() const {
  std::ostringstream oss;
  oss << "recv=" << events_received << " norm=" << events_normalized
      << " drop=" << events_dropped << " err=" << errors
      << " reconn=" << reconnect_count;
  return oss.str();
}

ExternalObservationManager::ExternalObservationManager() {}
ExternalObservationManager::~ExternalObservationManager() {}

void ExternalObservationManager::setEventCallback(EventCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  event_callback_ = std::move(cb);
}

void ExternalObservationManager::setStatusCallback(StatusCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  status_callback_ = std::move(cb);
}

bool ExternalObservationManager::registerAdapter(const std::string& id, std::unique_ptr<IExternalObservationAdapter> adapter) {
  if (!adapter) return false;
  std::lock_guard<std::mutex> lock(mu_);
  if (adapters_.count(id) > 0) return false;
  AdapterEntry entry;
  entry.adapter = std::move(adapter);
  adapters_[id] = std::move(entry);
  return true;
}

bool ExternalObservationManager::unregisterAdapter(const std::string& id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = adapters_.find(id);
  if (it == adapters_.end()) return false;
  if (it->second.adapter->isConnected()) {
    it->second.adapter->disconnect();
  }
  adapters_.erase(it);
  return true;
}

bool ExternalObservationManager::connect(const std::string& adapter_id, const AdapterConfig& config) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = adapters_.find(adapter_id);
  if (it == adapters_.end()) return false;

  it->second.config = config;
  bool ok = it->second.adapter->connect(config);

  if (status_callback_) {
    status_callback_(adapter_id, ok ? AdapterStatus::Connected : AdapterStatus::Error);
  }

  return ok;
}

bool ExternalObservationManager::disconnect(const std::string& adapter_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = adapters_.find(adapter_id);
  if (it == adapters_.end()) return false;

  it->second.adapter->disconnect();

  if (status_callback_) {
    status_callback_(adapter_id, AdapterStatus::Disconnected);
  }

  return true;
}

std::vector<NormalizedEvent> ExternalObservationManager::poll(const std::string& adapter_id) {
  std::vector<NormalizedEvent> events;
  IExternalObservationAdapter* adapter = nullptr;

  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = adapters_.find(adapter_id);
    if (it == adapters_.end()) return events;
    if (!it->second.adapter->isConnected()) return events;
    adapter = it->second.adapter.get();
  }

  events = adapter->receive();

  if (event_callback_) {
    for (const auto& e : events) {
      event_callback_(e);
    }
  }

  return events;
}

std::vector<NormalizedEvent> ExternalObservationManager::pollAll() {
  std::vector<NormalizedEvent> all_events;
  std::vector<std::string> ids;

  {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& [id, entry] : adapters_) {
      if (entry.adapter->isConnected()) {
        ids.push_back(id);
      }
    }
  }

  for (const auto& id : ids) {
    auto events = poll(id);
    all_events.insert(all_events.end(), events.begin(), events.end());
  }

  return all_events;
}

AdapterStatus ExternalObservationManager::status(const std::string& adapter_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = adapters_.find(adapter_id);
  if (it == adapters_.end()) return AdapterStatus::Disconnected;
  return it->second.adapter->status();
}

bool ExternalObservationManager::isConnected(const std::string& adapter_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = adapters_.find(adapter_id);
  if (it == adapters_.end()) return false;
  return it->second.adapter->isConnected();
}

AdapterMetrics ExternalObservationManager::metrics(const std::string& adapter_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = adapters_.find(adapter_id);
  if (it == adapters_.end()) return AdapterMetrics{};
  return it->second.adapter->metrics();
}

std::size_t ExternalObservationManager::adapterCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return adapters_.size();
}

std::size_t ExternalObservationManager::connectedCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t count = 0;
  for (const auto& [id, entry] : adapters_) {
    if (entry.adapter->isConnected()) count++;
  }
  return count;
}

std::vector<std::string> ExternalObservationManager::adapterIds() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> ids;
  for (const auto& [id, entry] : adapters_) {
    ids.push_back(id);
  }
  return ids;
}

void ExternalObservationManager::shutdownAll() {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& [id, entry] : adapters_) {
    entry.adapter->shutdown();
  }
}

void ExternalObservationManager::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& [id, entry] : adapters_) {
    if (entry.adapter->isConnected()) {
      entry.adapter->disconnect();
    }
  }
  adapters_.clear();
}

}  // namespace monix::collectors::external
