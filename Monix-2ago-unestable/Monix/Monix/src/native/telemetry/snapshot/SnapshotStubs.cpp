#include "../../MonixApp.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <map>
#include <mutex>

static double g_prevCpuKernel = 0;
static double g_prevCpuUser = 0;
static double g_prevCpuIdle = 0;

struct ProcTimeEntry {
  FILETIME prevCreation;
  FILETIME prevExit;
  FILETIME prevKernel;
  FILETIME prevUser;
  double lastCpuPct = 0.0;
};
static std::map<DWORD, ProcTimeEntry> g_procTimes;
static std::mutex g_procTimesMutex;

static double FtToMs(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return static_cast<double>(u.QuadPart) / 10000.0;
}

static double FtDiffMs(const FILETIME& a, const FILETIME& b) {
  return FtToMs(a) - FtToMs(b);
}

static std::wstring PriorityClassToWStr(DWORD pc) {
  if (pc == ABOVE_NORMAL_PRIORITY_CLASS) return L"Above Normal";
  if (pc == BELOW_NORMAL_PRIORITY_CLASS) return L"Below Normal";
  if (pc == HIGH_PRIORITY_CLASS) return L"High";
  if (pc == IDLE_PRIORITY_CLASS) return L"Idle";
  if (pc == NORMAL_PRIORITY_CLASS) return L"Normal";
  if (pc == REALTIME_PRIORITY_CLASS) return L"Realtime";
  if (pc == PROCESS_MODE_BACKGROUND_BEGIN) return L"Background";
  return L"Normal";
}

Snapshot MonixApp::PollSnapshot() {
  Snapshot s;

  FILETIME idleFt, kernelFt, userFt;
  if (GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
    double idle = FtToMs(idleFt);
    double kernel = FtToMs(kernelFt);
    double user = FtToMs(userFt);
    double dIdle = idle - g_prevCpuIdle;
    double dKernel = kernel - g_prevCpuKernel;
    double dUser = user - g_prevCpuUser;
    g_prevCpuIdle = idle;
    g_prevCpuKernel = kernel;
    g_prevCpuUser = user;
    double total = dKernel + dUser;
    if (total > 0) {
      s.cpuPct = (1.0 - dIdle / total) * 100.0;
      if (s.cpuPct < 0) s.cpuPct = 0;
      if (s.cpuPct > 100) s.cpuPct = 100;
      s.kernelTimePct = dKernel / total * 100.0;
      s.userTimePct = dUser / total * 100.0;
    }
  }

  MEMORYSTATUSEX mem{};
  mem.dwLength = sizeof(mem);
  if (GlobalMemoryStatusEx(&mem)) {
    s.ramTotalBytes = mem.ullTotalPhys;
    s.ramUsedBytes = mem.ullTotalPhys - mem.ullAvailPhys;
    s.ramAvailBytes = mem.ullAvailPhys;
    s.commitUsedBytes = mem.ullTotalPageFile - mem.ullAvailPageFile;
    s.commitLimitBytes = mem.ullTotalPageFile;
  }

  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  s.cpuLogicalCpus = si.dwNumberOfProcessors;

  s.bootPhase = 3;
  s.sessionCount = 1;
  s.currentSessionId = 1;

  int totalThreads = 0;
  int totalProcesses = 0;

  double wallMs = 0;
  {
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    static FILETIME g_prevWall = nowFt;
    wallMs = FtDiffMs(nowFt, g_prevWall);
    g_prevWall = nowFt;
    if (wallMs < 1.0) wallMs = 1000.0;
  }

  std::map<DWORD, ProcTimeEntry> newProcTimes;

  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
      do {
        ProcessInfo pi;
        pi.pid = static_cast<int>(pe.th32ProcessID);
        pi.name = pe.szExeFile;
        pi.parentPid = static_cast<int>(pe.th32ParentProcessID);
        totalThreads += pe.cntThreads;

        HANDLE hP = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
        if (hP) {
          PROCESS_MEMORY_COUNTERS pmc{};
          pmc.cb = sizeof(pmc);
          if (GetProcessMemoryInfo(hP, &pmc, sizeof(pmc))) {
            pi.ramBytes = pmc.WorkingSetSize;
          }

          DWORD pc = GetPriorityClass(hP);
          pi.priority = PriorityClassToWStr(pc);

          FILETIME creationFt, exitFt, kernelFt2, userFt2;
          if (GetProcessTimes(hP, &creationFt, &exitFt, &kernelFt2, &userFt2)) {
            bool isRunning = (exitFt.dwLowDateTime == 0 && exitFt.dwHighDateTime == 0);
            pi.status = isRunning ? L"Running" : L"Terminated";

            DWORD pid = pe.th32ProcessID;
            auto it = g_procTimes.find(pid);
            if (it != g_procTimes.end()) {
              double dKernel = FtDiffMs(kernelFt2, it->second.prevKernel);
              double dUser = FtDiffMs(userFt2, it->second.prevUser);
              double cpuTime = dKernel + dUser;
              pi.cpuPct = (cpuTime / (wallMs * static_cast<double>(si.dwNumberOfProcessors))) * 100.0;
              if (pi.cpuPct < 0) pi.cpuPct = 0;
              if (pi.cpuPct > 100) pi.cpuPct = 100;
            } else {
              pi.cpuPct = 0.0;
            }

            ProcTimeEntry entry;
            entry.prevCreation = creationFt;
            entry.prevExit = exitFt;
            entry.prevKernel = kernelFt2;
            entry.prevUser = userFt2;
            entry.lastCpuPct = pi.cpuPct;
            newProcTimes[pid] = entry;
          }

          CloseHandle(hP);
        } else {
          pi.priority = L"Normal";
          pi.status = L"";
        }

        s.processes.push_back(std::move(pi));
        ++totalProcesses;
      } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
  }

  {
    std::lock_guard<std::mutex> lock(g_procTimesMutex);
    g_procTimes = std::move(newProcTimes);
  }

  s.processCount = totalProcesses;
  s.threadCount = totalThreads;

  s.processorQueueLength = -1;
  s.contextSwitchesPerSec = -1;
  s.interruptsPerSec = -1;
  s.systemCallsPerSec = -1;

  s.displayWidth = GetSystemMetrics(SM_CXSCREEN);
  s.displayHeight = GetSystemMetrics(SM_CYSCREEN);

  {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"Hardware\\Description\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      wchar_t procName[256] = {};
      DWORD nameLen = sizeof(procName);
      if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr,
          reinterpret_cast<LPBYTE>(procName), &nameLen) == ERROR_SUCCESS) {
        size_t i = 0;
        while (procName[i] && i < 48) { s.cpuBrand[i] = static_cast<char>(procName[i]); ++i; }
        s.cpuBrand[i] = '\0';
      }
      RegCloseKey(hKey);
    }
  }

  return s;
}
