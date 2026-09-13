#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <string>

namespace monix::internal {

inline std::atomic<const char*> g_phase{"INIT"};
inline std::atomic<DWORD> g_telTid{0};

inline std::string GetCrashLogPath() {
  char tempPath[MAX_PATH];
  if (GetTempPathA(MAX_PATH, tempPath)) {
    return std::string(tempPath) + "monix_crash.txt";
  }
  return "C:\\Windows\\Temp\\monix_crash.txt";
}

}  // namespace monix::internal
