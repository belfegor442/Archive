#pragma once

#include "../ui/screens/Screen.hpp"
#include "../ui/screens/DashboardScreen.hpp"
#include "../ui/screens/TimelineScreen.hpp"
#include "../ui/screens/ProcessScreen.hpp"
#include "../ui/screens/NetworkScreen.hpp"
#include "../ui/screens/HardwareScreen.hpp"
#include "../ui/screens/DiagnosticsScreen.hpp"
#include "../ui/screens/SessionScreen.hpp"
#include "../core/Timeline.hpp"
#include "../core/SessionManager.hpp"
#include "../core/snapshot/SystemSnapshot.hpp"
#include "../core/history/ProcessHistory.hpp"
#include "../core/history/NetworkHistory.hpp"
#include "../core/history/HardwareHistory.hpp"
#include "../core/diagnostics/DiagnosticsEngine.hpp"
#include "../events/EventBus.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace monix::tests {

inline void TestScreenBase() {
  printf("  TestScreenBase... ");

  monix::ui::DashboardScreen dash;
  assert(dash.Name() == L"Dashboard");
  assert(dash.Id() == 0);

  monix::ui::TimelineScreen tl;
  assert(tl.Name() == L"Timeline");
  assert(tl.Id() == 1);

  monix::ui::ProcessScreen proc;
  assert(proc.Name() == L"Processes");
  assert(proc.Id() == 2);

  monix::ui::NetworkScreen net;
  assert(net.Name() == L"Network");
  assert(net.Id() == 3);

  monix::ui::HardwareScreen hw;
  assert(hw.Name() == L"Hardware");
  assert(hw.Id() == 4);

  monix::ui::DiagnosticsScreen diag;
  assert(diag.Name() == L"Diagnostics");
  assert(diag.Id() == 5);

  monix::ui::SessionScreen sess;
  assert(sess.Name() == L"Sessions");
  assert(sess.Id() == 6);

  printf("OK\n");
}

inline void TestScreenButtons() {
  printf("  TestScreenButtons... ");

  monix::ui::Screen screen;
  int clickCount = 0;

  monix::ui::ButtonDef btn;
  btn.label = L"Test Button";
  btn.rect = {10, 10, 110, 30};
  btn.enabled = true;
  btn.onClick = [&]() { clickCount++; };
  screen.AddButton(screen.Buttons().size() > 0 ?
    monix::ui::ButtonDef{} : std::move(btn));

  monix::ui::ScreenContext ctx = {};
  ctx.dc = nullptr;
  ctx.canvas = {};
  ctx.mouseClicked = false;

  screen.UpdateButtons(ctx);
  assert(clickCount == 0);

  monix::ui::ButtonDef btn2;
  btn2.label = L"Click Me";
  btn2.rect = {10, 10, 110, 30};
  btn2.enabled = true;
  btn2.onClick = [&]() { clickCount++; };

  monix::ui::Screen screen2;
  screen2.AddButton(std::move(btn2));

  ctx.mouseX = 50;
  ctx.mouseY = 20;
  ctx.mouseClicked = true;
  screen2.UpdateButtons(ctx);
  assert(clickCount == 1);

  ctx.mouseClicked = false;
  screen2.UpdateButtons(ctx);
  assert(clickCount == 1);

  ctx.mouseX = 200;
  ctx.mouseY = 200;
  ctx.mouseClicked = true;
  screen2.UpdateButtons(ctx);
  assert(clickCount == 1);

  monix::ui::Screen screen3;
  monix::ui::ButtonDef disabled;
  disabled.label = L"Disabled";
  disabled.rect = {10, 10, 110, 30};
  disabled.enabled = false;
  disabled.onClick = [&]() { clickCount++; };
  screen3.AddButton(std::move(disabled));

  ctx.mouseX = 50;
  ctx.mouseY = 20;
  ctx.mouseClicked = true;
  screen3.UpdateButtons(ctx);
  assert(clickCount == 1);

  printf("OK\n");
}

inline void TestDashboardScreenData() {
  printf("  TestDashboardScreenData... ");

  monix::ui::DashboardScreen dash;

  SystemSnapshot snap;
  snap.id = 1;
  snap.timestampNs = 1000000;
  snap.cpu.pct = 75.5;
  snap.thermal.cpuCoreTempC = 72.0;
  snap.memory.usedBytes = 10ULL * 1024 * 1024 * 1024;
  snap.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  snap.memory.compressedBytes = 2ULL * 1024 * 1024 * 1024;
  snap.gpu.pct = 45.0;
  snap.gpu.pctValid = 1;
  snap.gpu.tempC = 68.0;
  snap.gpu.powerW = 120.0;
  snap.gpu.usedBytes = 3ULL * 1024 * 1024 * 1024;
  snap.gpu.totalBytes = 8ULL * 1024 * 1024 * 1024;
  snap.network.upBytesPerSec = 500ULL * 1024;
  snap.network.downBytesPerSec = 2000ULL * 1024;
  snap.network.pingRttMs = 25;
  snap.network.inboundConnections = 50;
  snap.network.outboundConnections = 50;
  snap.storage.readBytesPerSec = 150ULL * 1024 * 1024;
  snap.storage.writeBytesPerSec = 80ULL * 1024 * 1024;
  snap.storage.readIops = 250;
  snap.storage.writeIops = 250;
  snap.storage.queueLength = 2.5;
  snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  snap.storage.tempC = 40.0;
  snap.processes.count = 180;
  snap.processes.threadCount = 2500;
  snap.processes.handleCount = 50000;
  snap.processes.topCpuName = L"chrome.exe";
  snap.processes.topCpuPct = 25.0;
  snap.processes.topRamName = L"game.exe";
  snap.processes.topRamPct = 35.0;

  dash.SetSnapshot(snap);
  dash.SetScore(85);
  dash.SetScramScore(25);

  assert(dash.Name() == L"Dashboard");

  printf("OK\n");
}

inline void TestTimelineScreenData() {
  printf("  TestTimelineScreenData... ");

  Timeline tl;
  tl.AddSnapshot(1000000, 1, L"Snapshot #1");
  tl.AddSnapshot(2000000, 2, L"Snapshot #2");

  SystemEvent evt;
  evt.timestampNs = 1500000;
  evt.type = L"CpuHigh";
  evt.category = EventCategory::Hardware;
  evt.severity = EventSeverity::High;
  evt.description = L"CPU 95%";
  tl.AddEvent(evt);

  tl.AddFinding(101, L"Thermal Warning", 65, 3000000);

  monix::ui::TimelineScreen screen;
  screen.SetTimeline(&tl);
  assert(screen.Name() == L"Timeline");
  assert(screen.Id() == 1);

  printf("OK\n");
}

inline void TestProcessScreenData() {
  printf("  TestProcessScreenData... ");

  ProcessHistory ph;
  ProcessTable table;
  ProcessInfo p1;
  p1.pid = 100;
  p1.name = L"chrome.exe";
  p1.cpuPct = 25.0;
  p1.ramBytes = 500000000;
  p1.threadCount = 10;
  p1.handleCount = 500;
  table.list.push_back(p1);
  table.count = 1;
  table.threadCount = 10;
  table.handleCount = 500;
  ph.RecordSnapshot(table, 1000000, 1);

  monix::ui::ProcessScreen screen;
  screen.SetHistory(&ph);
  assert(screen.Name() == L"Processes");

  printf("OK\n");
}

inline void TestNetworkScreenData() {
  printf("  TestNetworkScreenData... ");

  NetworkHistory nh;
  NetworkState net;
  net.upBytesPerSec = 500ULL * 1024;
  net.downBytesPerSec = 2000ULL * 1024;
  net.pingRttMs = 25;
  net.inboundConnections = 50;
  net.outboundConnections = 50;
  nh.RecordSnapshot(net, 1000000, 1);

  monix::ui::NetworkScreen screen;
  screen.SetHistory(&nh);
  assert(screen.Name() == L"Network");

  printf("OK\n");
}

inline void TestHardwareScreenData() {
  printf("  TestHardwareScreenData... ");

  HardwareHistory hh;
  SystemSnapshot snap;
  snap.id = 1;
  snap.timestampNs = 1000000;
  snap.cpu.pct = 50.0;
  snap.gpu.pct = 40.0;
  snap.gpu.pctValid = 1;
  snap.gpu.tempC = 65.0;
  snap.gpu.powerW = 120.0;
  snap.thermal.cpuCoreTempC = 65.0;
  snap.thermal.cpuPackageTempC = 70.0;
  snap.thermal.gpuTempC = 65.0;
  snap.thermal.ssdTempC = 40.0;
  snap.thermal.fanCount = 3;
  snap.thermal.fanSpeeds = {1200, 1100, 1300};
  snap.storage.readBytesPerSec = 100ULL * 1024 * 1024;
  snap.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
  snap.storage.readIops = 250;
  snap.storage.writeIops = 250;
  snap.storage.queueLength = 2.5;
  snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  snap.storage.tempC = 40.0;
  hh.RecordSnapshot(snap);

  monix::ui::HardwareScreen screen;
  screen.SetHistory(&hh);
  assert(screen.Name() == L"Hardware");

  printf("OK\n");
}

inline void TestDiagnosticsScreenData() {
  printf("  TestDiagnosticsScreenData... ");

  DiagnosticsEngine diag;
  CorrelationEngine corr;
  AnomalyDetector anomaly;

  SystemSnapshot snap;
  snap.id = 1;
  snap.cpu.pct = 50.0;
  snap.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
  snap.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  snap.gpu.pct = 40.0;
  snap.gpu.pctValid = 1;
  snap.thermal.cpuCoreTempC = 65.0;
  snap.network.pingRttMs = 20;
  snap.network.downBytesPerSec = 1000ULL * 1024;
  snap.storage.readBytesPerSec = 100ULL * 1024 * 1024;
  snap.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
  snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  snap.security.unsignedDriverCount = 0;
  snap.security.suspiciousScriptHosts = 0;
  snap.security.lsassAccessCount = 0;
  snap.security.debugPortActive = 0;
  snap.reliability.crashEventsToday = 0;
  snap.reliability.unhandledExceptionCount = 0;
  snap.reliability.heapCorruptionDetected = 0;
  snap.power.batteryFlag = 128;

  auto report = diag.Run(snap, corr, anomaly, 1000000);

  monix::ui::DiagnosticsScreen screen;
  screen.SetReport(&report);
  assert(screen.Name() == L"Diagnostics");

  printf("OK\n");
}

inline void TestSessionScreenData() {
  printf("  TestSessionScreenData... ");

  SessionManager sm;
  auto& session = sm.StartSession(L"Test Session");
  SystemSnapshot snap;
  snap.id = 1;
  snap.cpu.pct = 50.0;
  sm.RecordSnapshot(snap);
  sm.StopSession();

  monix::ui::SessionScreen screen;
  screen.SetSessionManager(&sm);
  assert(screen.Name() == L"Sessions");

  printf("OK\n");
}

inline void RunFase5Tests() {
  printf("Running FASE 5 tests...\n");
  TestScreenBase();
  TestScreenButtons();
  TestDashboardScreenData();
  TestTimelineScreenData();
  TestProcessScreenData();
  TestNetworkScreenData();
  TestHardwareScreenData();
  TestDiagnosticsScreenData();
  TestSessionScreenData();
  printf("All FASE 5 tests passed!\n");
}

}
