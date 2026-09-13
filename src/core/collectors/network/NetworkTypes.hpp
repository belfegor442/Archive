#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monix::collectors::network {

using InterfaceId = std::uint32_t;
using Port = std::uint16_t;

enum class NetworkEventKind : std::uint8_t {
  InterfaceCreated,
  InterfaceRemoved,
  InterfaceUp,
  InterfaceDown,
  AddressAdded,
  AddressRemoved,
  RouteChanged,
  ConnectionStarted,
  ConnectionClosed
};

const char* NetworkEventKindName(NetworkEventKind k);
std::string NetworkEventKindAction(NetworkEventKind k);

enum class InterfaceState : std::uint8_t {
  Up,
  Down,
  Testing,
  Unknown,
  Dormant,
  NotPresent,
  LowerLayerDown
};

const char* InterfaceStateName(InterfaceState s);

enum class Protocol : std::uint8_t {
  TCP,
  UDP,
  ICMP,
  ICMPv6,
  Unknown
};

const char* ProtocolName(Protocol p);
Protocol ProtocolFromName(const std::string& name);

enum class ConnectionState : std::uint8_t {
  Listen,
  Established,
  SynSent,
  SynReceived,
  FinWait1,
  FinWait2,
  CloseWait,
  Closing,
  LastAck,
  TimeWait,
  Closed,
  Unknown
};

const char* ConnectionStateName(ConnectionState s);
ConnectionState ConnectionStateFromName(const std::string& name);

enum class NetworkDetectionOrigin : std::uint8_t {
  IPHelper,
  WMI,
  Manual,
  Polling
};

const char* NetworkDetectionOriginName(NetworkDetectionOrigin o);

struct NetworkInterface {
  InterfaceId id = 0;
  std::string name;
  std::string description;
  std::string mac_address;
  std::vector<std::string> ipv4_addresses;
  std::vector<std::string> ipv6_addresses;
  InterfaceState state = InterfaceState::Unknown;
  std::uint64_t bytes_sent = 0;
  std::uint64_t bytes_received = 0;
  std::uint32_t mtu = 0;
  bool is_loopback = false;

  bool isValid() const;
  bool hasAddress(const std::string& addr) const;
  std::string summary() const;
};

struct NetworkAddress {
  std::string address;
  std::uint32_t prefix_length = 0;
  InterfaceId interface_id = 0;
  std::string interface_name;

  bool isValid() const;
  bool isIPv4() const;
  bool isIPv6() const;
};

struct NetworkRoute {
  std::string destination;
  std::uint32_t prefix_length = 0;
  std::string gateway;
  std::string interface_name;
  std::uint32_t metric = 0;

  bool isValid() const;
  bool isDefault() const;
  std::string summary() const;
};

struct NetworkConnection {
  std::string local_address;
  Port local_port = 0;
  std::string remote_address;
  Port remote_port = 0;
  Protocol protocol = Protocol::Unknown;
  ConnectionState state = ConnectionState::Unknown;
  std::uint32_t pid = 0;
  std::string process_name;

  bool isValid() const;
  bool isOutbound() const;
  bool isListening() const;
  std::string summary() const;
};

}  // namespace monix::collectors::network
