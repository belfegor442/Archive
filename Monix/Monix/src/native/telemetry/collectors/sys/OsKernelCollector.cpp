#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winternl.h>
#include <psapi.h>

#include <cstdint>
#include <vector>

#include "OsKernelCollector.hpp"

namespace monix {

#ifndef _SYSTEM_HANDLE_INFORMATION_EX_DEFINED
#define _SYSTEM_HANDLE_INFORMATION_EX_DEFINED
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
  PVOID Object;
  ULONG_PTR UniqueProcessId;
  HANDLE HandleValue;
  ULONG GrantedAccess;
  USHORT CreatorBackTraceIndex;
  USHORT ObjectTypeIndex;
  ULONG HandleAttributes;
  ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
  ULONG_PTR NumberOfHandles;
  ULONG_PTR NumberOfObjects;
  SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_HANDLE_INFORMATION_EX;
#endif

#ifndef _SYSTEM_PERFORMANCE_INFORMATION_DEFINED
#define _SYSTEM_PERFORMANCE_INFORMATION_DEFINED
typedef struct _SYSTEM_PERFORMANCE_INFORMATION {
  LARGE_INTEGER IdleProcessTime;
  LARGE_INTEGER IosReadTransferCount;
  LARGE_INTEGER IosWriteTransferCount;
  LARGE_INTEGER IosOtherTransferCount;
  ULONG IosReadOperationCount;
  ULONG IosWriteOperationCount;
  ULONG IosOtherOperationCount;
  ULONG AvailablePages;
  ULONG CommittedPages;
  ULONG CommitLimit;
  ULONG PeakCommitment;
  ULONG PageFaultCount;
  ULONG CopyOnWriteCount;
  ULONG TransitionCount;
  ULONG CacheTransitionCount;
  ULONG DemandZeroCount;
  ULONG PageReadCount;
  ULONG PageReadIoCount;
  ULONG CacheReadCount;
  ULONG CacheIoCount;
  ULONG DirtyPagesWriteCount;
  ULONG DirtyWriteIoCount;
  ULONG MappedPagesWriteCount;
  ULONG MappedWriteIoCount;
  ULONG PagedPoolPages;
  ULONG NonPagedPoolPages;
  ULONG PagedPoolAllocs;
  ULONG PagedPoolFrees;
  ULONG NonPagedPoolAllocs;
  ULONG NonPagedPoolFrees;
  ULONG FreeSystemPtes;
  ULONG ResidentSystemCodePage;
  ULONG SharedSystemCodePage;
  ULONG ModificationPageNoReference;
  LARGE_INTEGER IoReadBytes;
  LARGE_INTEGER IoWriteBytes;
  LARGE_INTEGER IoOtherBytes;
  ULONG IoReadTransferGap;
  ULONG IoWriteTransferGap;
  ULONG IoOtherTransferGap;
} SYSTEM_PERFORMANCE_INFORMATION;
#endif

void CollectOsKernelData(Snapshot& snapshot) {
  FILETIME ft = {};
  GetSystemTimeAsFileTime(&ft);
  unsigned long long now100ns = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  snapshot.systemTime100ns = now100ns;
  snapshot.uptimeMs = GetTickCount64();
  snapshot.bootPhase = (snapshot.uptimeMs < 60000) ? 0 : (snapshot.uptimeMs < 300000) ? 1 : 2;

  DWORD sessionId = 0;
  if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
    snapshot.currentSessionId = static_cast<int>(sessionId);
  }

  DWORD bytesNeeded = 0;
  DWORD driverCount = 0;
  EnumDeviceDrivers(nullptr, 0, &bytesNeeded);
  if (bytesNeeded > 0) {
    std::vector<void*> bases(bytesNeeded / sizeof(void*));
    if (EnumDeviceDrivers(bases.data(), bytesNeeded, &bytesNeeded)) {
      driverCount = bytesNeeded / sizeof(void*);
      snapshot.driverCount = driverCount;
      for (DWORD i = 0; i < driverCount && i < bases.size(); ++i) {
        wchar_t nameBuf[MAX_PATH] = {};
        if (GetDeviceDriverBaseNameW(bases[i], nameBuf, MAX_PATH)) {
          snapshot.driverNames.insert(nameBuf);
        }
      }
    }
  }

  typedef NTSTATUS (NTAPI *NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
  auto pNtQSI = reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation"));
  if (pNtQSI) {
    ULONG retLen = 0;
    SYSTEM_HANDLE_INFORMATION_EX handleInfo {};
    NTSTATUS st = pNtQSI(64, &handleInfo, sizeof(handleInfo), &retLen);
    if (st == 0) {
      snapshot.totalHandles = handleInfo.NumberOfHandles;
      snapshot.totalObjects = handleInfo.NumberOfObjects;
    } else if (st == (NTSTATUS)0xC0000004L) {
      std::vector<BYTE> buf(retLen);
      st = pNtQSI(64, buf.data(), retLen, &retLen);
      if (st == 0) {
        auto pHdr = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buf.data());
        snapshot.totalHandles = pHdr->NumberOfHandles;
        snapshot.totalObjects = pHdr->NumberOfObjects;
      }
    }

    ULONG perfLen = 0;
    st = pNtQSI(3, nullptr, 0, &perfLen);
    if (st == (NTSTATUS)0xC0000004L) {
      std::vector<BYTE> perfBuf(perfLen);
      st = pNtQSI(3, perfBuf.data(), perfLen, &perfLen);
      if (st == 0) {
        auto pPerf = reinterpret_cast<SYSTEM_PERFORMANCE_INFORMATION*>(perfBuf.data());
        static ULONG prevPageFaultCount = 0;
        if (prevPageFaultCount > 0 && pPerf->PageFaultCount >= prevPageFaultCount) {
          snapshot.pageFaultsDelta = pPerf->PageFaultCount - prevPageFaultCount;
        } else {
          snapshot.pageFaultsDelta = 0;
        }
        prevPageFaultCount = pPerf->PageFaultCount;
        snapshot.ioReadBytesDelta = pPerf->IosReadTransferCount.QuadPart;
        snapshot.ioWriteBytesDelta = pPerf->IosWriteTransferCount.QuadPart;
      }
    }
  }
}

}
