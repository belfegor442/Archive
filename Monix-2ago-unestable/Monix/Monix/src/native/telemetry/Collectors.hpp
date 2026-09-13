#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>

#include "Snapshot.hpp"

#include "collectors/network/NetworkCollectors.hpp"
#include "collectors/sys/SchedulerCollector.hpp"
#include "collectors/sys/OsKernelCollector.hpp"
#include "collectors/security/SecurityCollector.hpp"
#include "collectors/power/PowerCollector.hpp"
#include "collectors/thermal/ThermalCollector.hpp"
#include "collectors/hw/HardwareBoardCollector.hpp"
#include "collectors/fs/FilesystemCollector.hpp"
#include "collectors/fs/RegistryCollector.hpp"
#include "collectors/audio/AudioCollector.hpp"
#include "collectors/sys/ReliabilityCollector.hpp"
#include "collectors/gpu/GpuDisplayCollector.hpp"

namespace monix {

extern std::atomic<int> g_sehExceptionCount;
extern std::atomic<int> g_unhandledExceptionCount;
extern std::atomic<int> g_accessViolationCount;
extern std::atomic<int> g_heapCorruptionDetected;
extern std::atomic<int> g_assertionFailureCount;
extern std::atomic<int> g_stackOverflowCount;

int IcmpPingGateway();
int ProbeDnsResolution();
std::uint64_t ComputeRouteTableHash();
void CollectSchedulerData(Snapshot& snapshot);
void CollectOsKernelData(Snapshot& snapshot);
void CollectSecurityData(Snapshot& snapshot);
void CollectPowerData(Snapshot& snapshot);
void CollectThermalData(Snapshot& snapshot);
void CollectHardwareBoardData(Snapshot& snapshot);
void ScanDirectory(const wchar_t* path, int& totalFiles, int& hiddenFiles, int& systemFiles, int& reparsePoints, int& sparseFiles);
int CountAlternateDataStreams(const wchar_t* filePath);
void CollectFilesystemData(Snapshot& snapshot);
unsigned long HashRegistryValues(HKEY hKey);
void CollectRegistryData(Snapshot& snapshot);
void CollectAudioData(Snapshot& snapshot);
unsigned long HashProcessList();
unsigned long HashModuleList(DWORD processId);
void CollectReliabilityData(Snapshot& snapshot);
void CollectGpuDisplayInfo(Snapshot& snapshot);
void CollectNetworkDiagnostics(Snapshot& snapshot);
std::uint64_t CountTcpTeardownStates();

}
