#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdint>

#include "Snapshot.hpp"

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
std::uint64_t CountTcpResets();

}
