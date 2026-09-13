#include "TelemetryThread.hpp"

#include "../../MonixApp.hpp"
#include "../../TelemetryInternal.hpp"
#include "../../app/bootstrap/AppConstants.hpp"
#include "../../crash/CrashHandling.hpp"

#include <windows.h>
#include <process.h>
#include <exception>

using namespace monix;
using namespace monix::internal;

unsigned __stdcall monix::runtime::TelemetryThreadProc(void* param) {
  auto* app = static_cast<MonixApp*>(param);
  app->TelemetryLoop();
  return 0;
}

void MonixApp::StartTelemetry() {
  running_ = true;
  telemetryHandle_ = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 8 * 1024 * 1024, monix::runtime::TelemetryThreadProc, this, 0, nullptr));
}

void MonixApp::StopTelemetry() {
  running_ = false;
  if (telemetryHandle_) {
    DWORD waitResult = WaitForSingleObject(telemetryHandle_, 10000);
    if (waitResult == WAIT_TIMEOUT) {
      HANDLE h = CreateFileA(internal::GetCrashLogPath().c_str(),
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, nullptr, FILE_END);
        const char msg[] = "WARNING: Telemetry thread did not exit within 10s timeout\n";
        DWORD written = 0;
        WriteFile(h, msg, sizeof(msg) - 1, &written, nullptr);
        CloseHandle(h);
      }
      for (int i = 0; i < 20 && WaitForSingleObject(telemetryHandle_, 500) == WAIT_TIMEOUT; ++i) {}
    }
    CloseHandle(telemetryHandle_);
    telemetryHandle_ = nullptr;
  }
  hwnd_.store(nullptr);
}

void MonixApp::RequestRefresh() {
  refreshRequested_ = true;
}

void MonixApp::TelemetryLoop() {
  _set_se_translator(internal::SehTranslator);
  g_phase = "TEL:THREAD_START";
  g_telTid.store(GetCurrentThreadId());
  { HANDLE h = CreateFileA(internal::GetCrashLogPath().c_str(),
    GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      char buf[128]; int len = snprintf(buf, sizeof(buf), "TEL_TID=%lu\n", (unsigned long)GetCurrentThreadId());
      DWORD written = 0; WriteFile(h, buf, len, &written, nullptr); CloseHandle(h);
    }
  }
  while (running_) {
    Snapshot snapshot;
    bool pollOk = false;
    try {
      g_phase = "POLL:ENTER";
      snapshot = PollSnapshot();
      pollOk = true;
    } catch (const std::exception& ex) {
      internal::LogSehToCrashLog("PollSnapshot", 0, ex.what());
      Sleep(200);
      continue;
    } catch (...) {
      internal::LogSehToCrashLog("PollSnapshot", 0, "UNKNOWN");
      Sleep(200);
      continue;
    }
    if (pollOk && (!snapshot.processes.empty() || snapshot.ramTotalBytes != 0)) {
      try {
        g_phase = "CONSUME:ENTER";
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        ConsumeSnapshot(std::move(snapshot));
      } catch (const std::exception& ex) {
        internal::LogSehToCrashLog("ConsumeSnapshot", 0, ex.what());
      } catch (...) {
        internal::LogSehToCrashLog("ConsumeSnapshot", 0, "UNKNOWN");
      }

      HWND curHwnd = hwnd_.load();
      if (curHwnd) {
        PostMessageW(curHwnd, WM_MONIX_UPDATE, 0, 0);
      }
    }

    UINT delay = 1000;
    {
      UINT baseDelay = std::max<UINT>(GetConfig().telemetryIntervalMs, 75u);
      double risk = 0.0;
      {
        std::lock_guard<std::recursive_mutex> lock(stateMutex_);
        risk = state_.scramState.smoothedRisk;
      }
      if (risk >= 70.0) {
        delay = baseDelay * 5;
      } else if (risk >= 50.0) {
        delay = baseDelay * 3;
      } else if (risk >= 30.0) {
        delay = baseDelay * 2;
      } else {
        delay = baseDelay;
      }
    }

    for (UINT elapsed = 0; elapsed < delay && running_; elapsed += 10) {
      if (refreshRequested_.exchange(false)) {
        break;
      }
      Sleep(10);
    }
  }
  g_phase = "TEL:THREAD_EXIT";
}
