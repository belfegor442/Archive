#include "DemoLogs.hpp"

#include "../../MonixApp.hpp"

#include <windows.h>
#include <string>

using namespace monix;

void MonixApp::SpawnDemoLogs() {
  if (!state_.loggedIn) {
    SetToast(L"Access denied — authentication required");
    return;
  }

  struct DemoEntry {
    std::wstring domain;
    std::wstring severity;
    std::wstring message;
    ColorRole color;
  };
  const DemoEntry entries[] = {
    { L"PROCESS",    L"INFO",     L"chrome.exe entered the active task set (PID 16376)", ColorRole::White },
    { L"PROCESS",    L"WARNING",  L"chrome.exe memory allocation increased rapidly", ColorRole::Warning },
    { L"PROCESS",    L"ERROR",    L"svchost.exe terminated unexpectedly (PID 10608)", ColorRole::Error },
    { L"CPU",        L"WARNING",  L"Sustained CPU pressure detected around discord.exe", ColorRole::Warning },
    { L"CPU",        L"ERROR",    L"Core throttling triggered — temperature above Tjunction", ColorRole::Error },
    { L"CPU",        L"INFO",     L"Context switches stabilized at 14518/s", ColorRole::Kernel },
    { L"RAM",        L"WARNING",  L"Memory residency crossed threshold: chrome.exe 2.9GB", ColorRole::Warning },
    { L"RAM",        L"ERROR",    L"Memory allocation failure imminent on java.exe", ColorRole::Error },
    { L"RAM",        L"CRITICAL", L"Pagefile exhaustion — system may become unstable", ColorRole::Fatal },
    { L"NETWORK",    L"INFO",     L"Latency 15ms, 23 established / 17 listening sockets", ColorRole::Network },
    { L"NETWORK",    L"WARNING",  L"ProtonVPN.Client.exe upload spike detected (3.2MB/s)", ColorRole::Warning },
    { L"NETWORK",    L"ERROR",    L"Outbound socket fan-out crossed threshold (42 active)", ColorRole::Error },
    { L"NETWORK",    L"ERROR",    L"Latency crossed degraded threshold: 180ms average", ColorRole::Error },
    { L"GPU",        L"WARNING",  L"GPU utilization sustained above 90% for 30 seconds", ColorRole::Warning },
    { L"GPU",        L"ERROR",    L"DXGI swap chain reported device removed", ColorRole::Error },
    { L"GPU",        L"CRITICAL", L"VRAM allocation failure — driver reset required", ColorRole::Fatal },
    { L"DISK",       L"WARNING",  L"Storage temperature climbed above normal range", ColorRole::Warning },
    { L"DISK",       L"INFO",     L"Write throughput: 142 MB/s on volume C:", ColorRole::White },
    { L"DISK",       L"ERROR",    L"S.M.A.R.T. threshold exceeded: reallocated sector count", ColorRole::Error },
    { L"KERNEL",     L"INFO",     L"Queue 0 | switches 13429/s | IRQ 401/s", ColorRole::Kernel },
    { L"KERNEL",     L"INFO",     L"Thread scheduler latency: 1.2ms", ColorRole::Kernel },
    { L"KERNEL",     L"WARNING",  L"DPC latency spike: 4.8ms on core 3", ColorRole::Kernel },
    { L"SCRAM",      L"INFO",     L"Memory pressure is increasing", ColorRole::Scram },
    { L"SCRAM",      L"WARNING",  L"Heuristic cluster divergence in network module", ColorRole::Scram },
    { L"SCRAM",      L"ERROR",    L"Behavioral anomaly cluster detected — escalation pending", ColorRole::Fatal },
    { L"SCRAM",      L"CRITICAL", L"Anomalous process behavior detected — initiating triage", ColorRole::Fatal },
    { L"S.C.R.A.M",  L"CRITICAL", L"TEMPERATURE: INCREASING FAN SPEED (3% > 40%)", ColorRole::Fatal },
    { L"S.C.R.A.M",  L"WARNING",  L"Thermal envelope approaching critical margins", ColorRole::Scram },
    { L"ENGINE",     L"INFO",     L"UI render latency stable at 4ms", ColorRole::Success },
    { L"SYSTEM",     L"SUCCESS",  L"Collector synchronized with kernel counters", ColorRole::Success },
    { L"APPLICATION",L"ERROR",    L"render_service.exe crashed — Exception: 0xc0000005", ColorRole::Error },
    { L"APPLICATION",L"WARNING",  L"Plugin host unresponsive for 2400ms", ColorRole::Warning },
    { L"TASKS",      L"INFO",     L"Inspection focus moved to discord.exe (PID 15604)", ColorRole::White },
    { L"CONFIG",     L"INFO",     L"Settings are writable live — hot reload active", ColorRole::Dim },
    { L"SECURITY",   L"WARNING",  L"Failed login attempt from unknown session", ColorRole::Warning },
    { L"SECURITY",   L"ERROR",    L"Integrity check failed on config block", ColorRole::Error },
    { L"SECURITY",   L"CRITICAL", L"Unauthorized access attempt blocked", ColorRole::Fatal },
    { L"THERMAL",    L"WARNING",  L"CPU package temp: 92°C — fan curve adjusting", ColorRole::Warning },
    { L"THERMAL",    L"ERROR",    L"Thermal throttling active on all cores", ColorRole::Error },
    { L"AUTH",       L"INFO",     L"Session authenticated — user: admin", ColorRole::Success },
    { L"AUTH",       L"WARNING",  L"Lockout timer active — 2 attempts remaining", ColorRole::Warning },
    { L"TEST",       L"INFO",     L"Unit test suite passed: 142/142 assertions", ColorRole::Success },
    { L"TEST",       L"WARNING",  L"Integration test latency exceeded 500ms threshold", ColorRole::Warning },
    { L"TEST",       L"ERROR",    L"Regression detected: test_kernel_scheduler failed", ColorRole::Error },
    { L"TEST",       L"CRITICAL", L"Smoke test abort — critical path broken in module core", ColorRole::Fatal },
    { L"TEST",       L"INFO",     L"Benchmark: render loop 16.2ms avg (target <16.6ms)", ColorRole::Success },
    { L"TEST",       L"WARNING",  L"Memory leak detected in test_cleanup_teardown", ColorRole::Warning },
  };
  constexpr int kTotal = 20;
  constexpr int kEntryCount = sizeof(entries) / sizeof(entries[0]);
  for (int i = 0; i < kTotal; ++i) {
    const auto& e = entries[(GetTickCount64() + i * 7 + i * i * 3) % kEntryCount];
    const std::wstring tag = L" #" + std::to_wstring(i) + L" t" + std::to_wstring(GetTickCount64() % 10000);
    PushLog(e.domain, e.severity, e.message + tag, e.color, L"core", L"demo");
  }
  SetToast(L"Spawned " + std::to_wstring(kTotal) + L" demo entries — all types");
}
