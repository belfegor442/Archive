#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../core/Types.hpp"
#include "../settings/MonixConfigTypes.hpp"
#include "../logging/LogEntry.hpp"
#include "../telemetry/Snapshot.hpp"
#include "../telemetry/state/ProcessChangeDetector.hpp"
#include "../telemetry/state/UeoCorrelator.hpp"
#include "AppState.hpp"
#include "shaders/ShaderBrowserPanel.hpp"
#include "AppStateGroups.hpp"

struct AppState {
  // Navigation
  monix::Tab activeTab = monix::Tab::Log;
  int coreMonitorMenuIndex = 0;
  int currentFontIndex = 0;

  // Live telemetry snapshot (written by telemetry thread under stateMutex_)
  monix::Snapshot snapshot;
  std::unique_ptr<monix::Snapshot> previousSnapshot;
  bool hasPreviousSnapshot = false;

  // Grouped sub-states (replaces flat field duplication)
  monix::LogState logState;
  monix::ScramState scramState;
  monix::HistoryBuffers history;
  monix::NotificationState notifState;
  monix::ShaderBrowserUiState shaderUi;
  monix::ProcessLifecycleState processLifecycle;
  monix::NetworkFlowState networkFlow;
  monix::LatencyState latencyState;
  monix::ReadyQueueState queueState;
  monix::DirtyBitState dirtyBitState;
  monix::ClickDebugState clickDebug;
  monix::SessionInfo session;
  monix::MetricsWindow metricsWindow;
  monix::IncidentTracker incidentTracker;
  monix::telemetry::UeoCorrelator ueoCorrelator;
  std::uint64_t nextUeoIncidentId = 1;

  // Persistent process change detector (must survive across ConsumeSnapshot calls)
  monix::telemetry::ProcessChangeDetector processDetector;

  int taskScroll = 0;
  int selectedTaskIndex = 0;
  int selectedTaskPid = 0;
  int settingsCategory = 0;
  int pressedButton = -1;
  int hoveredMenuIndex = -1;

  // Presentation-only state (no grouped sub-struct needed)
  monix::IntroState intro;
  monix::ContextMenuState taskMenu;
  monix::UpdateState updateState;
  RECT viewport_ { 0, 0, 0, 0 };
  bool loggedIn = false;

  struct DevShellState {
    bool active = false;
    std::vector<std::wstring> history;
    std::wstring input;
    int scrollOffset = 0;
  } devShell;
};
