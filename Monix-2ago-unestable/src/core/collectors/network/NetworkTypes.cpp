#include "NetworkTypes.hpp"

#include <algorithm>
#include <cctype>

namespace monix::collectors::network {

const char* NetworkEventKindName(NetworkEventKind k) {
  switch (k) {
    case NetworkEventKind::InterfaceCreated:   return "InterfaceCreated";
    case NetworkEventKind::InterfaceRemoved:   return "InterfaceRemoved";
    case NetworkEventKind::InterfaceUp:        return "InterfaceUp";
    case NetworkEventKind::InterfaceDown:      return "InterfaceDown";
    case NetworkEventKind::AddressAdded:       return "AddressAdded";
    case NetworkEventKind::AddressRemoved:     return "AddressRemoved";
    case NetworkEventKind::RouteChanged:       return "RouteChanged";
    case NetworkEventKind::ConnectionStarted:  return "ConnectionStarted";
    case NetworkEventKind::ConnectionClosed:   return "ConnectionClosed";
  }
  return "Unknown";
}

std::string NetworkEventKindAction(NetworkEventKind k) {
  switch (k) {
    case NetworkEventKind::InterfaceCreated:   return "interface.created";
    case NetworkEventKind::InterfaceRemoved:   return "interface.removed";
    case NetworkEventKind::InterfaceUp:        return "interface.up";
    case NetworkEventKind::InterfaceDown:      return "interface.down";
    case NetworkEventKind::AddressAdded:       return "address.added";
    case NetworkEventKind::AddressRemoved:     return "address.removed";
    case NetworkEventKind::RouteChanged:       return "route.changed";
    case NetworkEventKind::ConnectionStarted:  return "connection.started";
    case NetworkEventKind::ConnectionClosed:   return "connection.closed";
  }
  return "unknown";
}

const char* InterfaceStateName(InterfaceState s) {
  switch (s) {
    case InterfaceState::Up:              return "Up";
    case InterfaceState::Down:            return "Down";
    case InterfaceState::Testing:         return "Testing";
    case InterfaceState::Unknown:         return "Unknown";
    case InterfaceState::Dormant:         return "Dormant";
    case InterfaceState::NotPresent:      return "NotPresent";
    case InterfaceState::LowerLayerDown:  return "LowerLayerDown";
  }
  return "Unknown";
}

const char* ProtocolName(Protocol p) {
  switch (p) {
    case Protocol::TCP:     return "TCP";
    case Protocol::UDP:     return "UDP";
    case Protocol::ICMP:    return "ICMP";
    case Protocol::ICMPv6:  return "ICMPv6";
    case Protocol::Unknown: return "Unknown";
  }
  return "Unknown";
}

Protocol ProtocolFromName(const std::string& name) {
  std::string upper = name;
  std::transform(upper.begin(), upper.end(), upper.begin(),
    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  if (upper == "TCP") return Protocol::TCP;
  if (upper == "UDP") return Protocol::UDP;
  if (upper == "ICMP") return Protocol::ICMP;
  if (upper == "ICMPV6") return Protocol::ICMPv6;
  return Protocol::Unknown;
}

const char* ConnectionStateName(ConnectionState s) {
  switch (s) {
    case ConnectionState::Listen:      return "Listen";
    case ConnectionState::Established: return "Established";
    case ConnectionState::SynSent:     return "SynSent";
    case ConnectionState::SynReceived: return "SynReceived";
    case ConnectionState::FinWait1:    return "FinWait1";
    case ConnectionState::FinWait2:    return "FinWait2";
    case ConnectionState::CloseWait:   return "CloseWait";
    case ConnectionState::Closing:     return "Closing";
    case ConnectionState::LastAck:     return "LastAck";
    case ConnectionState::TimeWait:    return "TimeWait";
    case ConnectionState::Closed:      return "Closed";
    case ConnectionState::Unknown:     return "Unknown";
  }
  return "Unknown";
}

ConnectionState ConnectionStateFromName(const std::string& name) {
  std::string upper = name;
  std::transform(upper.begin(), upper.end(), upper.begin(),
    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  if (upper == "LISTEN") return ConnectionState::Listen;
  if (upper == "ESTABLISHED") return ConnectionState::Established;
  if (upper == "SYN_SENT") return ConnectionState::SynSent;
  if (upper == "SYN_RECEIVED") return ConnectionState::SynReceived;
  if (upper == "FIN_WAIT_1") return ConnectionState::FinWait1;
  if (upper == "FIN_WAIT_2") return ConnectionState::FinWait2;
  if (upper == "CLOSE_WAIT") return ConnectionState::CloseWait;
  if (upper == "CLOSING") return ConnectionState::Closing;
  if (upper == "LAST_ACK") return ConnectionState::LastAck;
  if (upper == "TIME_WAIT") return ConnectionState::TimeWait;
  if (upper == "CLOSED") return ConnectionState::Closed;
  return ConnectionState::Unknown;
}

const char* NetworkDetectionOriginName(NetworkDetectionOrigin o) {
  switch (o) {
    case NetworkDetectionOrigin::IPHelper:  return "IPHelper";
    case NetworkDetectionOrigin::WMI:      return "WMI";
    case NetworkDetectionOrigin::Manual:   return "Manual";
    case NetworkDetectionOrigin::Polling:  return "Polling";
  }
  return "Unknown";
}

bool NetworkInterface::isValid() const {
  return id != 0 || !name.empty();
}

bool NetworkInterface::hasAddress(const std::string& addr) const {
  for (const auto& a : ipv4_addresses) {
    if (a == addr) return true;
  }
  for (const auto& a : ipv6_addresses) {
    if (a == addr) return true;
  }
  return false;
}

std::string NetworkInterface::summary() const {
  if (!description.empty()) return description;
  if (!name.empty()) return name;
  return "Unknown Interface";
}

bool NetworkAddress::isValid() const {
  return !address.empty();
}

bool NetworkAddress::isIPv4() const {
  return address.find('.') != std::string::npos && address.find(':') == std::string::npos;
}

bool NetworkAddress::isIPv6() const {
  return address.find(':') != std::string::npos;
}

bool NetworkRoute::isValid() const {
  return !destination.empty();
}

bool NetworkRoute::isDefault() const {
  return destination == "0.0.0.0" || destination == "::" || destination.empty();
}

std::string NetworkRoute::summary() const {
  std::string result = destination;
  if (!gateway.empty()) result += " via " + gateway;
  if (!interface_name.empty()) result += " dev " + interface_name;
  return result;
}

bool NetworkConnection::isValid() const {
  return !local_address.empty() || local_port != 0;
}

bool NetworkConnection::isOutbound() const {
  return remote_port != 0 && remote_address != "0.0.0.0" && remote_address != "::" && !remote_address.empty();
}

bool NetworkConnection::isListening() const {
  return state == ConnectionState::Listen;
}

std::string NetworkConnection::summary() const {
  std::string result = ProtocolName(protocol);
  result += " " + local_address + ":" + std::to_string(local_port);
  if (isOutbound()) {
    result += " -> " + remote_address + ":" + std::to_string(remote_port);
  }
  result += " [" + std::string(ConnectionStateName(state)) + "]";
  return result;
}

}  // namespace monix::collectors::network
