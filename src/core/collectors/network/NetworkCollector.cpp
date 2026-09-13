#include "NetworkCollector.hpp"

#include <algorithm>
#include <chrono>

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ipifcons.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace monix::collectors::network {

NetworkCollectorConfig NetworkCollectorConfig::defaults() {
  NetworkCollectorConfig cfg;
  cfg.track_interfaces = true;
  cfg.track_addresses = true;
  cfg.track_routes = true;
  cfg.track_connections = true;
  cfg.log_content = false;
  cfg.dedup_window_ms = 3000;
  cfg.max_events = 10000;
  return cfg;
}

NetworkCollector::NetworkCollector() = default;

NetworkCollector::~NetworkCollector() {
  stop();
}

bool NetworkCollector::start(NetworkCollectorConfig config) {
  if (running_) return false;
  config_ = std::move(config);
  running_ = true;
  events_emitted_ = 0;
  tracked_interfaces_.clear();
  tracked_connections_.clear();
  dedup_keys_.clear();
  return true;
}

bool NetworkCollector::stop() {
  if (!running_) return false;
  running_ = false;
  tracked_interfaces_.clear();
  tracked_connections_.clear();
  dedup_keys_.clear();
  return true;
}

bool NetworkCollector::isRunning() const {
  return running_;
}

void NetworkCollector::setCallback(NetworkCallback callback) {
  callback_ = std::move(callback);
}

void NetworkCollector::reportInterfaceCreated(const NetworkInterface& iface, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_interfaces) return;
  if (isDuplicate(NetworkEventKind::InterfaceCreated, "if:" + iface.name)) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (tracked_interfaces_.size() < config_.max_events) {
      tracked_interfaces_[iface.id] = iface;
    }
  }

  emit(NetworkEventKind::InterfaceCreated, origin, iface.summary());
}

void NetworkCollector::reportInterfaceRemoved(const NetworkInterface& iface, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_interfaces) return;
  if (isDuplicate(NetworkEventKind::InterfaceRemoved, "if:" + iface.name)) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_interfaces_.erase(iface.id);
  }

  emit(NetworkEventKind::InterfaceRemoved, origin, iface.summary());
}

void NetworkCollector::reportInterfaceUp(const NetworkInterface& iface, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_interfaces) return;
  if (isDuplicate(NetworkEventKind::InterfaceUp, "if:" + iface.name)) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = tracked_interfaces_.find(iface.id);
    if (it != tracked_interfaces_.end()) {
      it->second.state = InterfaceState::Up;
    } else if (tracked_interfaces_.size() < config_.max_events) {
      tracked_interfaces_[iface.id] = iface;
    }
  }

  emit(NetworkEventKind::InterfaceUp, origin, iface.summary());
}

void NetworkCollector::reportInterfaceDown(const NetworkInterface& iface, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_interfaces) return;
  if (isDuplicate(NetworkEventKind::InterfaceDown, "if:" + iface.name)) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = tracked_interfaces_.find(iface.id);
    if (it != tracked_interfaces_.end()) {
      it->second.state = InterfaceState::Down;
    }
  }

  emit(NetworkEventKind::InterfaceDown, origin, iface.summary());
}

void NetworkCollector::reportAddressAdded(const NetworkAddress& addr, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_addresses) return;
  if (isDuplicate(NetworkEventKind::AddressAdded, "addr:" + addr.address)) return;

  emit(NetworkEventKind::AddressAdded, origin, addr.address);
}

void NetworkCollector::reportAddressRemoved(const NetworkAddress& addr, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_addresses) return;
  if (isDuplicate(NetworkEventKind::AddressRemoved, "addr:" + addr.address)) return;

  emit(NetworkEventKind::AddressRemoved, origin, addr.address);
}

void NetworkCollector::reportRouteChanged(const NetworkRoute& route, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_routes) return;
  if (isDuplicate(NetworkEventKind::RouteChanged, "route:" + route.destination + ":" + route.gateway)) return;

  emit(NetworkEventKind::RouteChanged, origin, route.summary());
}

void NetworkCollector::reportConnectionStarted(const NetworkConnection& conn, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_connections) return;
  std::string key = conn.local_address + ":" + std::to_string(conn.local_port) +
                    "->" + conn.remote_address + ":" + std::to_string(conn.remote_port);
  if (isDuplicate(NetworkEventKind::ConnectionStarted, "conn:" + key)) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_connections_.push_back(conn);
  }

  emit(NetworkEventKind::ConnectionStarted, origin, conn.summary());
}

void NetworkCollector::reportConnectionClosed(const NetworkConnection& conn, NetworkDetectionOrigin origin) {
  if (!running_ || !config_.track_connections) return;
  std::string key = conn.local_address + ":" + std::to_string(conn.local_port) +
                    "->" + conn.remote_address + ":" + std::to_string(conn.remote_port);
  if (isDuplicate(NetworkEventKind::ConnectionClosed, "conn:" + key)) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = tracked_connections_.begin(); it != tracked_connections_.end(); ++it) {
      if (it->local_address == conn.local_address && it->local_port == conn.local_port &&
          it->remote_address == conn.remote_address && it->remote_port == conn.remote_port) {
        tracked_connections_.erase(it);
        break;
      }
    }
  }

  emit(NetworkEventKind::ConnectionClosed, origin, conn.summary());
}

std::size_t NetworkCollector::eventsEmitted() const {
  return events_emitted_;
}

std::size_t NetworkCollector::interfacesTracked() const {
  return tracked_interfaces_.size();
}

std::vector<NetworkInterface> NetworkCollector::currentInterfaces() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<NetworkInterface> result;
  for (const auto& [id, iface] : tracked_interfaces_) {
    result.push_back(iface);
  }
  return result;
}

std::vector<NetworkConnection> NetworkCollector::currentConnections() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tracked_connections_;
}

std::vector<NetworkInterface> NetworkCollector::enumerateInterfaces() const {
  std::vector<NetworkInterface> result;

  ULONG buf_len = 0;
  GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &buf_len);
  if (buf_len == 0) return result;

  std::vector<BYTE> buf(buf_len);
  PIP_ADAPTER_ADDRESSES adapter_addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
  if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapter_addrs, &buf_len) != ERROR_SUCCESS) {
    return result;
  }

  PIP_ADAPTER_ADDRESSES current = adapter_addrs;
  InterfaceId idx = 1;
  while (current) {
    NetworkInterface iface;
    iface.id = idx++;
    iface.name = current->AdapterName;

    if (current->Description) {
      int sz = WideCharToMultiByte(CP_UTF8, 0, current->Description, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) {
        iface.description.resize(static_cast<std::size_t>(sz) - 1);
        WideCharToMultiByte(CP_UTF8, 0, current->Description, -1, iface.description.data(), sz, nullptr, nullptr);
      }
    }

    if (current->PhysicalAddressLength >= 6) {
      char mac[32]{};
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
        current->PhysicalAddress[0], current->PhysicalAddress[1], current->PhysicalAddress[2],
        current->PhysicalAddress[3], current->PhysicalAddress[4], current->PhysicalAddress[5]);
      iface.mac_address = mac;
    }

    PIP_ADAPTER_UNICAST_ADDRESS ua = current->FirstUnicastAddress;
    while (ua) {
      if (ua->Address.lpSockaddr->sa_family == AF_INET) {
        char ip[INET_ADDRSTRLEN]{};
        auto* sa = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
        iface.ipv4_addresses.push_back(ip);
      } else if (ua->Address.lpSockaddr->sa_family == AF_INET6) {
        char ip[INET6_ADDRSTRLEN]{};
        auto* sa = reinterpret_cast<sockaddr_in6*>(ua->Address.lpSockaddr);
        inet_ntop(AF_INET6, &sa->sin6_addr, ip, sizeof(ip));
        iface.ipv6_addresses.push_back(ip);
      }
      ua = ua->Next;
    }

    iface.state = (current->OperStatus == IfOperStatusUp) ? InterfaceState::Up : InterfaceState::Down;
    iface.is_loopback = (current->IfType == 24);
    iface.mtu = current->Mtu;

    result.push_back(std::move(iface));
    current = current->Next;
  }

  return result;
}

bool NetworkCollector::isDuplicate(NetworkEventKind kind, const std::string& key) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  std::string dedup_key = std::to_string(static_cast<int>(kind)) + ":" + key +
                            "@" + std::to_string(now / std::max<std::uint32_t>(config_.dedup_window_ms, 1u));

  std::lock_guard<std::mutex> lock(mu_);
  if (dedup_keys_.count(dedup_key) > 0) {
    return true;
  }
  dedup_keys_.insert(dedup_key);
  return false;
}

void NetworkCollector::emit(NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
  events_emitted_++;
  if (callback_) {
    callback_(kind, origin, summary);
  }
}

}  // namespace monix::collectors::network
