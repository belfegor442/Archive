#include "ui/render/RenderDispatcher.hpp"
#include "ui/render/primitives/RenderPrimitives.hpp"
#include "ui/render/overlays/RenderOverlay.hpp"
#include "ui/render/RenderCoreMonitor.hpp"
#include "ui/render/panels/log/RenderLog.hpp"
#include "ui/render/panels/tasks/RenderTasks.hpp"
#include "ui/render/panels/hardware/RenderHardware.hpp"
#include "ui/render/panels/network/RenderNetwork.hpp"
#include "ui/render/panels/scram/RenderScram.hpp"
#include "ui/render/panels/settings/RenderSettings.hpp"

void MonixApp::Render(HDC dc, const RECT& clientRect) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex_);
  DrawBackground(dc, clientRect);

  if (!state_.loggedIn) {
    kernel_.Update();
    if (!kernel_.IsRunning()) {
      kernelDisplay_.Draw(dc, clientRect, kernel_, bodyFont_, smallFont_, dpiScale_);
      DrawToast(dc, clientRect);
      DrawClickDebug(dc, clientRect);
      return;
    }
    TransitionToLoggedIn();
  }

  if (state_.intro.active) {
    DrawIntro(dc, clientRect);
    return;
  }

  if (IsCoreMonitorThemeActive()) {
    RenderCoreMonitorTheme(dc, clientRect);
    DrawToast(dc, clientRect);
    DrawClickDebug(dc, clientRect);
    return;
  }

  DrawTabs(dc, clientRect);

  switch (state_.activeTab) {
    case Tab::Log: DrawLogView(dc, clientRect); break;
    case Tab::Tasks: DrawTasksView(dc, clientRect); break;
    case Tab::Hardware: DrawHardwareView(dc, clientRect); break;
    case Tab::Network: DrawNetworkView(dc, clientRect); break;
    case Tab::Scram: DrawScramView(dc, clientRect); break;
    case Tab::Settings: DrawSettingsView(dc, clientRect); break;
  }

  if (state_.taskMenu.visible) {
    DrawTaskContextMenu(dc);
  }

  DrawFooter(dc, clientRect);
  DrawToast(dc, clientRect);
  DrawClickDebug(dc, clientRect);
  DrawNotifications(dc, clientRect);
  DrawDevShell(dc, clientRect);
}
