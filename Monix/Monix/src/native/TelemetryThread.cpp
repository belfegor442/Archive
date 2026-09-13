#include "MonixApp.hpp"

#include "crash/CrashHandling.hpp"
#include "telemetry/runtime/TelemetryThread.hpp"
#include "telemetry/runtime/ProcessCapture.hpp"
#include "telemetry/snapshot/SnapshotPoller.hpp"
#include "telemetry/snapshot/SnapshotConsumer.hpp"
#include "telemetry/snapshot/ReferenceSeeder.hpp"
#include "logging/LogSink.hpp"
#include "logging/sound/SoundEffects.hpp"
#include "logging/notify/Notifications.hpp"
#include "logging/export/LogExport.hpp"
#include "logging/history/HistoryAppend.hpp"
#include "app/lifecycle/DemoLogs.hpp"

#include <windows.h>
#include <exception>

using namespace monix;

LRESULT CALLBACK MonixApp::StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    auto* self = static_cast<MonixApp*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }

  auto* self = reinterpret_cast<MonixApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (self) {
    return self->WndProc(hwnd, message, wParam, lParam);
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}
