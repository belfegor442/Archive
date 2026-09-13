#include "ReferenceSeeder.hpp"

#include "../../MonixApp.hpp"

using namespace monix;

void MonixApp::SeedReferenceState() {
  state_.snapshot.host = L"MONIX";
  state_.snapshot.cpuCores = 6;
  state_.snapshot.cpuLogicalCpus = 12;
  state_.snapshot.cpuHtEnabled = 1;
  state_.snapshot.processorCount = 12;
  state_.snapshot.uptimeSeconds = 0;

  state_.intro.active = config_.introEnabled;
  if (testMode_) { state_.loggedIn = true; state_.intro.active = false; }
  state_.scramState.trackedPid = 0;
  PushLog(L"SYSTEM", L"SUCCESS", L"Runtime shell initialized with live tabs and full-screen log viewport.", ColorRole::Success);
  PushLog(L"ENGINE", L"INFO", L"VHS Gothic font loaded. Renderer: Vulkan + OpenGL GLSL preset passthrough.", ColorRole::Primary);
  PushLog(L"CONFIG", L"INFO", L"Settings are writable live from the panel and persisted to monix.ini.", ColorRole::Success);
  PushLog(L"NETWORK", L"INFO", L"Socket flow monitor attached to the native collector.", ColorRole::Network);
  PushLog(L"SCRAM", L"INFO", L"Passive analysis engine prepared for hardware and kernel depth.", ColorRole::Scram);
  AppendHistoryPoint(state_.snapshot);
  {
    state_.scramState.headline = L"SCRAM calibrating baseline\u2026";
    state_.scramState.insight = L"Waiting for telemetry samples before evaluating risk.";
    state_.scramState.riskScore = 0;
    state_.scramState.smoothedRisk = 0.0;
    state_.scramState.prevRisk = 0.0;
    state_.scramState.currentSeverity = 0;
  }
}
