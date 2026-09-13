#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iphlpapi.h>
#include <icmpapi.h>
#include <iptypes.h>
#include <netioapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <powrprof.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <wtsapi32.h>
#include <shlobj.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "Collectors.hpp"
#include "../telemetry/Snapshot.hpp"
#include "../core/TextUtils.hpp"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace monix {

std::atomic<int> g_sehExceptionCount{0};
std::atomic<int> g_unhandledExceptionCount{0};
std::atomic<int> g_accessViolationCount{0};
std::atomic<int> g_heapCorruptionDetected{0};
std::atomic<int> g_assertionFailureCount{0};
std::atomic<int> g_stackOverflowCount{0};
int IcmpPingGateway() {
  HANDLE hIcmp = IcmpCreateFile();
  if (hIcmp == INVALID_HANDLE_VALUE) return -1;
  ULONG aaSize = 0;
  GetAdaptersAddresses(AF_INET, 0, nullptr, nullptr, &aaSize);
  if (aaSize == 0) { IcmpCloseHandle(hIcmp); return -1; }
  std::vector<BYTE> aaBuf(aaSize);
  auto* aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aaBuf.data());
  if (GetAdaptersAddresses(AF_INET, 0, nullptr, aa, &aaSize) != NO_ERROR) { IcmpCloseHandle(hIcmp); return -1; }
  for (auto* a = aa; a; a = a->Next) {
    if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    for (auto* ga = a->FirstGatewayAddress; ga; ga = ga->Next) {
      if (ga->Address.lpSockaddr->sa_family != AF_INET) continue;
      auto* sa = reinterpret_cast<sockaddr_in*>(ga->Address.lpSockaddr);
      if (sa->sin_addr.S_un.S_addr == 0) continue;
      char replyBuf[512] {};
      DWORD result = IcmpSendEcho(hIcmp, sa->sin_addr.S_un.S_addr, nullptr, 0, nullptr, replyBuf, sizeof(replyBuf), 2000);
      IcmpCloseHandle(hIcmp);
      if (result > 0) {
        auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
        return static_cast<int>(reply->RoundTripTime);
      }
      return -1;
    }
  }
  IcmpCloseHandle(hIcmp);
  return -1;
}

int ProbeDnsResolution() {
  ADDRINFO hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ADDRINFO* result = nullptr;
  auto start = std::chrono::steady_clock::now();
  int rc = getaddrinfo("google.com", nullptr, &hints, &result);
  auto elapsed = std::chrono::steady_clock::now() - start;
  int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
  if (rc != 0 || !result) {
    if (result) freeaddrinfo(result);
    return -1;
  }
  freeaddrinfo(result);
  return ms;
}

uint64_t ComputeRouteTableHash() {
  ULONG tableSize = 0;
  GetIpForwardTable(nullptr, &tableSize, FALSE);
  if (tableSize == 0) return 0;
  std::vector<BYTE> buf(tableSize);
  auto* table = reinterpret_cast<PMIB_IPFORWARDTABLE>(buf.data());
  if (GetIpForwardTable(table, &tableSize, FALSE) != NO_ERROR) return 0;
  uint64_t hash = 0x12345678;
  for (DWORD i = 0; i < table->dwNumEntries; ++i) {
    const auto& entry = table->table[i];
    hash ^= static_cast<uint64_t>(entry.dwForwardDest) << (i % 5);
    hash ^= static_cast<uint64_t>(entry.dwForwardMask) << ((i + 2) % 7);
    hash ^= static_cast<uint64_t>(entry.dwForwardNextHop) << ((i + 3) % 3);
    hash = (hash << 7) | (hash >> 57);
  }
  return hash;
}

typedef long (__stdcall *NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
NtQuerySystemInformation_t GetNtQuerySystemInfo() {
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
      snapshot.totalDpcCount += static_cast<uint64_t>(infos[i].DpcTime.QuadPart);
      snapshot.totalIsrCount += static_cast<uint64_t>(infos[i].InterruptTime.QuadPart);
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
    for (ULONG t = 0; t < procInfo->NumberOfThreads; t++, threadBase += sizeof(SYSTEM_THREAD_INFORMATION)) {
      if (threadBase + sizeof(SYSTEM_THREAD_INFORMATION) > endPtr) break;
      auto* thread = reinterpret_cast<SYSTEM_THREAD_INFORMATION*>(threadBase);
      switch (thread->ThreadState) {
        case 0: totalSuspended++; break;
        case 1: totalReady++; break;
        case 2: case 3: case 4: totalSuspended++; break;
        case 5: totalWaiting++; break;
        default: totalWaiting++; break;
      }
      if (thread->Priority >= 16) rtCount++;
      if (thread->Priority >= 12) hiCount++;
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

void CollectOsKernelData(Snapshot& snapshot) {
  FILETIME ft = {};
  GetSystemTimeAsFileTime(&ft);
  unsigned long long now100ns = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  snapshot.systemTime100ns = now100ns;
  snapshot.uptimeMs = GetTickCount64();
  snapshot.bootPhase = (snapshot.uptimeMs < 60000) ? 0 : (snapshot.uptimeMs < 300000) ? 1 : 2;
  snapshot.sessionCount = 0;
  snapshot.currentSessionId = 0;

  DWORD sessionId = 0;
  if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
    snapshot.currentSessionId = static_cast<int>(sessionId);
  }

  PWTS_SESSION_INFOA sessions = nullptr;
  DWORD sessionCount = 0;
  if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &sessionCount)) {
    snapshot.sessionCount = static_cast<int>(sessionCount);
    WTSFreeMemory(sessions);
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
        snapshot.pageFaultsDelta = pPerf->PageFaultCount;
        snapshot.ioReadBytesDelta = pPerf->IosReadTransferCount.QuadPart;
        snapshot.ioWriteBytesDelta = pPerf->IosWriteTransferCount.QuadPart;
      }
    }
  }
}

void CollectSecurityData(Snapshot& snapshot) {
  wchar_t exePath[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  snapshot.selfExePath = exePath;

  WINTRUST_FILE_INFO fileInfo = {};
  fileInfo.cbStruct = sizeof(fileInfo);
  fileInfo.pcwszFilePath = exePath;
  fileInfo.hFile = nullptr;
  fileInfo.pgKnownSubject = nullptr;

  GUID actionId = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  WINTRUST_DATA wintrustData = {};
  wintrustData.cbStruct = sizeof(wintrustData);
  wintrustData.pPolicyCallbackData = nullptr;
  wintrustData.pSIPClientData = nullptr;
  wintrustData.dwUIChoice = WTD_UI_NONE;
  wintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrustData.dwUnionChoice = WTD_CHOICE_FILE;
  wintrustData.dwStateAction = WTD_STATEACTION_VERIFY;
  wintrustData.hWVTStateData = nullptr;
  wintrustData.pwszURLReference = nullptr;
  wintrustData.dwProvFlags = WTD_SAFER_FLAG;
  wintrustData.dwUIContext = 0;
  wintrustData.pFile = &fileInfo;

  LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &wintrustData);
  wintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &wintrustData);

  snapshot.selfSignatureValid = (status == ERROR_SUCCESS) ? 1 : 0;

  HANDLE hFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile != INVALID_HANDLE_VALUE) {
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize != INVALID_FILE_SIZE && fileSize < 64 * 1024 * 1024) {
      std::vector<BYTE> fileData(fileSize);
      DWORD bytesRead = 0;
      if (ReadFile(hFile, fileData.data(), fileSize, &bytesRead, nullptr)) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        if (CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
          if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (CryptHashData(hHash, fileData.data(), bytesRead, 0)) {
              snapshot.selfHashVerified = 1;
            }
            CryptDestroyHash(hHash);
          }
          CryptReleaseContext(hProv, 0);
        }
      }
    }
    CloseHandle(hFile);
  }

  snapshot.unsignedDriverCount = 0;
  snapshot.unsignedDriverNames.clear();

  DWORD bytesNeeded = 0;
  EnumDeviceDrivers(nullptr, 0, &bytesNeeded);
  if (bytesNeeded > 0) {
    std::vector<void*> bases(bytesNeeded / sizeof(void*));
    if (EnumDeviceDrivers(bases.data(), bytesNeeded, &bytesNeeded)) {
      DWORD driverCount = bytesNeeded / sizeof(void*);
      for (DWORD i = 0; i < driverCount && i < bases.size(); ++i) {
        wchar_t nameBuf[MAX_PATH] = {};
        if (GetDeviceDriverBaseNameW(bases[i], nameBuf, MAX_PATH)) {
          wchar_t fullPath[MAX_PATH] = {};
          swprintf_s(fullPath, L"\\SystemRoot\\System32\\drivers\\%s", nameBuf);
          WINTRUST_FILE_INFO drvInfo = {};
          drvInfo.cbStruct = sizeof(drvInfo);
          drvInfo.pcwszFilePath = fullPath;
          drvInfo.hFile = nullptr;
          drvInfo.pgKnownSubject = nullptr;

          WINTRUST_DATA drvWtData = {};
          drvWtData.cbStruct = sizeof(drvWtData);
          drvWtData.dwUIChoice = WTD_UI_NONE;
          drvWtData.fdwRevocationChecks = WTD_REVOKE_NONE;
          drvWtData.dwUnionChoice = WTD_CHOICE_FILE;
          drvWtData.dwStateAction = WTD_STATEACTION_VERIFY;
          drvWtData.hWVTStateData = nullptr;
          drvWtData.pFile = &drvInfo;

          LONG drvStatus = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &drvWtData);
          drvWtData.dwStateAction = WTD_STATEACTION_CLOSE;
          WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &drvWtData);

          if (drvStatus != ERROR_SUCCESS) {
            snapshot.unsignedDriverCount++;
            snapshot.unsignedDriverNames.insert(nameBuf);
          }
        }
      }
    }
  }

  snapshot.suspiciousScriptHosts = 0;
  snapshot.suspiciousModules.clear();
  snapshot.lsassAccessCount = 0;
  snapshot.debugPortActive = IsDebuggerPresent() ? 1 : 0;

  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
      do {
        std::wstring procName(pe.szExeFile);
        std::transform(procName.begin(), procName.end(), procName.begin(), ::towlower);
        if (procName == L"wscript.exe" || procName == L"cscript.exe" || procName == L"mshta.exe" || procName == L"powershell.exe") {
          snapshot.suspiciousScriptHosts++;
        }
        if (procName == L"consent.exe") {
          snapshot.uacConsentProcesses++;
        }
        if (procName == L"lsass.exe") {
          snapshot.lsassAccessCount++;
        }
      } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
  }

  snapshot.vmIndicators = 0;
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\VBoxGuest", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    snapshot.vmIndicators++;
    RegCloseKey(hKey);
  }
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vmci", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    snapshot.vmIndicators++;
    RegCloseKey(hKey);
  }
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vmhgfs", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    snapshot.vmIndicators++;
    RegCloseKey(hKey);
  }

  snapshot.hookModulesDetected = 0;
  snapshot.peHeaderTamper = 0;
  snapshot.processPaths.clear();

  hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (hSnap != INVALID_HANDLE_VALUE) {
    MODULEENTRY32W me = {};
    me.dwSize = sizeof(me);
    if (Module32FirstW(hSnap, &me)) {
      do {
        std::wstring modPath(me.szExePath);
        snapshot.processPaths.insert(modPath);

        wchar_t modNameLower[MAX_PATH] = {};
        wcscpy_s(modNameLower, me.szModule);
        for (wchar_t* p = modNameLower; *p; ++p) *p = towlower(*p);
        std::wstring modName(modNameLower);

        if (modName == L"monix.exe") {
          continue;
        }

        if (modName.find(L"hook") != std::wstring::npos || modName.find(L"inject") != std::wstring::npos || modName.find(L"detour") != std::wstring::npos) {
          snapshot.hookModulesDetected++;
          snapshot.suspiciousModules.insert(me.szModule);
        }

        static const std::set<std::wstring> kKnownSafeModules = {
          L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"advapi32.dll",
          L"user32.dll", L"gdi32.dll", L"gdi32full.dll", L"msvcrt.dll",
          L"sechost.dll", L"rpcrt4.dll", L"ucrtbase.dll", L"combase.dll",
          L"ole32.dll", L"oleaut32.dll", L"shell32.dll", L"shlwapi.dll",
          L"comctl32.dll", L"comdlg32.dll", L"ws2_32.dll", L"iphlpapi.dll",
          L"winmm.dll", L"wininet.dll", L"winhttp.dll", L"crypt32.dll",
          L"bcrypt.dll", L"bcryptprimitives.dll", L"ncrypt.dll", L"wintrust.dll",
          L"imagehlp.dll", L"setupapi.dll", L"cfgmgr32.dll", L"devobj.dll",
          L"powrprof.dll", L"ntdll.dll", L"clbcatq.dll", L"comsvcs.dll",
          L"version.dll", L"profapi.dll", L"cryptsp.dll", L"wldp.dll",
          L"msasn1.dll", L"cryptdlg.dll", L"imm32.dll", L"msctf.dll",
          L"win32u.dll", L"dxgi.dll", L"d3d11.dll", L"dwmapi.dll",
          L"opengl32.dll", L"dxcore.dll", L"msi.dll", L"schannel.dll",
          L"winnsi.dll", L"dpapi.dll", L"wtsapi32.dll", L"iphlpapi.dll",
          L"userenv.dll", L"bcp47mrm.dll", L"twinapi.appcore.dll",
          L"rmclient.dll", L"threadpoolwinrt.dll", L"propsys.dll",
          L"Windows.Storage.ApplicationData.dll", L"MMDevAPI.dll",
          L"AudioSes.dll", L"wintypes.dll", L"ExecModelClient.dll",
          L"resourcepolicyclient.dll", L"SmartContentProtection.dll",
        };

        if (me.modBaseAddr && me.modBaseSize > 0) {
          PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(me.modBaseAddr);
          if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
            PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
              reinterpret_cast<BYTE*>(me.modBaseAddr) + dosHeader->e_lfanew);
            if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
              DWORD headerSize = ntHeaders->OptionalHeader.SizeOfHeaders;
              if (headerSize < me.modBaseSize && kKnownSafeModules.find(modName) == kKnownSafeModules.end()) {
                HANDLE hModFile = CreateFileW(me.szExePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                if (hModFile != INVALID_HANDLE_VALUE) {
                  DWORD modFileSize = GetFileSize(hModFile, nullptr);
                  if (modFileSize != INVALID_FILE_SIZE && modFileSize >= headerSize) {
                    std::vector<BYTE> diskHeader(headerSize);
                    DWORD br = 0;
                    if (ReadFile(hModFile, diskHeader.data(), headerSize, &br, nullptr)) {
                      if (memcmp(diskHeader.data(), me.modBaseAddr, headerSize) != 0) {
                        snapshot.peHeaderTamper++;
                        snapshot.suspiciousModules.insert(me.szModule);
                      }
                    }
                  }
                  CloseHandle(hModFile);
                }
              }
            }
          }
        }
      } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
  }

  snapshot.scheduledTaskCount = 0;
  wchar_t taskDir[MAX_PATH] = {};
  if (GetWindowsDirectoryW(taskDir, MAX_PATH)) {
    std::wstring taskPath = std::wstring(taskDir) + L"\\System32\\Tasks";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW((taskPath + L"\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
          if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
            snapshot.scheduledTaskCount++;
          }
        }
      } while (FindNextFileW(hFind, &fd));
      FindClose(hFind);
    }
  }
}

void CollectPowerData(Snapshot& snapshot) {
  SYSTEM_POWER_STATUS sps = {};
  if (GetSystemPowerStatus(&sps)) {
    snapshot.acLineStatus = sps.ACLineStatus;
    snapshot.batteryFlag = sps.BatteryFlag;
    snapshot.batteryLifePercent = sps.BatteryLifePercent;
    if (sps.BatteryLifeTime != (DWORD)-1) {
      snapshot.batteryLifeTimeSec = sps.BatteryLifeTime;
    }
  }

  GUID* pActiveGuid = nullptr;
  if (PowerGetActiveScheme(nullptr, &pActiveGuid) == ERROR_SUCCESS && pActiveGuid) {
    snapshot.powerPlanGuid = *pActiveGuid;

    GUID saverGuid = {0xa1841308, 0x3541, 0x4fab, {0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a}};
    GUID perfGuid = {0x8c5e7fda, 0xe8bf, 0x4a96, {0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c}};
    GUID balancedGuid = {0x381b4222, 0xf694, 0x41f0, {0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e}};

    if (memcmp(pActiveGuid, &saverGuid, sizeof(GUID)) == 0) {
      snapshot.powerSaverActive = 1;
      snapshot.highPerfActive = 0;
      snapshot.balancedActive = 0;
      snapshot.powerPlanIndex = 0;
    } else if (memcmp(pActiveGuid, &perfGuid, sizeof(GUID)) == 0) {
      snapshot.powerSaverActive = 0;
      snapshot.highPerfActive = 1;
      snapshot.balancedActive = 0;
      snapshot.powerPlanIndex = 2;
    } else {
      snapshot.powerSaverActive = 0;
      snapshot.highPerfActive = 0;
      snapshot.balancedActive = 1;
      snapshot.powerPlanIndex = 1;
    }

    LocalFree(pActiveGuid);
  }

  snapshot.batteryChargeRate = 0;
  snapshot.batteryChargeState = 0;
  snapshot.batteryWearLevel = -1;
  snapshot.batteryCycleCount = -1;
  snapshot.batteryTemperature = -1;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (SUCCEEDED(hr)) {
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (SUCCEEDED(hr)) {
      IWbemServices* pSvc = nullptr;
      hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
      if (SUCCEEDED(hr)) {
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (SUCCEEDED(hr)) {
          IEnumWbemClassObject* pEnumerator = nullptr;
          hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_Battery"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
          if (SUCCEEDED(hr)) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              VARIANT vt;
              if (SUCCEEDED(pObj->Get(L"ChargeRate", 0, &vt, nullptr, nullptr))) {
                if (vt.vt != VT_NULL) snapshot.batteryChargeRate = vt.lVal;
                VariantClear(&vt);
              }
              if (SUCCEEDED(pObj->Get(L"EstimatedChargeRemaining", 0, &vt, nullptr, nullptr))) {
                if (vt.vt != VT_NULL) snapshot.batteryChargeState = vt.lVal;
                VariantClear(&vt);
              }
              if (SUCCEEDED(pObj->Get(L"ChargeCompletionTime", 0, &vt, nullptr, nullptr))) {
                VariantClear(&vt);
              }
              if (SUCCEEDED(pObj->Get(L"DesignCapacity", 0, &vt, nullptr, nullptr))) {
                int designCap = (vt.vt != VT_NULL) ? static_cast<int>(vt.lVal) : 0;
                VariantClear(&vt);
                if (SUCCEEDED(pObj->Get(L"FullChargeCapacity", 0, &vt, nullptr, nullptr))) {
                  int fullCap = (vt.vt != VT_NULL) ? static_cast<int>(vt.lVal) : 0;
                  VariantClear(&vt);
                  if (designCap > 0 && fullCap > 0) {
                    snapshot.batteryWearLevel = 100 - (fullCap * 100 / designCap);
                  }
                }
              }
              if (SUCCEEDED(pObj->Get(L"CycleCount", 0, &vt, nullptr, nullptr))) {
                if (vt.vt != VT_NULL) snapshot.batteryCycleCount = static_cast<int>(vt.lVal);
                VariantClear(&vt);
              }
              if (SUCCEEDED(pObj->Get(L"Temperature", 0, &vt, nullptr, nullptr))) {
                if (vt.vt != VT_NULL) snapshot.batteryTemperature = static_cast<int>(vt.lVal) / 10;
                VariantClear(&vt);
              }
              pObj->Release();
            }
            pEnumerator->Release();
          }
        }
        pSvc->Release();
      }
      pLoc->Release();
    }
    CoUninitialize();
  }

  snapshot.idlePowerDrawHigh = 0;
  if (snapshot.cpuPct < 5.0 && snapshot.acLineStatus == 0 && snapshot.batteryLifeTimeSec > 0 && snapshot.batteryLifeTimeSec < 3600) {
    snapshot.idlePowerDrawHigh = 1;
  }

  snapshot.sleepStateActive = 0;
  snapshot.hibernateActive = 0;
  snapshot.modernStandbyActive = -1;
}

void CollectThermalData(Snapshot& snapshot) {
  snapshot.cpuCoreTempC = 0.0;
  snapshot.cpuCoreTempMax = 0.0;
  snapshot.motherboardTempC = 0.0;
  snapshot.vrmTempC = 0.0;
  snapshot.ambientTempC = 0.0;
  snapshot.cpuThrottleTempC = 0.0;
  snapshot.cpuThrottling = 0;
  snapshot.fanSpeeds.clear();
  snapshot.pumpSpeed = 0;
  snapshot.pumpPresent = 0;
  snapshot.thermalSensorCount = 0;
  snapshot.thermalSensorFailures = 0;
  snapshot.thermalHeatSoakIndex = 0.0;
  snapshot.coolingCurveSlope = 0.0;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool coInited = SUCCEEDED(hr);
  if (coInited) {
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (SUCCEEDED(hr)) {
      IWbemServices* pSvc = nullptr;
      hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
      if (SUCCEEDED(hr)) {
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        IEnumWbemClassObject* pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          double totalTemp = 0.0;
          int zoneCount = 0;
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            double zoneTemp = 0.0;
            if (SUCCEEDED(pObj->Get(L"CurrentTemperature", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_I4 || vt.vt == VT_UI4) {
                zoneTemp = (static_cast<double>(vt.lVal) - 2732.0) / 10.0;
              } else if (vt.vt == VT_R8 || vt.vt == VT_R4) {
                zoneTemp = vt.dblVal;
              }
              VariantClear(&vt);
            }
            if (zoneTemp > 0.0 && zoneTemp < 150.0) {
              totalTemp += zoneTemp;
              zoneCount++;
              snapshot.thermalSensorCount++;
              if (zoneTemp > snapshot.cpuCoreTempMax) {
                snapshot.cpuCoreTempMax = zoneTemp;
              }
            } else {
              snapshot.thermalSensorFailures++;
            }
            pObj->Release();
          }
          pEnumerator->Release();
          if (zoneCount > 0) {
            snapshot.cpuCoreTempC = totalTemp / zoneCount;
          }
        }

        IWbemServices* pCimSvc = nullptr;
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pCimSvc);
        if (SUCCEEDED(hr)) {
          CoSetProxyBlanket(pCimSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

          pEnumerator = nullptr;
          hr = pCimSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_Fan"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
          if (SUCCEEDED(hr)) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              snapshot.fanCount++;
              VARIANT vt;
              if (SUCCEEDED(pObj->Get(L"DesiredSpeed", 0, &vt, nullptr, nullptr))) {
                if (vt.vt == VT_I4 || vt.vt == VT_UI4) {
                  snapshot.fanSpeeds.push_back(static_cast<int>(vt.lVal));
                }
                VariantClear(&vt);
              } else {
                snapshot.fanSpeeds.push_back(0);
              }
              pObj->Release();
            }
            pEnumerator->Release();
          }

          pEnumerator = nullptr;
          hr = pCimSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_TemperatureProbe"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
          if (SUCCEEDED(hr)) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              snapshot.thermalSensorCount++;
              VARIANT vt;
              if (SUCCEEDED(pObj->Get(L"CurrentReading", 0, &vt, nullptr, nullptr))) {
                if (vt.vt == VT_NULL) {
                  snapshot.thermalSensorFailures++;
                }
                VariantClear(&vt);
              }
              pObj->Release();
            }
            pEnumerator->Release();
          }

          pCimSvc->Release();
        }
        pSvc->Release();
      }
      pLoc->Release();
    }
  }

  snapshot.motherboardTempC = 0.0;
  snapshot.vrmTempC = 0.0;
  snapshot.ambientTempC = 0.0;

  if (snapshot.cpuCoreTempC > 85.0) {
    snapshot.cpuThrottling = 1;
  }

  if (coInited) CoUninitialize();
}

void CollectHardwareBoardData(Snapshot& snapshot) {
  snapshot.tpmPresent = 0;
  snapshot.tpmReady = 0;
  snapshot.tpmVersion = 0;
  snapshot.voltage12V = 0.0;
  snapshot.voltage5V = 0.0;
  snapshot.voltage33V = 0.0;
  snapshot.voltageVcore = 0.0;
  snapshot.voltageVram = 0.0;
  snapshot.sensorPollFailures = 0;
  snapshot.pcieErrors = 0;
  snapshot.usbResets = 0;
  snapshot.sataResets = 0;
  snapshot.thunderboltEvents = 0;
  snapshot.ecEvents = 0;
  snapshot.cmosBatteryOk = 1;
  snapshot.boardTempHotspot = 0.0;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool coInited = SUCCEEDED(hr);
  if (coInited) {
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (SUCCEEDED(hr)) {
      IWbemServices* pSvc = nullptr;
      hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
      if (SUCCEEDED(hr)) {
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        IEnumWbemClassObject* pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_VoltageProbe"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            double reading = 0.0;
            if (SUCCEEDED(pObj->Get(L"CurrentReading", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_I4 || vt.vt == VT_UI4) reading = static_cast<double>(vt.lVal) / 1000.0;
              else if (vt.vt == VT_R8 || vt.vt == VT_R4) reading = vt.dblVal;
              VariantClear(&vt);
            }
            VARIANT vtName;
            if (SUCCEEDED(pObj->Get(L"Name", 0, &vtName, nullptr, nullptr))) {
              if (vtName.vt == VT_BSTR && vtName.bstrVal) {
                std::wstring name = vtName.bstrVal;
                if (name.find(L"12") != std::wstring::npos && name.find(L"V") != std::wstring::npos) snapshot.voltage12V = reading;
                else if (name.find(L"5") != std::wstring::npos && name.find(L"V") != std::wstring::npos) snapshot.voltage5V = reading;
                else if (name.find(L"3") != std::wstring::npos && name.find(L"3") != std::wstring::npos) snapshot.voltage33V = reading;
                else if (name.find(L"Core") != std::wstring::npos || name.find(L"Vcore") != std::wstring::npos) snapshot.voltageVcore = reading;
                else if (name.find(L"DDR") != std::wstring::npos || name.find(L"Memory") != std::wstring::npos || name.find(L"VRAM") != std::wstring::npos) snapshot.voltageVram = reading;
              }
              VariantClear(&vtName);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_Tpm"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            snapshot.tpmPresent = 1;
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"IsEnabled", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_BOOL) snapshot.tpmReady = vt.boolVal ? 1 : 0;
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"SpecVersion", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_BSTR && vt.bstrVal) {
                std::wstring ver = vt.bstrVal;
                if (ver.find(L"2.0") != std::wstring::npos) snapshot.tpmVersion = 20;
                else if (ver.find(L"1.2") != std::wstring::npos) snapshot.tpmVersion = 12;
              }
              VariantClear(&vt);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_BaseBoard"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"HostingBoard", 0, &vt, nullptr, nullptr))) {
              VariantClear(&vt);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pSvc->Release();
      }
      pLoc->Release();
    }
  }
  if (coInited) CoUninitialize();
}

void ScanDirectory(const wchar_t* path, int& totalFiles, int& hiddenFiles, int& systemFiles, int& reparsePoints, int& sparseFiles) {
  totalFiles = 0;
  hiddenFiles = 0;
  systemFiles = 0;
  reparsePoints = 0;
  sparseFiles = 0;
  WIN32_FIND_DATAW fd = {};
  std::wstring searchPath = std::wstring(path) + L"\\*";
  HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
  if (hFind == INVALID_HANDLE_VALUE) return;
  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    totalFiles++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) hiddenFiles++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) systemFiles++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) reparsePoints++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) sparseFiles++;
  } while (FindNextFileW(hFind, &fd));
  FindClose(hFind);
}

int CountAlternateDataStreams(const wchar_t* filePath) {
  int adsCount = 0;
  WIN32_FIND_STREAM_DATA streamData = {};
  HANDLE hStream = FindFirstStreamW(filePath, FindStreamInfoStandard, &streamData, 0);
  if (hStream != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(streamData.cStreamName, L"::$DATA") != 0) {
        adsCount++;
      }
    } while (FindNextStreamW(hStream, &streamData));
    FindClose(hStream);
  }
  return adsCount;
}

void CollectFilesystemData(Snapshot& snapshot) {
  snapshot.fsSystem32FileCount = 0;
  snapshot.fsSystem32HiddenCount = 0;
  snapshot.fsSystem32SystemCount = 0;
  snapshot.fsDriversFileCount = 0;
  snapshot.fsDriversHiddenCount = 0;
  snapshot.fsRecycleBinContentCount = 0;
  snapshot.fsProgramFilesCount = 0;
  snapshot.fsVolumeDirtyBit = 0;
  snapshot.fsVolumeLabel.clear();
  snapshot.fsVolumeSerial = 0;
  snapshot.fsMountPointCount = 0;
  snapshot.fsReparsePointCount = 0;
  snapshot.fsSparseFileCount = 0;
  snapshot.fsAdsWithDataCount = 0;
  snapshot.fsIntegrityFailures = 0;
  snapshot.fsUsnJournalId = 0;
  snapshot.fsLastUsn = 0;
  snapshot.fsCorruptionWarnings = 0;
  snapshot.fsChkdskPending = 0;

  wchar_t winDir[MAX_PATH] = {};
  GetWindowsDirectoryW(winDir, MAX_PATH);
  std::wstring sys32 = std::wstring(winDir) + L"\\System32";
  std::wstring drivers = std::wstring(winDir) + L"\\System32\\drivers";
  std::wstring recycle = std::wstring(winDir) + L"\\System32\\config\\systemprofile\\AppData\\Local\\Microsoft\\Windows\\Explorer";

  ScanDirectory(sys32.c_str(), snapshot.fsSystem32FileCount, snapshot.fsSystem32HiddenCount, snapshot.fsSystem32SystemCount, snapshot.fsReparsePointCount, snapshot.fsSparseFileCount);
  int driversTotal = 0, driversHidden = 0, driversSys = 0, driversReparse = 0, driversSparse = 0;
  ScanDirectory(drivers.c_str(), driversTotal, driversHidden, driversSys, driversReparse, driversSparse);
  snapshot.fsDriversFileCount = driversTotal;
  snapshot.fsDriversHiddenCount = driversHidden;
  snapshot.fsReparsePointCount += driversReparse;
  snapshot.fsSparseFileCount += driversSparse;

  WIN32_FIND_DATAW fd = {};
  std::wstring recyclePath = recycle + L"\\*";
  HANDLE hFind = FindFirstFileW(recyclePath.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
        snapshot.fsRecycleBinContentCount++;
      }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
  }

  wchar_t progFiles[MAX_PATH] = {};
  if (GetEnvironmentVariableW(L"ProgramFiles", progFiles, MAX_PATH)) {
    int pfTotal = 0, pfHidden = 0, pfSys = 0, pfReparse = 0, pfSparse = 0;
    ScanDirectory(progFiles, pfTotal, pfHidden, pfSys, pfReparse, pfSparse);
    snapshot.fsProgramFilesCount = pfTotal;
    snapshot.fsReparsePointCount += pfReparse;
  }

  wchar_t volRoot[] = {L'C', L':', L'\\', 0};
  wchar_t volLabel[MAX_PATH + 1] = {};
  DWORD volSerial = 0;
  DWORD maxCompLen = 0;
  DWORD fsFlags = 0;
  if (GetVolumeInformationW(volRoot, volLabel, MAX_PATH + 1, &volSerial, &maxCompLen, &fsFlags, nullptr, 0)) {
    snapshot.fsVolumeLabel = volLabel;
    snapshot.fsVolumeSerial = volSerial;
    if (fsFlags & 0x00000002) {
      snapshot.fsVolumeDirtyBit = 1;
      snapshot.fsChkdskPending = 1;
    }
    if (fsFlags & FILE_PERSISTENT_ACLS) {
    }
  }

  DWORD mountCount = 0;
  wchar_t mountPoints[8192] = {};
  if (GetVolumePathNamesForVolumeNameW(L"C:\\", mountPoints, 8192, &mountCount)) {
    snapshot.fsMountPointCount = 0;
    wchar_t* p = mountPoints;
    while (*p) {
      snapshot.fsMountPointCount++;
      p += wcslen(p) + 1;
    }
  }

  wchar_t testPath[MAX_PATH] = {};
  wcscpy_s(testPath, winDir);
  wcscat_s(testPath, L"\\System32\\ntoskrnl.exe");
  snapshot.fsAdsWithDataCount = CountAlternateDataStreams(testPath);

  if (snapshot.fsVolumeDirtyBit == 0) {
    snapshot.fsCorruptionWarnings = 0;
  }
}

unsigned long HashRegistryValues(HKEY hKey) {
  unsigned long hash = 2166136261u;
  wchar_t valueName[128];
  BYTE data[1024];
  DWORD valueNameLen, dataLen, valueType;
  DWORD index = 0;
  const DWORD kMaxValues = 128;
  while (index < kMaxValues) {
    valueNameLen = 128;
    dataLen = 1024;
    if (RegEnumValueW(hKey, index, valueName, &valueNameLen, nullptr, &valueType, data, &dataLen) != ERROR_SUCCESS) break;
    for (DWORD i = 0; i < valueNameLen; ++i) {
      hash ^= static_cast<unsigned char>(valueName[i]);
      hash *= 16777619u;
    }
    if (dataLen > 0 && dataLen <= 1024) {
      for (DWORD i = 0; i < dataLen; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
      }
    }
    ++index;
  }
  return hash;
}

void CollectRegistryData(Snapshot& snapshot) {
  struct RegMonKey {
    const wchar_t* subKey;
    int* outKeyCount;
    int* outValueCount;
    unsigned long* outHash;
  };

  const RegMonKey keys[] = {
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &snapshot.regKeyCountRun, &snapshot.regValueCountRun, &snapshot.regHashRun },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce", &snapshot.regKeyCountRunOnce, &snapshot.regValueCountRunOnce, &snapshot.regHashRunOnce },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run", &snapshot.regKeyCountShell, &snapshot.regValueCountShell, &snapshot.regHashShell },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", &snapshot.regKeyCountPolicies, &snapshot.regValueCountPolicies, &snapshot.regHashPolicies },
    { L"SYSTEM\\CurrentControlSet\\Services", &snapshot.regKeyCountServices, &snapshot.regValueCountServices, &snapshot.regHashServices },
    { L"SYSTEM\\CurrentControlSet\\Services", &snapshot.regKeyCountDrivers, &snapshot.regValueCountDrivers, &snapshot.regHashDrivers },
    { L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy", &snapshot.regKeyCountFirewall, &snapshot.regValueCountFirewall, &snapshot.regHashFirewall },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", &snapshot.regKeyCountUac, &snapshot.regValueCountUac, &snapshot.regHashUac },
    { L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Schedule\\TaskCache", &snapshot.regKeyCountTaskSched, &snapshot.regValueCountTaskSched, &snapshot.regHashTaskSched },
    { L"SOFTWARE\\Classes\\CLSID", &snapshot.regKeyCountCom, &snapshot.regValueCountCom, &snapshot.regHashCom },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Diagnostics\\DiagTrack", &snapshot.regKeyCountTelemetry, &snapshot.regValueCountTelemetry, &snapshot.regHashTelemetry },
    { L"SYSTEM\\CurrentControlSet\\Control\\Lsa", &snapshot.regKeyCountAudit, &snapshot.regValueCountAudit, &snapshot.regHashAudit },
    { L"Environment", &snapshot.regKeyCountEnv, &snapshot.regValueCountEnv, &snapshot.regHashEnv },
    { L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", &snapshot.regKeyCountPath, &snapshot.regValueCountPath, &snapshot.regHashPath },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts", &snapshot.regKeyCountAppAssoc, &snapshot.regValueCountAppAssoc, &snapshot.regHashAppAssoc },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", &snapshot.regKeyCountShellExt, &snapshot.regValueCountShellExt, &snapshot.regHashShellExt },
    { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace", &snapshot.regKeyCountDefApp, &snapshot.regValueCountDefApp, &snapshot.regHashDefApp },
    { L"SOFTWARE\\Classes", &snapshot.regKeyCountFileAssoc, &snapshot.regValueCountFileAssoc, &snapshot.regHashFileAssoc },
  };

  for (const auto& mon : keys) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, mon.subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      DWORD subKeyCount = 0, valueCount = 0, maxSubKeyLen = 0, maxValueNameLen = 0, maxDataLen = 0;
      RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &subKeyCount, &maxSubKeyLen, nullptr, &valueCount, &maxValueNameLen, &maxDataLen, nullptr, nullptr);
      *mon.outKeyCount = static_cast<int>(subKeyCount);
      *mon.outValueCount = static_cast<int>(valueCount);
      if (valueCount <= 256) {
        *mon.outHash = HashRegistryValues(hKey);
      } else {
        *mon.outHash = valueCount * 2654435761u;
      }
      RegCloseKey(hKey);
    } else {
      *mon.outKeyCount = 0;
      *mon.outValueCount = 0;
      *mon.outHash = 0;
    }
  }

  wchar_t pathBuf[32768];
  DWORD pathLen = 32768;
  if (RegGetValueW(HKEY_LOCAL_MACHINE,
    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
    L"Path", RRF_RT_REG_SZ, nullptr, pathBuf, &pathLen) == ERROR_SUCCESS) {
    unsigned long h = 2166136261u;
    for (DWORD i = 0; i < pathLen / sizeof(wchar_t); ++i) {
      h ^= static_cast<unsigned char>(pathBuf[i]);
      h *= 16777619u;
    }
    snapshot.regHashEnvPath = h;
  }

  int startupCount = 0;
  wchar_t startupPath[MAX_PATH];
  if (SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath) == S_OK) {
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((std::wstring(startupPath) + L"\\*.*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
          ++startupCount;
        }
      } while (FindNextFileW(hFind, &fd));
      FindClose(hFind);
    }
  }
  snapshot.startupFolderCount = startupCount;

  snapshot.envVarCount = 0;
  LPWCH envBlock = GetEnvironmentStringsW();
  if (envBlock) {
    LPWCH p = envBlock;
    while (*p) {
      ++snapshot.envVarCount;
      p += lstrlenW(p) + 1;
    }
    FreeEnvironmentStringsW(envBlock);
  }
}

// Audio subsystem telemetry collection
void CollectAudioData(Snapshot& snapshot) {
  snapshot.audioOutputDeviceCount = waveOutGetNumDevs();
  snapshot.audioInputDeviceCount = waveInGetNumDevs();
  snapshot.audioMixerCount = mixerGetNumDevs();

  unsigned long masterVol = 0;
  if (waveOutGetVolume(0, &masterVol) == MMSYSERR_NOERROR) {
    snapshot.audioMasterVolume = masterVol;
    snapshot.audioMasterVolumeLeft = LOWORD(masterVol);
    snapshot.audioMasterVolumeRight = HIWORD(masterVol);
    snapshot.audioSystemMuted = (masterVol == 0) ? 1 : 0;
  }

  if (snapshot.audioInputDeviceCount > 0) {
    WAVEINCAPSW wic;
    if (waveInGetDevCapsW(0, &wic, sizeof(wic)) == MMSYSERR_NOERROR) {
      snapshot.audioSampleRate = wic.dwFormats;
    }
  }

  snapshot.audioWaveOutOpen = 0;
  snapshot.audioWaveInOpen = 0;

  DWORD mixerHash = 2166136261u;
  for (UINT i = 0; i < snapshot.audioMixerCount; ++i) {
    MIXERCAPSW mc;
    if (mixerGetDevCapsW(i, &mc, sizeof(mc)) == MMSYSERR_NOERROR) {
      for (int j = 0; mc.szPname[j]; ++j) {
        mixerHash ^= static_cast<unsigned char>(mc.szPname[j]);
        mixerHash *= 16777619u;
      }
    }
  }
  snapshot.audioDeviceHash = mixerHash;

  DWORD formatHash = 2166136261u;
  for (UINT i = 0; i < snapshot.audioOutputDeviceCount; ++i) {
    WAVEOUTCAPSW woc;
    if (waveOutGetDevCapsW(i, &woc, sizeof(woc)) == MMSYSERR_NOERROR) {
      for (int j = 0; woc.szPname[j]; ++j) {
        formatHash ^= static_cast<unsigned char>(woc.szPname[j]);
        formatHash *= 16777619u;
      }
      formatHash ^= woc.dwFormats;
      formatHash *= 16777619u;
      formatHash ^= woc.wChannels;
      formatHash *= 16777619u;
    }
  }
  snapshot.audioFormatHash = formatHash;

  DWORD svcStatus = 0;
  SERVICE_STATUS_PROCESS ssp;
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_QUERY_LOCK_STATUS);
  if (scm) {
    SC_HANDLE svc = OpenServiceW(scm, L"Audiosrv", SERVICE_QUERY_STATUS);
    if (svc) {
      if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &svcStatus)) {
        snapshot.audioServiceRunning = (ssp.dwCurrentState == SERVICE_RUNNING) ? 1 : 0;
      }
      CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
  }

  snapshot.audioCodecEvents = 0;
  snapshot.audioDecoderErrors = 0;
}

unsigned long HashProcessList() {
  unsigned long hash = 2166136261u;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
      do {
        hash ^= pe.th32ProcessID;
        hash *= 16777619u;
        for (int i = 0; pe.szExeFile[i]; ++i) {
          hash ^= static_cast<unsigned char>(pe.szExeFile[i]);
          hash *= 16777619u;
        }
      } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
  }
  return hash;
}

unsigned long HashModuleList(DWORD processId) {
  unsigned long hash = 2166136261u;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
  if (snap != INVALID_HANDLE_VALUE) {
    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
      do {
        for (int i = 0; me.szModule[i]; ++i) {
          hash ^= static_cast<unsigned char>(me.szModule[i]);
          hash *= 16777619u;
        }
        hash ^= reinterpret_cast<uintptr_t>(me.modBaseAddr);
        hash *= 16777619u;
      } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
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
          if (checked >= 64) break;
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

void CollectGpuDisplayInfo(Snapshot& snapshot) {
  {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
      IWbemLocator* pLoc = nullptr;
      hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
      if (SUCCEEDED(hr) && pLoc) {
        IWbemServices* pSvc = nullptr;
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, 0, 0, &pSvc);
        if (SUCCEEDED(hr) && pSvc) {
          CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
          IEnumWbemClassObject* pEnum = nullptr;
          hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT Name, DriverVersion, AdapterRAM, MaxClockSpeed, CurrentClockSpeed FROM Win32_VideoController"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
          if (SUCCEEDED(hr) && pEnum) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            if (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              VARIANT vt;
              VariantInit(&vt);
              if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, nullptr, nullptr)) && vt.bstrVal) {
                snapshot.gpuModel = vt.bstrVal;
              }
              VariantClear(&vt);
              VariantInit(&vt);
              if (SUCCEEDED(pObj->Get(L"DriverVersion", 0, &vt, nullptr, nullptr)) && vt.bstrVal) {
                snapshot.gpuDriverVersion = vt.bstrVal;
              }
              VariantClear(&vt);
              VariantInit(&vt);
              if (SUCCEEDED(pObj->Get(L"AdapterRAM", 0, &vt, nullptr, nullptr))) {
                snapshot.gpuVramTotalBytes = vt.uintVal;
              }
              VariantClear(&vt);
              pObj->Release();
            }
            pEnum->Release();
          }
          pSvc->Release();
        }
        pLoc->Release();
      }
      CoUninitialize();
    }
  }

  snapshot.displayMonitorCount = 0;
  DISPLAY_DEVICEW dd {};
  dd.cb = sizeof(dd);
  for (int i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
    if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) {
      snapshot.displayMonitorCount++;
      snapshot.displayNames.insert(dd.DeviceName);
    }
    dd.cb = sizeof(dd);
  }

  DEVMODEW dm {};
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
    snapshot.displayWidth = dm.dmPelsWidth;
    snapshot.displayHeight = dm.dmPelsHeight;
    snapshot.displayRefreshRateHz = dm.dmDisplayFrequency;
    snapshot.displayBitsPerPel = dm.dmBitsPerPel;
  }

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD tdrLevel = 3;
    DWORD dataSize = sizeof(tdrLevel);
    RegQueryValueExW(hKey, L"TdrLevel", nullptr, nullptr, reinterpret_cast<BYTE*>(&tdrLevel), &dataSize);
    snapshot.tdrLevel = static_cast<int>(tdrLevel);
    RegCloseKey(hKey);
  }

  HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
  if (dwm) {
    typedef HRESULT (WINAPI *DwmIsCompositionEnabled_t)(BOOL*);
    auto pDwmIsCompositionEnabled = reinterpret_cast<DwmIsCompositionEnabled_t>(GetProcAddress(dwm, "DwmIsCompositionEnabled"));
    if (pDwmIsCompositionEnabled) {
      BOOL enabled = FALSE;
      if (SUCCEEDED(pDwmIsCompositionEnabled(&enabled))) {
        snapshot.desktopCompositionEnabled = enabled ? 1 : 0;
      }
    }
    FreeLibrary(dwm);
  }
}

void CollectNetworkDiagnostics(Snapshot& snapshot) {
  snapshot.pingRttMs = IcmpPingGateway();
  snapshot.dnsResolutionMs = ProbeDnsResolution();
  snapshot.dnsResolutionOk = (snapshot.dnsResolutionMs >= 0) ? 1 : 0;
  snapshot.routeTableHash = ComputeRouteTableHash();

  ULONG aaSize = 0;
  GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &aaSize);
  if (aaSize > 0) {
    std::vector<BYTE> aaBuf(aaSize);
    auto* aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aaBuf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &aaSize) == NO_ERROR) {
      snapshot.netAdapterCount = 0;
      for (auto* a = aa; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->OperStatus != IfOperStatusUp) continue;
        snapshot.netAdapterCount++;
        snapshot.netAdapterOperStatuses.insert(a->OperStatus);
        snapshot.netAdapterTypes.insert(a->IfType);
        if (a->IfType == IF_TYPE_TUNNEL || a->IfType == IF_TYPE_PPP || a->IfType == IF_TYPE_IEEE1394) {
          if (a->FriendlyName) snapshot.vpnAdapterDescriptions.insert(a->FriendlyName);
        }
        if (a->Description) {
          std::wstring desc = a->Description;
          for (auto& ch : desc) ch = towlower(ch);
          if (desc.find(L"vpn") != std::wstring::npos || desc.find(L"tunnel") != std::wstring::npos ||
              desc.find(L"wireguard") != std::wstring::npos || desc.find(L"tap") != std::wstring::npos ||
              desc.find(L"openvpn") != std::wstring::npos || desc.find(L"forticlient") != std::wstring::npos) {
            snapshot.vpnAdapterDescriptions.insert(a->FriendlyName ? a->FriendlyName : L"VPN");
          }
        }
        DWORD speed = static_cast<DWORD>(a->TransmitLinkSpeed);
        snapshot.netAdapterSpeeds.insert(speed);
        if (speed > snapshot.netPrimaryLinkSpeedBps) {
          snapshot.netPrimaryLinkSpeedBps = speed;
        }
        for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
          if (ua->Address.lpSockaddr->sa_family == AF_INET) {
            auto* sa4 = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char ipBuf[INET_ADDRSTRLEN] {};
            inet_ntop(AF_INET, &sa4->sin_addr, ipBuf, sizeof(ipBuf));
            snapshot.netAdapterAddresses.insert(Utf8ToWide(ipBuf));
          }
        }
        for (auto* ga = a->FirstGatewayAddress; ga; ga = ga->Next) {
          if (ga->Address.lpSockaddr->sa_family == AF_INET) {
            auto* sa4 = reinterpret_cast<sockaddr_in*>(ga->Address.lpSockaddr);
            char ipBuf[INET_ADDRSTRLEN] {};
            inet_ntop(AF_INET, &sa4->sin_addr, ipBuf, sizeof(ipBuf));
            std::wstring gw = Utf8ToWide(ipBuf);
            if (gw != L"0.0.0.0") snapshot.netAdapterGateways.insert(gw);
          }
        }
      }
    }
  }

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD proxyEnabled = 0;
    DWORD dataSize = sizeof(proxyEnabled);
    RegQueryValueExW(hKey, L"ProxyEnable", nullptr, nullptr, reinterpret_cast<BYTE*>(&proxyEnabled), &dataSize);
    snapshot.proxyEnabled = static_cast<int>(proxyEnabled);
    RegCloseKey(hKey);
  }
}

uint64_t CountTcpResets() {
  uint64_t resets = 0;
  DWORD tableSize = 0;
  GetExtendedTcpTable(nullptr, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
  if (tableSize > 0) {
    std::vector<BYTE> buf(tableSize);
    auto* tcp = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
    if (GetExtendedTcpTable(tcp, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
      for (DWORD i = 0; i < tcp->dwNumEntries; ++i) {
        DWORD st = tcp->table[i].dwState;
        if (st == MIB_TCP_STATE_TIME_WAIT || st == MIB_TCP_STATE_CLOSING || st == MIB_TCP_STATE_LAST_ACK) {
          resets++;
        }
      }
    }
  }
  return resets;
}

}
