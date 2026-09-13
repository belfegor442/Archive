#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <psapi.h>

#include <cstdint>
#include <vector>

#include "SchedulerCollector.hpp"

namespace monix {

typedef long (__stdcall *NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
static NtQuerySystemInformation_t GetNtQuerySystemInfo() {
  static NtQuerySystemInformation_t fn = nullptr;
  static bool resolved = false;
  if (!resolved) {
    resolved = true;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) fn = reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
  }
  return fn;
}

struct SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
  LARGE_INTEGER IdleTime;
  LARGE_INTEGER KernelTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER DpcTime;
  LARGE_INTEGER InterruptTime;
  ULONG ContextSwitches;
  ULONG InterruptCount;
};

#ifndef KPRIORITY
typedef LONG KPRIORITY;
#endif

struct MY_CLIENT_ID {
  HANDLE UniqueProcess;
  HANDLE UniqueThread;
};

struct MY_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
};

struct SYSTEM_THREAD_INFORMATION {
  LARGE_INTEGER KernelTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER CreateTime;
  ULONG WaitTime;
  PVOID StartAddress;
  MY_CLIENT_ID ClientId;
  LONG Priority;
  LONG BasePriority;
  ULONG ContextSwitches;
  ULONG ThreadState;
  ULONG WaitReason;
  ULONG _pad;
};

struct SYSTEM_PROCESS_INFORMATION {
  ULONG NextEntryOffset;
  ULONG NumberOfThreads;
  LARGE_INTEGER WorkingSetPrivateSize;
  ULONG HardFaultCount;
  ULONG NumberOfThreadsHighWatermark;
  ULONGLONG CycleTime;
  LARGE_INTEGER CreateTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER KernelTime;
  MY_UNICODE_STRING ImageName;
  KPRIORITY BasePriority;
  HANDLE UniqueProcessId;
  ULONG InheritedFromUniqueProcessId;
  ULONG HandleCount;
  ULONG SessionId;
  ULONG_PTR UniqueProcessKey;
  SIZE_T PeakVirtualSize;
  SIZE_T VirtualSize;
  ULONG PageFaultCount;
  SIZE_T PeakWorkingSetSize;
  SIZE_T WorkingSetSize;
  SIZE_T QuotaPeakPagedPoolUsage;
  SIZE_T QuotaPagedPoolUsage;
  SIZE_T QuotaPeakNonPagedPoolUsage;
  SIZE_T QuotaNonPagedPoolUsage;
  SIZE_T PagefileUsage;
  SIZE_T PeakPagefileUsage;
  SIZE_T PrivatePageCount;
  LARGE_INTEGER ReadOperationCount;
  LARGE_INTEGER WriteOperationCount;
  LARGE_INTEGER OtherOperationCount;
  LARGE_INTEGER ReadTransferCount;
  LARGE_INTEGER WriteTransferCount;
  LARGE_INTEGER OtherTransferCount;
};

void CollectSchedulerData(Snapshot& snapshot) {
  auto ntQuery = GetNtQuerySystemInfo();
  if (!ntQuery) return;

  ULONG procInfoSize = sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * 64;
  std::vector<BYTE> procBuf(procInfoSize);
  ULONG retLen = 0;
  long status = ntQuery(8, procBuf.data(), procInfoSize, &retLen);
  if (status >= 0) {
    auto* infos = reinterpret_cast<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION*>(procBuf.data());
    int numProcs = static_cast<int>(retLen / sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));
    snapshot.processorCount = numProcs;
    for (int i = 0; i < numProcs; ++i) {
      snapshot.totalContextSwitches += infos[i].ContextSwitches;
      snapshot.totalInterruptCount += infos[i].InterruptCount;
      snapshot.totalDpcCount += static_cast<std::uint64_t>(infos[i].DpcTime.QuadPart);
      snapshot.totalIsrCount += static_cast<std::uint64_t>(infos[i].InterruptTime.QuadPart);
    }
  }

  ULONG procListSize = 0;
  status = ntQuery(5, nullptr, 0, &procListSize);
  if (procListSize == 0) return;
  procListSize += 8192;
  std::vector<BYTE> procListBuf(procListSize);
  status = ntQuery(5, procListBuf.data(), procListSize, &retLen);
  if (status < 0) return;

  BYTE* basePtr = procListBuf.data();
  BYTE* endPtr = basePtr + retLen;
  int totalSuspended = 0, totalReady = 0, totalWaiting = 0;
  int rtCount = 0, hiCount = 0;

  BYTE* current = basePtr;
  while (current < endPtr) {
    auto* procInfo = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(current);
    if (procInfo->NumberOfThreads == 0 || procInfo->UniqueProcessId == 0 ||
        procInfo->UniqueProcessId == reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(4))) {
      if (procInfo->NextEntryOffset == 0) break;
      current += procInfo->NextEntryOffset;
      continue;
    }
    BYTE* threadBase = current + sizeof(SYSTEM_PROCESS_INFORMATION);
    for (ULONG t = 0; t < procInfo->NumberOfThreads; threadBase += sizeof(SYSTEM_THREAD_INFORMATION)) {
      if (threadBase + sizeof(SYSTEM_THREAD_INFORMATION) > endPtr) break;
      auto* thread = reinterpret_cast<SYSTEM_THREAD_INFORMATION*>(threadBase);
      switch (thread->ThreadState) {
        case 0: totalReady++; break;
        case 1: totalReady++; break;
        case 2: case 3: totalReady++; break;
        case 4: break;
        case 5: totalWaiting++; break;
        case 6: totalWaiting++; break;
        case 7: totalReady++; break;
        default: totalWaiting++; break;
      }
      if (thread->Priority >= 16) rtCount++;
      if (thread->Priority >= 12) hiCount++;
      t++;
    }
    if (procInfo->NextEntryOffset == 0) break;
    current += procInfo->NextEntryOffset;
  }

  snapshot.suspendedThreadCount = totalSuspended;
  snapshot.readyThreadCount = totalReady;
  snapshot.waitingThreadCount = totalWaiting;
  snapshot.realtimeThreadCount = rtCount;
  snapshot.highPriorityThreadCount = hiCount;
}

}
