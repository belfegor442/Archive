#include "../../../core/collectors/network/NetworkTypes.hpp"
#include "../../../core/collectors/network/NetworkCollector.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace monix::collectors::network;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-62s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)

static NetworkInterface makeIface(InterfaceId id, const std::string& name, InterfaceState state = InterfaceState::Up) {
  NetworkInterface iface;
  iface.id = id;
  iface.name = name;
  iface.description = "Test " + name;
  iface.state = state;
  iface.mac_address = "AA:BB:CC:DD:EE:FF";
  iface.ipv4_addresses.push_back("192.168.1." + std::to_string(id));
  iface.mtu = 1500;
  return iface;
}

static NetworkAddress makeAddr(const std::string& addr, InterfaceId iid = 1) {
  NetworkAddress a;
  a.address = addr;
  a.prefix_length = 24;
  a.interface_id = iid;
  a.interface_name = "eth" + std::to_string(iid);
  return a;
}

static NetworkRoute makeRoute(const std::string& dest, const std::string& gw) {
  NetworkRoute r;
  r.destination = dest;
  r.gateway = gw;
  r.interface_name = "eth1";
  r.metric = 10;
  return r;
}

static NetworkConnection makeConn(const std::string& local, Port lp,
                                   const std::string& remote, Port rp,
                                   Protocol proto, ConnectionState state) {
  NetworkConnection c;
  c.local_address = local;
  c.local_port = lp;
  c.remote_address = remote;
  c.remote_port = rp;
  c.protocol = proto;
  c.state = state;
  c.pid = 1234;
  c.process_name = "test.exe";
  return c;
}

// ==================== NetworkEventKind Tests ====================

static void test_event_kind_names() {
  TEST("NetworkEventKind: all names");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::InterfaceCreated)), "InterfaceCreated");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::InterfaceRemoved)), "InterfaceRemoved");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::InterfaceUp)), "InterfaceUp");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::InterfaceDown)), "InterfaceDown");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::AddressAdded)), "AddressAdded");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::AddressRemoved)), "AddressRemoved");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::RouteChanged)), "RouteChanged");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::ConnectionStarted)), "ConnectionStarted");
  ASSERT_EQ(std::string(NetworkEventKindName(NetworkEventKind::ConnectionClosed)), "ConnectionClosed");
  PASS();
}

static void test_event_kind_actions() {
  TEST("NetworkEventKind: action strings");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::InterfaceCreated), "interface.created");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::InterfaceRemoved), "interface.removed");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::InterfaceUp), "interface.up");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::InterfaceDown), "interface.down");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::AddressAdded), "address.added");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::AddressRemoved), "address.removed");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::RouteChanged), "route.changed");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::ConnectionStarted), "connection.started");
  ASSERT_EQ(NetworkEventKindAction(NetworkEventKind::ConnectionClosed), "connection.closed");
  PASS();
}

// ==================== InterfaceState Tests ====================

static void test_interface_state_names() {
  TEST("InterfaceState: all names");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::Up)), "Up");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::Down)), "Down");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::Testing)), "Testing");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::Unknown)), "Unknown");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::Dormant)), "Dormant");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::NotPresent)), "NotPresent");
  ASSERT_EQ(std::string(InterfaceStateName(InterfaceState::LowerLayerDown)), "LowerLayerDown");
  PASS();
}

// ==================== Protocol Tests ====================

static void test_protocol_names() {
  TEST("Protocol: all names");
  ASSERT_EQ(std::string(ProtocolName(Protocol::TCP)), "TCP");
  ASSERT_EQ(std::string(ProtocolName(Protocol::UDP)), "UDP");
  ASSERT_EQ(std::string(ProtocolName(Protocol::ICMP)), "ICMP");
  ASSERT_EQ(std::string(ProtocolName(Protocol::ICMPv6)), "ICMPv6");
  ASSERT_EQ(std::string(ProtocolName(Protocol::Unknown)), "Unknown");
  PASS();
}

static void test_protocol_from_name() {
  TEST("ProtocolFromName: recognizes protocols");
  ASSERT_EQ(ProtocolFromName("TCP"), Protocol::TCP);
  ASSERT_EQ(ProtocolFromName("tcp"), Protocol::TCP);
  ASSERT_EQ(ProtocolFromName("UDP"), Protocol::UDP);
  ASSERT_EQ(ProtocolFromName("udp"), Protocol::UDP);
  ASSERT_EQ(ProtocolFromName("ICMP"), Protocol::ICMP);
  ASSERT_EQ(ProtocolFromName("ICMPv6"), Protocol::ICMPv6);
  ASSERT_EQ(ProtocolFromName("unknown"), Protocol::Unknown);
  PASS();
}

// ==================== ConnectionState Tests ====================

static void test_connection_state_names() {
  TEST("ConnectionState: all names");
  ASSERT_EQ(std::string(ConnectionStateName(ConnectionState::Listen)), "Listen");
  ASSERT_EQ(std::string(ConnectionStateName(ConnectionState::Established)), "Established");
  ASSERT_EQ(std::string(ConnectionStateName(ConnectionState::SynSent)), "SynSent");
  ASSERT_EQ(std::string(ConnectionStateName(ConnectionState::Closed)), "Closed");
  ASSERT_EQ(std::string(ConnectionStateName(ConnectionState::TimeWait)), "TimeWait");
  ASSERT_EQ(std::string(ConnectionStateName(ConnectionState::Unknown)), "Unknown");
  PASS();
}

static void test_connection_state_from_name() {
  TEST("ConnectionStateFromName: recognizes states");
  ASSERT_EQ(ConnectionStateFromName("LISTEN"), ConnectionState::Listen);
  ASSERT_EQ(ConnectionStateFromName("ESTABLISHED"), ConnectionState::Established);
  ASSERT_EQ(ConnectionStateFromName("SYN_SENT"), ConnectionState::SynSent);
  ASSERT_EQ(ConnectionStateFromName("CLOSED"), ConnectionState::Closed);
  ASSERT_EQ(ConnectionStateFromName("TIME_WAIT"), ConnectionState::TimeWait);
  ASSERT_EQ(ConnectionStateFromName("CLOSE_WAIT"), ConnectionState::CloseWait);
  ASSERT_EQ(ConnectionStateFromName("unknown"), ConnectionState::Unknown);
  PASS();
}

// ==================== DetectionOrigin Tests ====================

static void test_detection_origin_names() {
  TEST("NetworkDetectionOrigin: all names");
  ASSERT_EQ(std::string(NetworkDetectionOriginName(NetworkDetectionOrigin::IPHelper)), "IPHelper");
  ASSERT_EQ(std::string(NetworkDetectionOriginName(NetworkDetectionOrigin::WMI)), "WMI");
  ASSERT_EQ(std::string(NetworkDetectionOriginName(NetworkDetectionOrigin::Manual)), "Manual");
  ASSERT_EQ(std::string(NetworkDetectionOriginName(NetworkDetectionOrigin::Polling)), "Polling");
  PASS();
}

// ==================== NetworkInterface Tests ====================

static void test_network_interface_is_valid() {
  TEST("NetworkInterface: isValid checks id or name");
  NetworkInterface withId;
  withId.id = 1;
  ASSERT_TRUE(withId.isValid());

  NetworkInterface withName;
  withName.name = "eth0";
  ASSERT_TRUE(withName.isValid());

  NetworkInterface empty;
  ASSERT_FALSE(empty.isValid());
  PASS();
}

static void test_network_interface_has_address() {
  TEST("NetworkInterface: hasAddress checks ipv4 and ipv6");
  NetworkInterface iface = makeIface(1, "eth0");
  ASSERT_TRUE(iface.hasAddress("192.168.1.1"));
  ASSERT_FALSE(iface.hasAddress("10.0.0.1"));
  PASS();
}

static void test_network_interface_summary() {
  TEST("NetworkInterface: summary prefers description");
  NetworkInterface full = makeIface(1, "eth0");
  full.description = "Intel Ethernet";
  ASSERT_EQ(full.summary(), "Intel Ethernet");

  NetworkInterface noDesc;
  noDesc.name = "eth0";
  ASSERT_EQ(noDesc.summary(), "eth0");

  NetworkInterface empty;
  ASSERT_EQ(empty.summary(), "Unknown Interface");
  PASS();
}

// ==================== NetworkAddress Tests ====================

static void test_network_address_is_valid() {
  TEST("NetworkAddress: isValid checks address");
  NetworkAddress valid = makeAddr("192.168.1.1");
  ASSERT_TRUE(valid.isValid());

  NetworkAddress empty;
  ASSERT_FALSE(empty.isValid());
  PASS();
}

static void test_network_address_is_ip() {
  TEST("NetworkAddress: isIPv4 and isIPv6");
  NetworkAddress v4 = makeAddr("192.168.1.1");
  ASSERT_TRUE(v4.isIPv4());
  ASSERT_FALSE(v4.isIPv6());

  NetworkAddress v6 = makeAddr("::1");
  ASSERT_FALSE(v6.isIPv4());
  ASSERT_TRUE(v6.isIPv6());
  PASS();
}

// ==================== NetworkRoute Tests ====================

static void test_network_route_is_valid() {
  TEST("NetworkRoute: isValid checks destination");
  NetworkRoute valid = makeRoute("10.0.0.0", "192.168.1.1");
  ASSERT_TRUE(valid.isValid());

  NetworkRoute empty;
  ASSERT_FALSE(empty.isValid());
  PASS();
}

static void test_network_route_is_default() {
  TEST("NetworkRoute: isDefault checks 0.0.0.0 or ::");
  NetworkRoute def = makeRoute("0.0.0.0", "192.168.1.1");
  ASSERT_TRUE(def.isDefault());

  NetworkRoute specific = makeRoute("10.0.0.0", "192.168.1.1");
  ASSERT_FALSE(specific.isDefault());

  NetworkRoute ipv6def = makeRoute("::", "fe80::1");
  ASSERT_TRUE(ipv6def.isDefault());
  PASS();
}

// ==================== NetworkConnection Tests ====================

static void test_network_connection_is_valid() {
  TEST("NetworkConnection: isValid checks local addr/port");
  NetworkConnection valid = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  ASSERT_TRUE(valid.isValid());

  NetworkConnection empty;
  ASSERT_FALSE(empty.isValid());
  PASS();
}

static void test_network_connection_is_outbound() {
  TEST("NetworkConnection: isOutbound checks remote");
  NetworkConnection outbound = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  ASSERT_TRUE(outbound.isOutbound());

  NetworkConnection listening = makeConn("192.168.1.1", 80, "0.0.0.0", 0, Protocol::TCP, ConnectionState::Listen);
  ASSERT_FALSE(listening.isOutbound());
  PASS();
}

static void test_network_connection_is_listening() {
  TEST("NetworkConnection: isListening checks state");
  NetworkConnection listening = makeConn("192.168.1.1", 80, "0.0.0.0", 0, Protocol::TCP, ConnectionState::Listen);
  ASSERT_TRUE(listening.isListening());

  NetworkConnection established = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  ASSERT_FALSE(established.isListening());
  PASS();
}

static void test_network_connection_summary() {
  TEST("NetworkConnection: summary format");
  NetworkConnection conn = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  std::string sum = conn.summary();
  ASSERT_TRUE(sum.find("TCP") != std::string::npos);
  ASSERT_TRUE(sum.find("192.168.1.1:80") != std::string::npos);
  ASSERT_TRUE(sum.find("10.0.0.1:443") != std::string::npos);
  ASSERT_TRUE(sum.find("Established") != std::string::npos);
  PASS();
}

// ==================== Collector Lifecycle Tests ====================

static void test_collector_start_stop() {
  TEST("NetworkCollector: start and stop lifecycle");
  NetworkCollector collector;
  ASSERT_FALSE(collector.isRunning());
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  PASS();
}

static void test_collector_double_start() {
  TEST("NetworkCollector: double start returns false");
  NetworkCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("NetworkCollector: stop without start returns false");
  NetworkCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_config_defaults() {
  TEST("NetworkCollectorConfig: defaults are safe");
  auto cfg = NetworkCollectorConfig::defaults();
  ASSERT_TRUE(cfg.track_interfaces);
  ASSERT_TRUE(cfg.track_addresses);
  ASSERT_TRUE(cfg.track_routes);
  ASSERT_TRUE(cfg.track_connections);
  ASSERT_FALSE(cfg.log_content);
  ASSERT_TRUE(cfg.max_events > 0);
  PASS();
}

// ==================== Interface Up Tests ====================

static void test_interface_up() {
  TEST("Interface up: interface up event detected");
  NetworkCollector collector;
  std::atomic<int> count{0};
  NetworkEventKind capturedKind = NetworkEventKind::InterfaceDown;

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
    capturedKind = kind;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkInterface iface = makeIface(1, "eth0");
  collector.reportInterfaceUp(iface);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(capturedKind, NetworkEventKind::InterfaceUp);
  ASSERT_EQ(collector.interfacesTracked(), 1u);
  collector.stop();
  PASS();
}

// ==================== Interface Down Tests ====================

static void test_interface_down() {
  TEST("Interface down: interface down event detected");
  NetworkCollector collector;
  std::atomic<int> count{0};
  NetworkEventKind capturedKind = NetworkEventKind::InterfaceUp;

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
    capturedKind = kind;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkInterface iface = makeIface(1, "eth0");
  collector.reportInterfaceUp(iface);
  collector.reportInterfaceDown(iface);

  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(capturedKind, NetworkEventKind::InterfaceDown);
  collector.stop();
  PASS();
}

static void test_interface_create_remove() {
  TEST("Interface: create and remove tracked");
  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkInterface iface = makeIface(1, "eth0");
  collector.reportInterfaceCreated(iface);
  ASSERT_EQ(collector.interfacesTracked(), 1u);

  collector.reportInterfaceRemoved(iface);
  ASSERT_EQ(collector.interfacesTracked(), 0u);
  ASSERT_EQ(count.load(), 2);
  collector.stop();
  PASS();
}

// ==================== Address Tests ====================

static void test_address_added_removed() {
  TEST("Address: add and remove detected");
  NetworkCollector collector;
  std::atomic<int> count{0};
  NetworkEventKind lastKind = NetworkEventKind::InterfaceCreated;

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
    lastKind = kind;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkAddress addr = makeAddr("192.168.1.100");
  collector.reportAddressAdded(addr);
  collector.reportAddressRemoved(addr);

  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(lastKind, NetworkEventKind::AddressRemoved);
  collector.stop();
  PASS();
}

// ==================== Route Tests ====================

static void test_route_changed() {
  TEST("Route: change detected");
  NetworkCollector collector;
  std::atomic<int> count{0};
  NetworkEventKind lastKind = NetworkEventKind::InterfaceCreated;

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
    lastKind = kind;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkRoute route = makeRoute("10.0.0.0", "192.168.1.1");
  collector.reportRouteChanged(route);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(lastKind, NetworkEventKind::RouteChanged);
  collector.stop();
  PASS();
}

// ==================== Connection Tests ====================

static void test_connection_started_closed() {
  TEST("Connection: start and close detected");
  NetworkCollector collector;
  std::atomic<int> count{0};
  NetworkEventKind lastKind = NetworkEventKind::InterfaceCreated;

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
    lastKind = kind;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkConnection conn = makeConn("192.168.1.1", 49152, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  collector.reportConnectionStarted(conn);
  ASSERT_EQ(collector.currentConnections().size(), 1u);

  collector.reportConnectionClosed(conn);
  ASSERT_EQ(collector.currentConnections().size(), 0u);
  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(lastKind, NetworkEventKind::ConnectionClosed);
  collector.stop();
  PASS();
}

static void test_connection_correlation() {
  TEST("Connection: carries process correlation");
  NetworkCollector collector;
  std::uint32_t capturedPid = 0;
  std::string capturedProcess;

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkConnection conn = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  conn.pid = 5678;
  conn.process_name = "chrome.exe";
  collector.reportConnectionStarted(conn);

  auto conns = collector.currentConnections();
  ASSERT_TRUE(conns.size() == 1);
  ASSERT_EQ(conns[0].pid, 5678u);
  ASSERT_EQ(conns[0].process_name, "chrome.exe");
  collector.stop();
  PASS();
}

// ==================== Rapid Changes Tests ====================

static void test_rapid_changes() {
  TEST("Rapid changes: dedup suppresses rapid events");
  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkAddress addr = makeAddr("192.168.1.100");
  for (int i = 0; i < 10; i++) {
    collector.reportAddressAdded(addr);
  }

  ASSERT_EQ(count.load(), 1);
  collector.stop();
  PASS();
}

static void test_different_not_deduped() {
  TEST("Rapid changes: different addresses not suppressed");
  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkAddress a1 = makeAddr("192.168.1.100");
  NetworkAddress a2 = makeAddr("192.168.1.101");
  NetworkAddress a3 = makeAddr("192.168.1.102");
  collector.reportAddressAdded(a1);
  collector.reportAddressAdded(a2);
  collector.reportAddressAdded(a3);

  ASSERT_EQ(count.load(), 3);
  collector.stop();
  PASS();
}

// ==================== Storm Test ====================

static void test_storm_suppression() {
  TEST("Storm: massive rapid fire suppressed");
  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  collector.start(cfg);

  NetworkConnection conn = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  for (int i = 0; i < 1000; i++) {
    collector.reportConnectionStarted(conn);
  }

  ASSERT_EQ(count.load(), 1);
  collector.stop();
  PASS();
}

// ==================== Config Filtering Tests ====================

static void test_config_disable_interfaces() {
  TEST("Config: disable interface tracking");
  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  cfg.track_interfaces = false;

  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  collector.start(cfg);

  NetworkInterface iface = makeIface(1, "eth0");
  collector.reportInterfaceUp(iface);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.interfacesTracked(), 0u);
  collector.stop();
  PASS();
}

static void test_config_disable_connections() {
  TEST("Config: disable connection tracking");
  NetworkCollectorConfig cfg = NetworkCollectorConfig::defaults();
  cfg.track_connections = false;

  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  collector.start(cfg);

  NetworkConnection conn = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  collector.reportConnectionStarted(conn);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.currentConnections().size(), 0u);
  collector.stop();
  PASS();
}

// ==================== Permission Tests ====================

static void test_report_when_not_running() {
  TEST("Permission: report when not running ignored");
  NetworkCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](NetworkEventKind kind, NetworkDetectionOrigin origin, const std::string& summary) {
    count++;
  });

  NetworkInterface iface = makeIface(1, "eth0");
  collector.reportInterfaceUp(iface);
  collector.reportInterfaceDown(iface);

  NetworkAddress addr = makeAddr("192.168.1.100");
  collector.reportAddressAdded(addr);
  collector.reportAddressRemoved(addr);

  NetworkRoute route = makeRoute("10.0.0.0", "192.168.1.1");
  collector.reportRouteChanged(route);

  NetworkConnection conn = makeConn("192.168.1.1", 80, "10.0.0.1", 443, Protocol::TCP, ConnectionState::Established);
  collector.reportConnectionStarted(conn);
  collector.reportConnectionClosed(conn);

  ASSERT_EQ(count.load(), 0);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Network Collector Tests ===\n\n");

  printf("[NetworkEventKind]\n");
  test_event_kind_names();
  test_event_kind_actions();

  printf("[InterfaceState]\n");
  test_interface_state_names();

  printf("[Protocol]\n");
  test_protocol_names();
  test_protocol_from_name();

  printf("[ConnectionState]\n");
  test_connection_state_names();
  test_connection_state_from_name();

  printf("[DetectionOrigin]\n");
  test_detection_origin_names();

  printf("[NetworkInterface]\n");
  test_network_interface_is_valid();
  test_network_interface_has_address();
  test_network_interface_summary();

  printf("[NetworkAddress]\n");
  test_network_address_is_valid();
  test_network_address_is_ip();

  printf("[NetworkRoute]\n");
  test_network_route_is_valid();
  test_network_route_is_default();

  printf("[NetworkConnection]\n");
  test_network_connection_is_valid();
  test_network_connection_is_outbound();
  test_network_connection_is_listening();
  test_network_connection_summary();

  printf("[Collector Lifecycle]\n");
  test_collector_start_stop();
  test_collector_double_start();
  test_collector_stop_without_start();
  test_collector_config_defaults();

  printf("[Interface Up/Down]\n");
  test_interface_up();
  test_interface_down();
  test_interface_create_remove();

  printf("[Address]\n");
  test_address_added_removed();

  printf("[Route]\n");
  test_route_changed();

  printf("[Connection]\n");
  test_connection_started_closed();
  test_connection_correlation();

  printf("[Rapid Changes]\n");
  test_rapid_changes();
  test_different_not_deduped();

  printf("[Storm]\n");
  test_storm_suppression();

  printf("[Config Filtering]\n");
  test_config_disable_interfaces();
  test_config_disable_connections();

  printf("[Permission]\n");
  test_report_when_not_running();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_NetworkCollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"NetworkEventKind: names", test_event_kind_names},
  {"NetworkEventKind: actions", test_event_kind_actions},
  {"InterfaceState: names", test_interface_state_names},
  {"Protocol: names", test_protocol_names},
  {"Protocol: fromName", test_protocol_from_name},
  {"ConnectionState: names", test_connection_state_names},
  {"ConnectionState: fromName", test_connection_state_from_name},
  {"NetworkDetectionOrigin: names", test_detection_origin_names},
  {"NetworkInterface: isValid", test_network_interface_is_valid},
  {"NetworkInterface: hasAddress", test_network_interface_has_address},
  {"NetworkInterface: summary", test_network_interface_summary},
  {"NetworkAddress: isValid", test_network_address_is_valid},
  {"NetworkAddress: isIPv4 isIPv6", test_network_address_is_ip},
  {"NetworkRoute: isValid", test_network_route_is_valid},
  {"NetworkRoute: isDefault", test_network_route_is_default},
  {"NetworkConnection: isValid", test_network_connection_is_valid},
  {"NetworkConnection: isOutbound", test_network_connection_is_outbound},
  {"NetworkConnection: isListening", test_network_connection_is_listening},
  {"NetworkConnection: summary", test_network_connection_summary},
  {"NetworkCollector: start stop", test_collector_start_stop},
  {"NetworkCollector: double start", test_collector_double_start},
  {"NetworkCollector: stop without start", test_collector_stop_without_start},
  {"NetworkCollectorConfig: defaults", test_collector_config_defaults},
  {"Interface up: detection", test_interface_up},
  {"Interface down: detection", test_interface_down},
  {"Interface: create remove", test_interface_create_remove},
  {"Address: add remove", test_address_added_removed},
  {"Route: change detected", test_route_changed},
  {"Connection: start close", test_connection_started_closed},
  {"Connection: correlation", test_connection_correlation},
  {"Rapid changes: dedup", test_rapid_changes},
  {"Rapid changes: different not deduped", test_different_not_deduped},
  {"Storm: suppression", test_storm_suppression},
  {"Config: disable interfaces", test_config_disable_interfaces},
  {"Config: disable connections", test_config_disable_connections},
  {"Permission: not running ignored", test_report_when_not_running},
};

const KTestEntry* GetKTests_NetworkCollector() { return s_ktests; }
std::size_t GetKTestCount_NetworkCollector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
