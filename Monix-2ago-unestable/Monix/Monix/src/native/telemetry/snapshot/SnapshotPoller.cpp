#include "SnapshotPoller.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../../MonixApp.hpp"
#include "../../TelemetryInternal.hpp"
#include "../../crash/CrashHandling.hpp"
#include "../../telemetry/Collectors.hpp"
#include "../../core/TextUtils.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <psapi.h>
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <vector>

using namespace monix;
using namespace monix::internal;

Snapshot MonixApp::PollSnapshot() {
  return PollNativeSnapshot();
}

Snapshot MonixApp::PollNativeSnapshot() {
  g_phase = "POLL:ENTER";
  internal::HeapOk("ENTER");
  const int sc = state_.session.sampleCount;
  auto snapshotPtr = std::make_unique<Snapshot>();
  Snapshot& snapshot = *snapshotPtr;
  if (state_.hasPreviousSnapshot) {
    snapshot = state_.snapshot;
  }
  static std::uint64_t s_nextSnapshotId = 1;
  snapshot.snapshotId = s_nextSnapshotId++;
  {
    LARGE_INTEGER freq, qpcNow;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&qpcNow);
    snapshot.collectedAtMonotonicNs = static_cast<std::uint64_t>(qpcNow.QuadPart) * 1000000000ULL /
      static_cast<std::uint64_t>(freq.QuadPart);
    snapshot.collectedAtWallMs = GetTickCount64();
  }
  g_phase = "POLL:SNAP_CREATED";
  internal::HeapOk("SNAP_CREATED");

  wchar_t hostBuffer[MAX_COMPUTERNAME_LENGTH + 1] {};
  DWORD hostSize = static_cast<DWORD>(std::size(hostBuffer));
  if (GetComputerNameW(hostBuffer, &hostSize)) {
    snapshot.host = hostBuffer;
  }
  g_phase = "POLL:HOST_DONE";
  internal::HeapOk("HOST_DONE");

  LARGE_INTEGER nowQpc {};
  QueryPerformanceCounter(&nowQpc);
  double elapsedSeconds = 0.0;
  if (lastNativeSampleQpc_.QuadPart > 0 && qpcFrequency_.QuadPart > 0) {
    elapsedSeconds = static_cast<double>(nowQpc.QuadPart - lastNativeSampleQpc_.QuadPart) / static_cast<double>(qpcFrequency_.QuadPart);
  }
  if (elapsedSeconds <= 0.0) {
    elapsedSeconds = 0.10;
  }

  RamInfo ramInfo;
  hw_read_ram(&ramInfo);
  snapshot.ramTotalBytes = ramInfo.totalBytes;
  snapshot.ramUsedBytes = ramInfo.usedBytes;
  g_phase = "POLL:RAM_DONE";
  internal::HeapOk("RAM_DONE");

  PERFORMANCE_INFORMATION performance {};
  performance.cb = sizeof(performance);
  GetPerformanceInfo(&performance, sizeof(performance));
  snapshot.handleCount = static_cast<int>(performance.HandleCount);
  snapshot.processCount = static_cast<int>(performance.ProcessCount);
  snapshot.threadCount = static_cast<int>(performance.ThreadCount);
  g_phase = "POLL:PERF_DONE";
  internal::HeapOk("PERF_DONE");

  MemoryInfo memInfo;
  hw_read_memory_info(&memInfo);
  snapshot.ramAvailBytes = memInfo.availablePhysicalBytes;
  g_phase = "POLL:MEM_DONE";
  internal::HeapOk("MEM_DONE");

  CpuTimes cpuTimes;
  hw_read_cpu_times(&cpuTimes);
  g_phase = "POLL:CPUTIMES_DONE";
  internal::HeapOk("CPUTIMES_DONE");

  if (!cpuBaselineCaptured_) {
    g_phase = "POLL:CPUID_START";
    cpu_identify(&cpuBaseline_);
    g_phase = "POLL:CPUID_DONE";
    cpuBaselineCaptured_ = true;
    CpuMeta meta;
    g_phase = "POLL:CPU_META_START";
    hw_read_cpu_meta(&meta);
    g_phase = "POLL:CPU_META_DONE2";
    hw_estimate_base_clock(&meta);
    g_phase = "POLL:BASE_CLOCK_DONE";
    cpuBaseTscPerSec_ = meta.tscPerSec;
  }
  g_phase = "POLL:BASELINE_DONE";

  memcpy(snapshot.cpuVendor, cpuBaseline_.vendor, sizeof(snapshot.cpuVendor));
  memcpy(snapshot.cpuBrand, cpuBaseline_.brand, sizeof(snapshot.cpuBrand));
  g_phase = "POLL:CPU_META_DONE";
  internal::HeapOk("CPU_META_DONE");
  snapshot.cpuFamily = cpuBaseline_.family;
  snapshot.cpuModel = cpuBaseline_.model;
  snapshot.cpuStepping = cpuBaseline_.stepping;
  snapshot.cpuCores = cpuBaseline_.physicalCores;
  snapshot.cpuFeaturesEdx = cpuBaseline_.featuresEdx;
  snapshot.cpuFeaturesEcx = cpuBaseline_.featuresEcx;
  snapshot.cpuExtFeatures = cpuBaseline_.extFeatures;

  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  snapshot.cpuLogicalCpus = static_cast<uint32_t>(sysInfo.dwNumberOfProcessors);
  snapshot.cpuHtEnabled = (cpuBaseline_.featuresEdx & (1 << 28)) ? 1 : 0;

  {
    uint64_t tsc = cpu_read_tsc();
    snapshot.cpuTscDelta = tsc - baseline_.cpuTsc;
    baseline_.cpuTsc = tsc;

    if (cpuTimesInitialized_ && elapsedSeconds > 0.0 && snapshot.cpuTscDelta > 0) {
      snapshot.estimatedFrequencyMhz = (double)(snapshot.cpuTscDelta / (uint64_t)(elapsedSeconds * 1000000.0));
    }
    cpuTimesInitialized_ = true;
  }

  if (cpuTimes.userTime > 0) {
    const uint64_t kernelOnly = cpuTimes.kernelTime - cpuTimes.idleTime;
    snapshot.ipcEstimate = (double)cpuTimes.userTime / (double)(kernelOnly + cpuTimes.userTime);
  }

  internal::HeapOk("BEFORE_MAPS");
  volatile uint64_t stackCanary1 = 0xDEADBEEFCAFE1234ULL;
  volatile uint64_t stackCanary2 = 0x1234567890ABCDEFULL;
  std::map<int, std::wstring> processNames;
  std::map<int, NativeProcessSample> currentSamples;
  std::uint64_t totalReadBytes = 0;
  std::uint64_t totalWriteBytes = 0;

  std::uint64_t systemTotalDeltaForProcesses = 0;
  {
    FILETIME idleTimeSys {}, kernelTimeSys {}, userTimeSys {};
    if (GetSystemTimes(&idleTimeSys, &kernelTimeSys, &userTimeSys)) {
      const std::uint64_t curIdle = (static_cast<std::uint64_t>(idleTimeSys.dwHighDateTime) << 32) | idleTimeSys.dwLowDateTime;
      const std::uint64_t curKernel = (static_cast<std::uint64_t>(kernelTimeSys.dwHighDateTime) << 32) | kernelTimeSys.dwLowDateTime;
      const std::uint64_t curUser = (static_cast<std::uint64_t>(userTimeSys.dwHighDateTime) << 32) | userTimeSys.dwLowDateTime;
      if (lastSystemIdleTime_ > 0) {
        const std::uint64_t kernelDelta = curKernel - lastSystemKernelTime_;
        const std::uint64_t userDelta = curUser - lastSystemUserTime_;
        const std::uint64_t idleDelta = curIdle - lastSystemIdleTime_;
        const std::uint64_t totalWall = kernelDelta + userDelta;
        if (totalWall > 0) {
          systemTotalDeltaForProcesses = totalWall;
          const std::uint64_t busyTime = totalWall - idleDelta;
          snapshot.cpuPct = std::clamp(static_cast<double>(busyTime) / static_cast<double>(totalWall) * 100.0, 0.0, 100.0);
        }
      }
      lastSystemIdleTime_ = curIdle;
      lastSystemKernelTime_ = curKernel;
      lastSystemUserTime_ = curUser;
    }
  }

  HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (processSnapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(processSnapshot, &entry)) {
      do {
        if (entry.th32ProcessID == 0 || entry.th32ProcessID == GetCurrentProcessId()) {
          continue;
        }

        ProcessInfo process;
        process.name = entry.szExeFile;
        process.pid = static_cast<int>(entry.th32ProcessID);
        process.parentPid = static_cast<int>(entry.th32ParentProcessID);
        process.status = L"ACTIVE";
        process.priority = L"NORMAL";
        processNames[process.pid] = process.name;

        DWORD sessionId = 0;
        if (ProcessIdToSessionId(entry.th32ProcessID, &sessionId)) {
          process.sessionId = static_cast<int>(sessionId);
        }

        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        NativeProcessSample sample {};
        if (handle) {
          PROCESS_MEMORY_COUNTERS_EX memoryCounters {};
          if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters), sizeof(memoryCounters))) {
            process.ramBytes = memoryCounters.WorkingSetSize;
          }

          FILETIME createTime {};
          FILETIME exitTime {};
          FILETIME kernel {};
          FILETIME user {};
          if (GetProcessTimes(handle, &createTime, &exitTime, &kernel, &user)) {
            sample.createTime = FileTimeToUInt64(createTime);
            sample.cpuTime = FileTimeToUInt64(kernel) + FileTimeToUInt64(user);
            process.createTime100ns = sample.createTime;
            process.processGuid = FormatProcessGuid(process.pid, sample.createTime);
          }

          IO_COUNTERS io {};
          if (GetProcessIoCounters(handle, &io)) {
            sample.readBytes = io.ReadTransferCount;
            sample.writeBytes = io.WriteTransferCount;
            totalReadBytes += sample.readBytes;
            totalWriteBytes += sample.writeBytes;
          }

          DWORD handleCount = 0;
          if (GetProcessHandleCount(handle, &handleCount)) {
          }

          const DWORD priority = GetPriorityClass(handle);
          switch (priority) {
            case HIGH_PRIORITY_CLASS: process.priority = L"HIGH"; break;
            case IDLE_PRIORITY_CLASS: process.priority = L"LOW"; break;
            case REALTIME_PRIORITY_CLASS: process.priority = L"REALTIME"; break;
            case BELOW_NORMAL_PRIORITY_CLASS: process.priority = L"BELOW"; break;
            case ABOVE_NORMAL_PRIORITY_CLASS: process.priority = L"ABOVE"; break;
            default: process.priority = L"NORMAL"; break;
          }
          CloseHandle(handle);
        }

        const auto previousSample = previousProcessSamples_.find(process.pid);
        if (previousSample != previousProcessSamples_.end() && systemTotalDeltaForProcesses > 0 && sample.cpuTime >= previousSample->second.cpuTime) {
          const std::uint64_t processDelta = sample.cpuTime - previousSample->second.cpuTime;
          process.cpuPct = std::clamp((static_cast<double>(processDelta) / static_cast<double>(systemTotalDeltaForProcesses)) * 100.0, 0.0, 100.0);
        }

        currentSamples[process.pid] = sample;

        const std::wstring upper = ToUpper(process.name);
        if (upper.find(L"LSASS") != std::wstring::npos || upper.find(L"CSRSS") != std::wstring::npos || upper.find(L"SYSTEM") != std::wstring::npos || upper.find(L"SVCHOST") != std::wstring::npos) {
          process.status = L"SYSTEM";
        } else if (upper.find(L"CHATGPT") != std::wstring::npos || upper.find(L"MONIX") != std::wstring::npos) {
          process.status = L"RUNNING";
        }

        snapshot.processes.push_back(std::move(process));
      } while (Process32NextW(processSnapshot, &entry));
    }
    CloseHandle(processSnapshot);
  }
  g_phase = "POLL:PROC_ENUM_DONE";
  internal::HeapOk("PROC_ENUM_DONE");

  if (nativeBaselineReady_ && elapsedSeconds > 0.0) {
    snapshot.diskReadBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalReadBytes - std::min(totalReadBytes, lastDiskReadBytes_)) / elapsedSeconds);
    snapshot.diskWriteBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalWriteBytes - std::min(totalWriteBytes, lastDiskWriteBytes_)) / elapsedSeconds);
  }
  lastDiskReadBytes_ = totalReadBytes;
  lastDiskWriteBytes_ = totalWriteBytes;

  StorageInfo storInfo;
  hw_read_storage_info(&storInfo);
  snapshot.diskTotalBytes = storInfo.usage.totalBytes;
  snapshot.diskFreeBytes = storInfo.usage.freeBytes;
  snapshot.diskPctUsed = storInfo.usage.usedPercent;
  snapshot.diskQueueLength = storInfo.performance.diskQueueLength;
  snapshot.diskReadLatencyMs = storInfo.performance.readLatencyMs;
  snapshot.diskWriteLatencyMs = storInfo.performance.writeLatencyMs;
  snapshot.diskReadIops = storInfo.performance.readOpsPerSec;
  snapshot.diskWriteIops = storInfo.performance.writeOpsPerSec;
  snapshot.smartHealthOk = storInfo.health.smartHealthOk;
  snapshot.nvmeTempC = storInfo.health.temperatureC;
  snapshot.nvmeTempValid = storInfo.health.temperatureValid;
  if (snapshot.nvmeTempValid == 1) {
    snapshot.storageTempC = snapshot.nvmeTempC;
    snapshot.storageTempEstimated = false;
  }
  snapshot.previousDiskReadBytesPerSec = lastDiskReadBytesPerSec_;
  snapshot.previousDiskWriteBytesPerSec = lastDiskWriteBytesPerSec_;
  snapshot.previousDiskReadIops = lastDiskReadIops_;
  snapshot.previousDiskWriteIops = lastDiskWriteIops_;
  lastDiskReadBytesPerSec_ = snapshot.diskReadBytesPerSec;
  lastDiskWriteBytesPerSec_ = snapshot.diskWriteBytesPerSec;
  lastDiskReadIops_ = snapshot.diskReadIops;
  lastDiskWriteIops_ = snapshot.diskWriteIops;

  std::uint64_t totalNetIn = 0;
  std::uint64_t totalNetOut = 0;
  ULONG ifTableSize = 0;
  GetIfTable(nullptr, &ifTableSize, FALSE);
  if (ifTableSize > 0) {
    std::vector<BYTE> ifBuffer(ifTableSize);
    auto* ifTable = reinterpret_cast<MIB_IFTABLE*>(ifBuffer.data());
    if (GetIfTable(ifTable, &ifTableSize, FALSE) == NO_ERROR) {
      for (DWORD i = 0; i < ifTable->dwNumEntries; ++i) {
        const auto& row = ifTable->table[i];
        if (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL && row.dwType != IF_TYPE_SOFTWARE_LOOPBACK) {
          totalNetIn += row.dwInOctets;
          totalNetOut += row.dwOutOctets;
        }
      }
    }
  }
  if (nativeBaselineReady_ && elapsedSeconds > 0.0) {
    snapshot.netDownBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalNetIn - std::min(totalNetIn, lastNetworkInBytes_)) / elapsedSeconds);
    snapshot.netUpBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalNetOut - std::min(totalNetOut, lastNetworkOutBytes_)) / elapsedSeconds);
  }
  lastNetworkInBytes_ = totalNetIn;
  lastNetworkOutBytes_ = totalNetOut;

  std::map<int, NetworkFlow> flowByPid;
  auto flowName = [&](DWORD pid) {
    const auto name = processNames.find(static_cast<int>(pid));
    if (name != processNames.end()) {
      return name->second + L" PID " + std::to_wstring(pid);
    }
    return L"PID " + std::to_wstring(pid);
  };

  snapshot.outboundConnections = 0;
  snapshot.inboundConnections = 0;
  DWORD tableSize = 0;
  GetExtendedTcpTable(nullptr, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
  if (tableSize > 0) {
    std::vector<BYTE> buffer(tableSize);
    auto* tcpTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    if (GetExtendedTcpTable(tcpTable, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
      for (DWORD i = 0; i < tcpTable->dwNumEntries; ++i) {
        const auto& row = tcpTable->table[i];
        if (row.dwOwningPid == GetCurrentProcessId()) {
          continue;
        }
        NetworkFlow& flow = flowByPid[static_cast<int>(row.dwOwningPid)];
        flow.name = flowName(row.dwOwningPid);
        if (row.dwState == MIB_TCP_STATE_ESTAB) {
          ++flow.activeConnections;
          ++snapshot.outboundConnections;
          flow.state = L"ACTIVE";
          const std::wstring remote = FormatIpv4(row.dwRemoteAddr) + L":" + std::to_wstring(ntohs(static_cast<u_short>(row.dwRemotePort)));
          if (flow.remote.empty()) {
            flow.remote = remote;
          } else if (flow.remote.size() < 120 && flow.remote.find(remote) == std::wstring::npos) {
            flow.remote += L", " + remote;
          }
        } else if (row.dwState == MIB_TCP_STATE_LISTEN) {
          ++snapshot.inboundConnections;
          if (flow.remote.empty()) {
            flow.remote = L"listening :" + std::to_wstring(ntohs(static_cast<u_short>(row.dwLocalPort)));
          }
          if (flow.state.empty()) {
            flow.state = L"LISTEN";
          }
        }
      }
    }
  }

  tableSize = 0;
  GetExtendedUdpTable(nullptr, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
  if (tableSize > 0) {
    std::vector<BYTE> buffer(tableSize);
    auto* udpTable = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());
    if (GetExtendedUdpTable(udpTable, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
      snapshot.udpConnectionCount = static_cast<int>(udpTable->dwNumEntries);
      for (DWORD i = 0; i < udpTable->dwNumEntries; ++i) {
        const auto& row = udpTable->table[i];
        if (row.dwOwningPid == GetCurrentProcessId()) {
          continue;
        }
        NetworkFlow& flow = flowByPid[static_cast<int>(row.dwOwningPid)];
        flow.name = flowName(row.dwOwningPid);
        flow.state = flow.state.empty() ? L"UDP" : flow.state;
        if (flow.remote.empty()) {
          flow.remote = L"udp :" + std::to_wstring(ntohs(static_cast<u_short>(row.dwLocalPort)));
        }
      }
    }
  }

  for (auto it = flowByPid.begin(); it != flowByPid.end(); ++it) {
    auto& flow = it->second;
    if (flow.remote.empty()) {
      flow.remote = L"local";
    }
    if (flow.state.empty()) {
      flow.state = L"IDLE";
    }
    snapshot.flows.push_back(std::move(flow));
  }

  std::sort(snapshot.flows.begin(), snapshot.flows.end(), [](const NetworkFlow& left, const NetworkFlow& right) {
    return left.activeConnections > right.activeConnections;
  });
  if (snapshot.flows.size() > 24) {
    snapshot.flows.resize(24);
  }
  internal::HeapOk("FLOWS_DONE");

  snapshot.dnsPseudo = -1;

  snapshot.latencyMs = -1;
  snapshot.latencySource = L"unavailable";

  snapshot.tcpResets = CountTcpTeardownStates();

  MIB_TCPSTATS tcpStats {};
  if (GetTcpStatistics(&tcpStats) == NO_ERROR) {
    snapshot.tcpRetransmits = tcpStats.dwRetransSegs;
  }

  CollectSchedulerData(snapshot);
  CollectNetworkDiagnostics(snapshot);
  g_phase = "POLL:SCHED_NET_DONE";
  if (snapshot.pingRttMs >= 0) {
    snapshot.latencyMs = snapshot.pingRttMs;
    snapshot.latencySource = L"icmp";
  }
  if (snapshot.dnsResolutionMs >= 0) {
    snapshot.dnsPseudo = std::clamp(snapshot.dnsResolutionMs, 0, 96);
  }

  if (nativeBaselineReady_ && elapsedSeconds > 0.0) {
    const uint64_t csDelta = snapshot.totalContextSwitches >= baseline_.totalContextSwitches ?
      snapshot.totalContextSwitches - baseline_.totalContextSwitches : 0;
    const double csRate = static_cast<double>(csDelta) / elapsedSeconds;
    snapshot.contextSwitchesPerSec = (csRate > 1000000.0 || csRate < 0.0) ?
      -1 : static_cast<int>(csRate);

    const uint64_t irqDelta = snapshot.totalInterruptCount >= baseline_.totalInterruptCount ?
      snapshot.totalInterruptCount - baseline_.totalInterruptCount : 0;
    const double irqRate = static_cast<double>(irqDelta) / elapsedSeconds;
    snapshot.interruptsPerSec = (irqRate > 500000.0 || irqRate < 0.0) ?
      -1 : static_cast<int>(irqRate);

    const uint64_t dpcDelta = snapshot.totalDpcCount >= baseline_.totalDpcCount ?
      snapshot.totalDpcCount - baseline_.totalDpcCount : 0;
    snapshot.dpcTimePerSec = static_cast<uint64_t>(static_cast<double>(dpcDelta) / elapsedSeconds);

    const uint64_t isrDelta = snapshot.totalIsrCount >= baseline_.totalIsrCount ?
      snapshot.totalIsrCount - baseline_.totalIsrCount : 0;
    snapshot.isrTimePerSec = static_cast<uint64_t>(static_cast<double>(isrDelta) / elapsedSeconds);

    snapshot.processorQueueLength = snapshot.readyThreadCount > 0 ?
      std::max(0, snapshot.readyThreadCount - static_cast<int>(snapshot.cpuLogicalCpus)) : 0;
    snapshot.systemCallsPerSec = -1;

    const int prevThreadCount = state_.hasPreviousSnapshot ? state_.snapshot.threadCount : snapshot.threadCount;
    snapshot.threadCountDelta = snapshot.threadCount - prevThreadCount;
    snapshot.threadCreationDelta = snapshot.threadCountDelta > 0 ? snapshot.threadCountDelta : 0;
    snapshot.threadTerminationDelta = snapshot.threadCountDelta < 0 ? -snapshot.threadCountDelta : 0;
  } else {
    snapshot.processorQueueLength = -1;
    snapshot.contextSwitchesPerSec = -1;
    snapshot.systemCallsPerSec = -1;
    snapshot.interruptsPerSec = -1;
    snapshot.threadCountDelta = 0;
    snapshot.threadCreationDelta = 0;
    snapshot.threadTerminationDelta = 0;
  }
  baseline_.totalContextSwitches = snapshot.totalContextSwitches;
  baseline_.totalInterruptCount = snapshot.totalInterruptCount;
  baseline_.totalDpcCount = snapshot.totalDpcCount;
  baseline_.totalIsrCount = snapshot.totalIsrCount;
  snapshot.uptimeSeconds = hw_get_uptime_ms() / 1000ull;
  snapshot.uptimeMs = GetTickCount64();

  {
    static PDH_HQUERY gpuQuery = nullptr;
    static PDH_HCOUNTER gpuCounter = nullptr;
    static int gpuRetryCount = 0;
    static constexpr int kMaxGpuRetries = 5;
    static constexpr int kGpuRetryInterval = 30;
    if (!gpuQuery && gpuRetryCount < kMaxGpuRetries) {
      if (PdhOpenQueryW(nullptr, 0, &gpuQuery) == ERROR_SUCCESS) {
        if (PdhAddEnglishCounterW(gpuQuery, L"\\GPU Engine(*Total)\\Utilization Percentage", 0, &gpuCounter) != ERROR_SUCCESS) {
          PdhCloseQuery(gpuQuery);
          gpuQuery = nullptr;
          gpuRetryCount++;
        }
      } else {
        gpuRetryCount++;
      }
    }
    if (gpuQuery) {
      PDH_FMT_COUNTERVALUE gpuVal {};
      if (PdhCollectQueryData(gpuQuery) == ERROR_SUCCESS &&
          PdhGetFormattedCounterValue(gpuCounter, PDH_FMT_DOUBLE, nullptr, &gpuVal) == ERROR_SUCCESS) {
        snapshot.gpuPct = std::clamp(gpuVal.doubleValue, 0.0, 100.0);
        snapshot.gpuPctValid = 1;
        gpuRetryCount = 0;
      } else {
        snapshot.gpuPctValid = -1;
        if (gpuRetryCount < kMaxGpuRetries && sc % kGpuRetryInterval == 0) {
          PdhCloseQuery(gpuQuery);
          gpuQuery = nullptr;
          gpuRetryCount++;
        }
      }
    } else {
      snapshot.gpuPctValid = -1;
    }
  if (snapshot.gpuPctValid == 1) {
    snapshot.gpuTempC = 0.0;
    snapshot.gpuTempEstimated = false;
  }
  }

  CollectGpuDisplayInfo(snapshot);
  g_phase = "POLL:GPU_DONE";
  CollectAudioData(snapshot);
  g_phase = "POLL:AUDIO_DONE";

  if (sc % 4 == 0) {
    CollectOsKernelData(snapshot);
    g_phase = "POLL:OS_DONE";
    CollectPowerData(snapshot);
    g_phase = "POLL:POWER_DONE";
    CollectThermalData(snapshot);
    g_phase = "POLL:THERMAL_DONE";
  }
  if (sc % 5 == 0) {
    CollectSecurityData(snapshot);
    g_phase = "POLL:SECURITY_DONE";
    CollectHardwareBoardData(snapshot);
    g_phase = "POLL:HW_DONE";
    internal::HeapOk("SEC_HW_DONE");
  }
  if (sc % 10 == 0) {
    CollectFilesystemData(snapshot);
    g_phase = "POLL:FS_DONE";
    CollectRegistryData(snapshot);
    g_phase = "POLL:REG_DONE";
    CollectReliabilityData(snapshot);
    g_phase = "POLL:RELIABILITY_DONE";
    internal::HeapOk("FS_REG_DONE");
  }

  baseline_.driverNames = snapshot.driverNames;
  baseline_.totalHandles = snapshot.totalHandles;
  baseline_.totalObjects = snapshot.totalObjects;
  baseline_.pageFaultsDelta = snapshot.pageFaultsDelta;
  baseline_.ioReadBytesDelta = snapshot.ioReadBytesDelta;
  baseline_.ioWriteBytesDelta = snapshot.ioWriteBytesDelta;
  baseline_.systemTime100ns = snapshot.systemTime100ns;
  baseline_.sessionCount = snapshot.sessionCount;
  baseline_.selfSignatureValid = snapshot.selfSignatureValid;
  baseline_.selfHashComputed = snapshot.selfHashComputed;
  baseline_.unsignedDriverCount = snapshot.unsignedDriverCount;
  baseline_.unsignedDriverNames = snapshot.unsignedDriverNames;
  baseline_.suspiciousScriptHosts = snapshot.suspiciousScriptHosts;
  baseline_.uacConsentProcesses = snapshot.uacConsentProcesses;
  baseline_.lsassAccessCount = snapshot.lsassAccessCount;
  baseline_.debugPortActive = snapshot.debugPortActive;
  baseline_.hookModulesDetected = snapshot.hookModulesDetected;
  baseline_.peHeaderTamper = snapshot.peHeaderTamper;
  baseline_.scheduledTaskCount = snapshot.scheduledTaskCount;
  baseline_.suspiciousModules = snapshot.suspiciousModules;
  baseline_.acLineStatus = snapshot.acLineStatus;
  baseline_.batteryFlag = snapshot.batteryFlag;
  baseline_.batteryLifePercent = snapshot.batteryLifePercent;
  baseline_.batteryLifeTimeSec = snapshot.batteryLifeTimeSec;
  baseline_.batteryChargeRate = snapshot.batteryChargeRate;
  baseline_.batteryChargeState = snapshot.batteryChargeState;
  baseline_.batteryWearLevel = snapshot.batteryWearLevel;
  baseline_.batteryCycleCount = snapshot.batteryCycleCount;
  baseline_.batteryTemperature = snapshot.batteryTemperature;
  baseline_.powerPlanIndex = snapshot.powerPlanIndex;
  baseline_.powerSaverActive = snapshot.powerSaverActive;
  baseline_.highPerfActive = snapshot.highPerfActive;
  baseline_.idlePowerDrawHigh = snapshot.idlePowerDrawHigh;
  baseline_.cpuCoreTempC = snapshot.cpuCoreTempC;
  baseline_.cpuCoreTempMax = snapshot.cpuCoreTempMax;
  baseline_.motherboardTempC = snapshot.motherboardTempC;
  baseline_.vrmTempC = snapshot.vrmTempC;
  baseline_.ambientTempC = snapshot.ambientTempC;
  baseline_.cpuThrottling = snapshot.cpuThrottling;
  baseline_.fanCount = snapshot.fanCount;
  baseline_.fanSpeeds = snapshot.fanSpeeds;
  baseline_.pumpSpeed = snapshot.pumpSpeed;
  baseline_.thermalSensorCount = snapshot.thermalSensorCount;
  baseline_.thermalSensorFailures = snapshot.thermalSensorFailures;
  baseline_.voltage12V = snapshot.voltage12V;
  baseline_.voltage5V = snapshot.voltage5V;
  baseline_.voltage33V = snapshot.voltage33V;
  baseline_.voltageVcore = snapshot.voltageVcore;
  baseline_.tpmPresent = snapshot.tpmPresent;
  baseline_.tpmReady = snapshot.tpmReady;
  baseline_.cmosBatteryOk = snapshot.cmosBatteryOk;
  baseline_.sensorPollFailures = snapshot.sensorPollFailures;

  baseline_.audioOutputDeviceCount = snapshot.audioOutputDeviceCount;
  baseline_.audioInputDeviceCount = snapshot.audioInputDeviceCount;
  baseline_.audioMixerCount = snapshot.audioMixerCount;
  baseline_.audioMasterVolume = snapshot.audioMasterVolume;
  baseline_.audioMasterMuted = snapshot.audioMasterMuted;

  baseline_.sehExceptionCount = snapshot.sehExceptionCount;
  baseline_.unhandledExceptionCount = snapshot.unhandledExceptionCount;
  baseline_.accessViolationCount = snapshot.accessViolationCount;
  baseline_.heapCorruptionDetected = snapshot.heapCorruptionDetected;

  std::sort(snapshot.processes.begin(), snapshot.processes.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
    if (left.cpuPct == right.cpuPct) {
      return left.ramBytes > right.ramBytes;
    }
    return left.cpuPct > right.cpuPct;
  });

  previousProcessSamples_ = std::move(currentSamples);
  lastNativeSampleQpc_ = nowQpc;
  nativeBaselineReady_ = true;

  if (stackCanary1 != 0xDEADBEEFCAFE1234ULL || stackCanary2 != 0x1234567890ABCDEFULL) {
    HANDLE h = CreateFileA(internal::GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "STACK CANARY CORRUPTED at POLL:RETURN TID=%lu\n",
        (unsigned long)GetCurrentThreadId());
      DWORD written = 0;
      WriteFile(h, buf, len, &written, nullptr);
      CloseHandle(h);
    }
  }
  internal::HeapOk("POLL:RETURN");
  g_phase = "POLL:RETURN";
  return *snapshotPtr;
}

std::vector<ProcessInfo> MonixApp::BuildFallbackProcesses(const Snapshot& snapshot) {
  HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (processSnapshot == INVALID_HANDLE_VALUE) {
    return {};
  }

  std::vector<ProcessInfo> results;
  PROCESSENTRY32W entry {};
  entry.dwSize = sizeof(entry);

  if (Process32FirstW(processSnapshot, &entry)) {
    do {
      const std::wstring name = entry.szExeFile;
      if (name.empty() || name == L"Idle" || name == L"System Idle Process") {
        continue;
      }

      ProcessInfo process;
      process.name = name;
      process.pid = static_cast<int>(entry.th32ProcessID);
      process.status = L"ACTIVE";
      process.priority = L"NORMAL";

      HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
      if (handle) {
        PROCESS_MEMORY_COUNTERS_EX counters {};
        if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
          process.ramBytes = counters.WorkingSetSize;
        }

        const DWORD priority = GetPriorityClass(handle);
        switch (priority) {
          case HIGH_PRIORITY_CLASS: process.priority = L"HIGH"; break;
          case IDLE_PRIORITY_CLASS: process.priority = L"LOW"; break;
          case REALTIME_PRIORITY_CLASS: process.priority = L"REALTIME"; break;
          case BELOW_NORMAL_PRIORITY_CLASS: process.priority = L"BELOW"; break;
          case ABOVE_NORMAL_PRIORITY_CLASS: process.priority = L"ABOVE"; break;
          default: process.priority = L"NORMAL"; break;
        }
        CloseHandle(handle);
      }

      const std::wstring lowered = ToUpper(name);
      if (lowered.find(L"GAME") != std::wstring::npos || lowered.find(L"MONIX") != std::wstring::npos || lowered.find(L"CHATGPT") != std::wstring::npos) {
        process.status = L"RUNNING";
      } else if (lowered.find(L"EXPLORER") != std::wstring::npos) {
        process.status = L"SYSTEM";
      } else if (lowered.find(L"SYSTEM") != std::wstring::npos || lowered.find(L"SVCHOST") != std::wstring::npos) {
        process.status = L"KERNEL";
      } else if (lowered.find(L"OBS") != std::wstring::npos) {
        process.status = L"RECORDING";
      }

      results.push_back(process);
    } while (Process32NextW(processSnapshot, &entry));
  }

  CloseHandle(processSnapshot);
  std::sort(results.begin(), results.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
    return left.ramBytes > right.ramBytes;
  });

  if (results.size() > 18) {
    results.resize(18);
  }

  const std::array<double, 18> cpuWeights { 0.32, 0.18, 0.12, 0.10, 0.08, 0.06, 0.05, 0.04, 0.03, 0.02, 0.015, 0.012, 0.010, 0.008, 0.006, 0.005, 0.004, 0.003 };
  const std::array<double, 18> gpuWeights { 0.48, 0.18, 0.12, 0.08, 0.05, 0.04, 0.025, 0.018, 0.012, 0.010, 0.008, 0.006, 0.005, 0.004, 0.003, 0.003, 0.002, 0.002 };
  for (std::size_t i = 0; i < results.size(); ++i) {
    results[i].cpuPct = std::clamp(snapshot.cpuPct * cpuWeights[i], 0.0, 100.0);
    results[i].gpuPct = std::clamp(snapshot.gpuPct * gpuWeights[i], 0.0, 100.0);
  }

  return results;
}
