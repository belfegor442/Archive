#pragma once

#include "../core/history/ProcessHistory.hpp"
#include "../core/history/NetworkHistory.hpp"
#include "../core/history/HardwareHistory.hpp"
#include "../core/snapshot/SystemSnapshot.hpp"

#include <cassert>
#include <cstdio>

namespace monix::tests {

inline void TestProcessHistory() {
  printf("  TestProcessHistory... ");

  ProcessHistory ph;
  assert(ph.AliveCount() == 0);

  ProcessTable table;
  ProcessInfo p1;
  p1.pid = 100;
  p1.name = L"chrome.exe";
  p1.cpuPct = 25.0;
  p1.ramBytes = 500000000;
  p1.threadCount = 10;
  p1.handleCount = 500;
  table.list.push_back(p1);

  ProcessInfo p2;
  p2.pid = 200;
  p2.name = L"game.exe";
  p2.cpuPct = 80.0;
  p2.ramBytes = 2000000000;
  p2.threadCount = 8;
  p2.handleCount = 300;
  table.list.push_back(p2);
  table.count = 2;
  table.threadCount = 18;
  table.handleCount = 800;

  ph.RecordSnapshot(table, 1000000, 1);
  assert(ph.AliveCount() == 2);
  assert(ph.DeadCount() == 0);

  auto topCpu = ph.TopCpuProcesses(1);
  assert(topCpu.size() == 1);
  assert(topCpu[0].pid == 200);

  auto topRam = ph.TopRamProcesses(1);
  assert(topRam.size() == 1);
  assert(topRam[0].pid == 200);

  auto chrome = ph.ProcessesByName(L"chrome.exe");
  assert(chrome.size() == 1);
  assert(chrome[0].pid == 100);

  ProcessTable table2;
  ProcessInfo p3;
  p3.pid = 300;
  p3.name = L"notepad.exe";
  p3.cpuPct = 5.0;
  p3.ramBytes = 50000000;
  table2.list.push_back(p3);
  p1.cpuPct = 30.0;
  p1.ramBytes = 600000000;
  table2.list.push_back(p1);
  p2.cpuPct = 90.0;
  p2.ramBytes = 2500000000;
  table2.list.push_back(p2);
  table2.count = 3;

  ph.RecordSnapshot(table2, 2000000, 2);
  assert(ph.AliveCount() == 3);
  assert(ph.DeadCount() == 0);

  ProcessTable table3;
  table3.list.push_back(p1);
  table3.count = 1;

  ph.RecordSnapshot(table3, 3000000, 3);
  assert(ph.AliveCount() == 1);
  assert(ph.DeadCount() == 2);

  auto dead = ph.DeadProcesses();
  assert(dead.size() == 2);

  auto newProcs = ph.NewProcessesSince(1500000);
  assert(newProcs.size() == 1);
  assert(newProcs.count(300) == 1);

  auto deadProcs = ph.DeadProcessesSince(2500000);
  assert(deadProcs.size() == 2);

  auto highCpu = ph.HighCpuProcesses(50.0);
  assert(highCpu.size() == 0);

  auto trends = ph.DetectTrends();

  ph.Clear();
  assert(ph.AliveCount() == 0);

  printf("OK\n");
}

inline void TestNetworkHistory() {
  printf("  TestNetworkHistory... ");

  NetworkHistory nh;
  assert(nh.SampleCount() == 0);

  NetworkState net;
  net.upKbps = 500.0;
  net.downKbps = 2000.0;
  net.rttMs = 25.0;
  net.activeConns = 50;
  net.droppedPktPct = 0.1;
  net.tcpRetransmitPct = 0.5;
  net.totalConns = 100;

  nh.RecordSnapshot(net, 1000000, 1);
  assert(nh.SampleCount() == 1);

  net.rttMs = 150.0;
  net.droppedPktPct = 6.0;
  nh.RecordSnapshot(net, 2000000, 2);
  assert(nh.SampleCount() == 2);

  auto recent = nh.RecentSamples(10);
  assert(recent.size() == 2);
  assert(recent[1].rttMs == 150.0);

  auto latest = nh.LatestSample();
  assert(latest.rttMs == 150.0);

  assert(nh.AvgRtt() == 87.5);
  assert(nh.MaxRtt() == 150.0);
  assert(nh.MinRtt() == 25.0);

  auto alerts = nh.RecentAlerts();
  assert(alerts.size() >= 2);

  bool foundLatency = false;
  bool foundDrops = false;
  for (const auto& a : alerts) {
    if (a.type == L"HIGH_LATENCY") foundLatency = true;
    if (a.type == L"HIGH_DROPS") foundDrops = true;
  }
  assert(foundLatency);
  assert(foundDrops);

  std::vector<NetworkInterface> ifaces;
  NetworkInterface iface;
  iface.name = L"Ethernet";
  iface.description = L"Realtek PCIe GbE";
  iface.isWifi = false;
  iface.speedMbps = 1000.0;
  ifaces.push_back(iface);
  nh.SetInterfaces(ifaces);
  assert(nh.Interfaces().size() == 1);

  std::vector<NetworkConnection> conns;
  NetworkConnection conn;
  conn.localAddress = L"192.168.1.100";
  conn.localPort = 443;
  conn.remoteAddress = L"1.1.1.1";
  conn.remotePort = 443;
  conn.state = L"ESTABLISHED";
  conn.processId = 100;
  conn.processName = L"chrome.exe";
  conn.protocol = L"TCP";
  conns.push_back(conn);
  nh.SetConnections(conns);

  auto chromeConns = nh.ConnectionsByProcess(100);
  assert(chromeConns.size() == 1);

  auto estConns = nh.ConnectionsByState(L"ESTABLISHED");
  assert(estConns.size() == 1);

  auto trend = nh.RttTrend();
  assert(!trend.direction.empty());

  nh.Clear();
  assert(nh.SampleCount() == 0);

  printf("OK\n");
}

inline void TestHardwareHistory() {
  printf("  TestHardwareHistory... ");

  HardwareHistory hh;

  SystemSnapshot snap;
  snap.id = 1;
  snap.timestampNs = 1000000;
  snap.cpu.pct = 50.0;
  snap.cpu.coresActive = 4;
  snap.cpu.packagePowerW = 65.0;
  snap.gpu.pct = 40.0;
  snap.gpu.pctValid = 1;
  snap.gpu.tempC = 65.0;
  snap.gpu.powerW = 120.0;
  snap.gpu.usedBytes = 2ULL * 1024 * 1024 * 1024;
  snap.gpu.totalBytes = 8ULL * 1024 * 1024 * 1024;
  snap.thermal.cpuCoreTempC = 65.0;
  snap.thermal.cpuPackageTempC = 70.0;
  snap.thermal.gpuTempC = 65.0;
  snap.thermal.ssdTempC = 40.0;
  snap.thermal.fanCount = 3;
  snap.thermal.fanSpeeds = {1200, 1100, 1300};
  snap.thermal.cpuThrottling = 0;
  snap.storage.diskReadMBs = 100.0;
  snap.storage.diskWriteMBs = 50.0;
  snap.storage.iops = 500.0;
  snap.storage.queueDepth = 2.5;
  snap.storage.activeTimePct = 35.0;
  snap.storage.ready = 1;
  snap.storage.wearPct = 10.0;
  snap.storage.tempC = 40.0;
  snap.power.batteryPresent = 1;
  snap.power.batteryChargePct = 85.0;
  snap.power.batteryChargeCycles = 150;
  snap.power.acConnected = 1;

  hh.RecordSnapshot(snap);

  snap.id = 2;
  snap.timestampNs = 2000000;
  snap.cpu.pct = 70.0;
  snap.thermal.cpuCoreTempC = 78.0;
  hh.RecordSnapshot(snap);

  snap.id = 3;
  snap.timestampNs = 3000000;
  snap.cpu.pct = 85.0;
  snap.thermal.cpuCoreTempC = 88.0;
  snap.thermal.cpuThrottling = 1;
  hh.RecordSnapshot(snap);

  auto cpuHist = hh.CpuHistory();
  assert(cpuHist.size() == 3);
  assert(cpuHist[0].overallPct == 50.0);
  assert(cpuHist[2].overallPct == 85.0);

  auto thermalHist = hh.ThermalHistory();
  assert(thermalHist.size() == 3);
  assert(thermalHist[0].cpuCoreTempC == 65.0);

  auto gpuHist = hh.GpuHistory();
  assert(gpuHist.size() == 3);

  auto storageHist = hh.StorageHistory();
  assert(storageHist.size() == 3);

  auto powerHist = hh.PowerHistory();
  assert(powerHist.size() == 3);

  assert(hh.AvgCpu() == 68.33333333333333);
  assert(hh.MaxCpu() == 85.0);
  assert(hh.AvgTemp() == 77.0);
  assert(hh.MaxTemp() == 88.0);

  auto alerts = hh.RecentAlerts();
  assert(alerts.size() >= 2);

  bool foundThermal = false;
  bool foundThrottle = false;
  for (const auto& a : alerts) {
    if (a.type == L"THERMAL_HIGH" || a.type == L"THERMAL_CRITICAL") foundThermal = true;
    if (a.type == L"THROTTLING") foundThrottle = true;
  }
  assert(foundThermal);
  assert(foundThrottle);

  auto trends = hh.DetectTrends();
  assert(trends.size() > 0);

  hh.Clear();
  assert(hh.CpuHistory().empty());

  printf("OK\n");
}

inline void RunFase4Tests() {
  printf("Running FASE 4 tests...\n");
  TestProcessHistory();
  TestNetworkHistory();
  TestHardwareHistory();
  printf("All FASE 4 tests passed!\n");
}

}
