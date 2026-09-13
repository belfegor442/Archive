#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "ReliabilityCollector.hpp"

namespace monix {

std::atomic<int> g_sehExceptionCount{0};
std::atomic<int> g_unhandledExceptionCount{0};
std::atomic<int> g_accessViolationCount{0};
std::atomic<int> g_heapCorruptionDetected{0};
std::atomic<int> g_assertionFailureCount{0};
std::atomic<int> g_stackOverflowCount{0};

unsigned long HashProcessList() {
  unsigned long hash = 2166136261u;
  std::vector<std::pair<DWORD, std::wstring>> entries;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
      do {
        entries.emplace_back(pe.th32ProcessID, pe.szExeFile);
      } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
  }
  std::sort(entries.begin(), entries.end());
  for (const auto& [pid, name] : entries) {
    hash ^= pid;
    hash *= 16777619u;
    for (int i = 0; name[i]; ++i) {
      hash ^= static_cast<unsigned char>(name[i]);
      hash *= 16777619u;
    }
  }
  return hash;
}

unsigned long HashModuleList(DWORD processId) {
  unsigned long hash = 2166136261u;
  std::vector<std::pair<std::wstring, uintptr_t>> entries;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
  if (snap != INVALID_HANDLE_VALUE) {
    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
      do {
        entries.emplace_back(me.szModule, reinterpret_cast<uintptr_t>(me.modBaseAddr));
      } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
  }
  std::sort(entries.begin(), entries.end());
  for (const auto& [name, baseAddr] : entries) {
    for (int i = 0; name[i]; ++i) {
      hash ^= static_cast<unsigned char>(name[i]);
      hash *= 16777619u;
    }
    hash ^= baseAddr;
    hash *= 16777619u;
  }
  return hash;
}

void CollectReliabilityData(Snapshot& snapshot) {
  snapshot.sehExceptionCount = g_sehExceptionCount.load(std::memory_order_relaxed);
  snapshot.unhandledExceptionCount = g_unhandledExceptionCount.load(std::memory_order_relaxed);
  snapshot.accessViolationCount = g_accessViolationCount.load(std::memory_order_relaxed);
  snapshot.heapCorruptionDetected = g_heapCorruptionDetected.load(std::memory_order_relaxed);
  snapshot.assertionFailureCount = g_assertionFailureCount.load(std::memory_order_relaxed);
  snapshot.stackOverflowCount = g_stackOverflowCount.load(std::memory_order_relaxed);

  snapshot.processHash = HashProcessList();
  snapshot.moduleHash = HashModuleList(GetCurrentProcessId());

  snapshot.safeModeActive = 0;
  UINT bootStatus = GetSystemMetrics(SM_CLEANBOOT);
  if (bootStatus != 0) {
    snapshot.safeModeActive = 1;
  }

  snapshot.uiResponsivenessMs = 0;
  snapshot.mainThreadResponsive = 1;

  snapshot.threadHealthOk = 1;
  HANDLE threadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (threadSnap != INVALID_HANDLE_VALUE) {
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    DWORD selfTid = GetCurrentThreadId();
    if (Thread32First(threadSnap, &te)) {
      int checked = 0;
      do {
        if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != selfTid) {
          HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
          if (hThread) {
            DWORD exitCode = 0;
            if (GetExitCodeThread(hThread, &exitCode) && exitCode != STILL_ACTIVE) {
              snapshot.threadHealthOk = 0;
            }
            CloseHandle(hThread);
          }
          ++checked;
          if (checked >= 16) break;
        }
      } while (Thread32Next(threadSnap, &te));
    }
    CloseHandle(threadSnap);
  }

  snapshot.telemetryDropDetected = 0;
  snapshot.crashEventsToday = 0;
  snapshot.exceptionLogCount = 0;

  wchar_t sysDir[MAX_PATH];
  GetSystemDirectoryW(sysDir, MAX_PATH);
  WIN32_FIND_DATAW fd;
  std::wstring minidumpDir = std::wstring(sysDir) + L"\\Minidump";
  HANDLE hFind = FindFirstFileW((minidumpDir + L"\\*.dmp").c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    do {
      SYSTEMTIME ft;
      FileTimeToSystemTime(&fd.ftCreationTime, &ft);
      if (ft.wYear == st.wYear && ft.wMonth == st.wMonth && ft.wDay == st.wDay) {
        ++snapshot.crashEventsToday;
      }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
  }

  wchar_t werDir[MAX_PATH];
  if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, werDir) == S_OK) {
    std::wstring crashDir = std::wstring(werDir) + L"\\CrashDumps";
    hFind = FindFirstFileW((crashDir + L"\\*.*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      SYSTEMTIME st;
      GetLocalTime(&st);
      do {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
          SYSTEMTIME ft;
          FileTimeToSystemTime(&fd.ftCreationTime, &ft);
          if (ft.wYear == st.wYear && ft.wMonth == st.wMonth && ft.wDay == st.wDay) {
            ++snapshot.crashEventsToday;
          }
        }
      } while (FindNextFileW(hFind, &fd));
      FindClose(hFind);
    }
  }
}

}
