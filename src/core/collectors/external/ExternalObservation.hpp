#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::external {

enum class AdapterStatus : std::uint8_t {
  Disconnected,
  Connecting,
  Connected,
  Error,
  Shutdown
};

const char* AdapterStatusName(AdapterStatus s);

enum class AdapterKind : std::uint8_t {
  Camera,
  NVR,
  Router,
  NAS,
  UPS,
  Sensor,
  Server,
  IoT,
  Custom
};

const char* AdapterKindName(AdapterKind k);

struct AdapterConfig {
  std::string id;
  std::string name;
  AdapterKind kind = AdapterKind::Custom;
  std::string endpoint;
  std::int64_t poll_interval_ms = 5000;
  std::int64_t timeout_ms = 10000;
  bool auto_reconnect = true;
  std::size_t max_reconnect_attempts = 5;

  bool isValid() const;
};

struct NormalizedEvent {
  std::string source_id;
  std::string source_type;
  std::string event_type;
  std::string description;
  std::int64_t timestamp_ms = 0;
  double numeric_value = 0.0;
  std::string string_value;
  std::unordered_map<std::string, std::string> attributes;

  bool isValid() const;
  std::string summary() const;
};

struct AdapterMetrics {
  std::size_t events_received = 0;
  std::size_t events_normalized = 0;
  std::size_t events_dropped = 0;
  std::size_t errors = 0;
  std::int64_t last_event_ms = 0;
  std::int64_t connected_at_ms = 0;
  std::size_t reconnect_count = 0;

  std::string summary() const;
};

using EventCallback = std::function<void(const NormalizedEvent&)>;
using StatusCallback = std::function<void(const std::string& adapter_id, AdapterStatus status)>;

class IExternalObservationAdapter {
public:
  virtual ~IExternalObservationAdapter() = default;

  virtual const char* name() const = 0;
  virtual const char* version() const = 0;
  virtual AdapterKind kind() const = 0;

  virtual bool connect(const AdapterConfig& config) = 0;
  virtual void disconnect() = 0;
  virtual AdapterStatus status() const = 0;

  virtual std::vector<NormalizedEvent> receive() = 0;
  virtual NormalizedEvent normalize(const std::string& raw_data) = 0;

  virtual void shutdown() = 0;

  virtual AdapterMetrics metrics() const = 0;
  virtual bool isConnected() const = 0;
};

class ExternalObservationManager {
public:
  ExternalObservationManager();
  ~ExternalObservationManager();

  ExternalObservationManager(const ExternalObservationManager&) = delete;
  ExternalObservationManager& operator=(const ExternalObservationManager&) = delete;

  void setEventCallback(EventCallback cb);
  void setStatusCallback(StatusCallback cb);

  bool registerAdapter(const std::string& id, std::unique_ptr<IExternalObservationAdapter> adapter);
  bool unregisterAdapter(const std::string& id);

  bool connect(const std::string& adapter_id, const AdapterConfig& config);
  bool disconnect(const std::string& adapter_id);

  std::vector<NormalizedEvent> poll(const std::string& adapter_id);
  std::vector<NormalizedEvent> pollAll();

  AdapterStatus status(const std::string& adapter_id) const;
  bool isConnected(const std::string& adapter_id) const;
  AdapterMetrics metrics(const std::string& adapter_id) const;

  std::size_t adapterCount() const;
  std::size_t connectedCount() const;
  std::vector<std::string> adapterIds() const;

  void shutdownAll();
  void clear();

private:
  EventCallback event_callback_;
  StatusCallback status_callback_;
  mutable std::mutex mu_;

  struct AdapterEntry {
    std::unique_ptr<IExternalObservationAdapter> adapter;
    AdapterConfig config;
  };

  std::unordered_map<std::string, AdapterEntry> adapters_;
};

}  // namespace monix::collectors::external
