#pragma once

#include "NetworkTypes.hpp"

#include <functional>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

namespace monix::collectors::network {

using NetworkCallback = std::function<void(
  NetworkEventKind, NetworkDetectionOrigin, const std::string&)>;

struct NetworkCollectorConfig {
  bool track_interfaces = true;
  bool track_addresses = true;
  bool track_routes = true;
  bool track_connections = true;
  bool log_content = false;
  std::uint32_t dedup_window_ms = 3000;
  std::size_t max_events = 10000;

  static NetworkCollectorConfig defaults();
};

class NetworkCollector {
public:
  NetworkCollector();
  ~NetworkCollector();

  NetworkCollector(const NetworkCollector&) = delete;
  NetworkCollector& operator=(const NetworkCollector&) = delete;

  bool start(NetworkCollectorConfig config = NetworkCollectorConfig::defaults());
  bool stop();
  bool isRunning() const;

  void setCallback(NetworkCallback callback);

  void reportInterfaceCreated(const NetworkInterface& iface,
                              NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);
  void reportInterfaceRemoved(const NetworkInterface& iface,
                              NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);
  void reportInterfaceUp(const NetworkInterface& iface,
                         NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);
  void reportInterfaceDown(const NetworkInterface& iface,
                           NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);

  void reportAddressAdded(const NetworkAddress& addr,
                          NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);
  void reportAddressRemoved(const NetworkAddress& addr,
                            NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);

  void reportRouteChanged(const NetworkRoute& route,
                          NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);

  void reportConnectionStarted(const NetworkConnection& conn,
                               NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);
  void reportConnectionClosed(const NetworkConnection& conn,
                              NetworkDetectionOrigin origin = NetworkDetectionOrigin::Manual);

  std::size_t eventsEmitted() const;
  std::size_t interfacesTracked() const;
  std::vector<NetworkInterface> currentInterfaces() const;
  std::vector<NetworkConnection> currentConnections() const;

  std::vector<NetworkInterface> enumerateInterfaces() const;

private:
  bool isDuplicate(NetworkEventKind kind, const std::string& key);
  void emit(NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary);

  NetworkCollectorConfig config_;
  NetworkCallback callback_;
  std::unordered_map<InterfaceId, NetworkInterface> tracked_interfaces_;
  std::vector<NetworkConnection> tracked_connections_;
  std::unordered_set<std::string> dedup_keys_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::network
