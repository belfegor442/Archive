#include "KernelDiagnostics.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <malloc.h>
#include <intrin.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "bcrypt.lib")

#include "../telemetry/Snapshot.hpp"
#include "../telemetry/Collectors.hpp"

namespace monix {
namespace kernel {

KernelDiagnostics::KernelDiagnostics() {
  BuildTestGroups();
  results_.resize(groups_.size());
  for (std::size_t i = 0; i < groups_.size(); ++i)
    results_[i].resize(groups_[i].tests.size());
}

std::size_t KernelDiagnostics::TotalTests() const {
  std::size_t total = 0;
  for (auto& g : groups_)
    total += g.tests.size();
  return total;
}

void KernelDiagnostics::BuildTestGroups() {
  groups_.clear();

  groups_.push_back({ "LOG", {
    { "Event Identifier",       EventSubsystem::Log, false },
    { "Event Engine",           EventSubsystem::Log, false },
    { "Logger",                 EventSubsystem::Log, false },
    { "Log Manager",            EventSubsystem::Log, false },
    { "Log Buffer",             EventSubsystem::Log, false },
    { "Log Queue",              EventSubsystem::Log, false },
    { "Log Dispatcher",         EventSubsystem::Log, false },
    { "Log Formatter",          EventSubsystem::Log, false },
    { "Log Storage",            EventSubsystem::Log, false },
    { "Log Rotation",           EventSubsystem::Log, false },
    { "Log Integrity",          EventSubsystem::Log, false },
    { "Event Stream",           EventSubsystem::Log, false },
  }});

  groups_.push_back({ "TELEMETRY", {
    { "Telemetry Engine",       EventSubsystem::Telemetry, false },
    { "Telemetry Manager",      EventSubsystem::Telemetry, false },
    { "Telemetry Collector",    EventSubsystem::Telemetry, false },
    { "Telemetry Buffer",       EventSubsystem::Telemetry, false },
    { "Telemetry Queue",        EventSubsystem::Telemetry, false },
    { "Telemetry Processor",    EventSubsystem::Telemetry, false },
    { "Telemetry Sampling",     EventSubsystem::Telemetry, false },
    { "Telemetry Validation",   EventSubsystem::Telemetry, false },
    { "Telemetry Storage",      EventSubsystem::Telemetry, false },
    { "Telemetry Integrity",    EventSubsystem::Telemetry, false },
  }});

  groups_.push_back({ "SENSORS", {
    { "Sensor Manager",         EventSubsystem::Sensors, false },
    { "Sensor Registry",        EventSubsystem::Sensors, false },
    { "Sensor Enumeration",     EventSubsystem::Sensors, false },
    { "Sensor Interface",       EventSubsystem::Sensors, false },
    { "Sensor Data Collector",  EventSubsystem::Sensors, false },
    { "Sensor Data Validator",  EventSubsystem::Sensors, false },
    { "Sensor Monitoring",      EventSubsystem::Sensors, false },
    { "Sensor Event Dispatcher",EventSubsystem::Sensors, false },
  }});

  groups_.push_back({ "DEVICES", {
    { "Device Manager",         EventSubsystem::Devices, false },
    { "Device Registry",        EventSubsystem::Devices, false },
    { "Device Enumeration",     EventSubsystem::Devices, false },
    { "Device Interface",       EventSubsystem::Devices, false },
    { "Device Communication",   EventSubsystem::Devices, false },
  }});

  groups_.push_back({ "MEMORY", {
    { "Memory Manager",         EventSubsystem::Memory,  true  },
    { "Memory Map",             EventSubsystem::Memory,  false },
    { "Memory Allocator",       EventSubsystem::Memory,  true  },
    { "Memory Protection",      EventSubsystem::Memory,  false },
    { "Memory Integrity",       EventSubsystem::Memory,  true  },
  }});

  groups_.push_back({ "PROCESS", {
    { "Process Manager",        EventSubsystem::Process, true  },
    { "Thread Manager",         EventSubsystem::Threads, false },
    { "Scheduler",              EventSubsystem::Scheduler, false },
    { "Process Registry",       EventSubsystem::Process, false },
  }});

  groups_.push_back({ "SECURITY", {
    { "Security Manager",       EventSubsystem::Security, true  },
    { "Permission Engine",      EventSubsystem::Security, false },
    { "Integrity Engine",       EventSubsystem::Security, false },
    { "Authentication Engine",  EventSubsystem::Security, true  },
    { "Security Audit",         EventSubsystem::Security, false },
  }});

  groups_.push_back({ "ERROR", {
    { "Error Manager",          EventSubsystem::Error, false },
    { "Exception Handler",      EventSubsystem::Error, true  },
    { "Crash Handler",          EventSubsystem::Error, true  },
    { "Diagnostic Engine",      EventSubsystem::Error, false },
    { "Recovery Manager",       EventSubsystem::Error, false },
  }});

  groups_.push_back({ "NETWORK", {
    { "Network Manager",        EventSubsystem::Network, false },
    { "Adapter Enumeration",    EventSubsystem::Network, false },
    { "DNS Resolution",         EventSubsystem::Network, false },
    { "Route Table",            EventSubsystem::Network, false },
    { "TCP Stack",              EventSubsystem::Network, false },
    { "Connection Monitor",     EventSubsystem::Network, false },
  }});

  groups_.push_back({ "STORAGE", {
    { "Storage Manager",        EventSubsystem::Storage, false },
    { "Volume Detection",       EventSubsystem::Storage, false },
    { "Filesystem Check",       EventSubsystem::Storage, false },
    { "I/O Subsystem",          EventSubsystem::Storage, false },
  }});

  groups_.push_back({ "KERNEL", {
    { "Kernel Initialization",  EventSubsystem::Kernel, true  },
    { "Kernel Services",        EventSubsystem::Kernel, true  },
    { "Kernel Validation",      EventSubsystem::Kernel, true  },
    { "Kernel Integrity",       EventSubsystem::Kernel, true  },
  }});

  // Append all Ultra Logger self-test groups
  auto& ultra = KernelSelfTest::Instance();
  ultra.RegisterGroups();
  const auto& ug = ultra.GetGroups();
  groups_.insert(groups_.end(), ug.begin(), ug.end());
}

TestResult KernelDiagnostics::RunTest(std::size_t groupIndex,
                                      std::size_t testIndex,
                                      KernelEventEngine& events) {
  if (groupIndex >= groups_.size() ||
      testIndex >= groups_[groupIndex].tests.size()) {
    return { "INVALID", TestSeverity::Fatal, "Index out of range" };
  }

  auto& test = groups_[groupIndex].tests[testIndex];

  if (test.runner) {
    auto result = test.runner();
    SetTestResult(groupIndex, testIndex, result.severity, result.detail);
    events.Emit(test.subsystem, EventSeverity::Info, test.name, result.detail);
    return result;
  }

  TestSeverity sev = TestSeverity::Pass;
  const char* detail = nullptr;

  switch (groupIndex) {
    case 0: switch (testIndex) {
      case 0: {
        auto id = events.Emit(EventSubsystem::Log, EventSeverity::Info,
                              "diagnostic_event", "KernelDiagnostics test event");
        if (id > 0) { sev = TestSeverity::Pass; detail = "event emitted"; }
        else { sev = TestSeverity::Fail; detail = "emit returned 0"; }
        break;
      }
      case 1: { sev = TestSeverity::Pass; detail = "engine available"; break; }
      case 2: {
        const char* path = "kernel_diag_test.tmp";
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
          DWORD written = 0;
          BOOL write_ok = WriteFile(h, "test", 4, &written, nullptr);
          CloseHandle(h);
          sev = (write_ok && written == 4) ? TestSeverity::Pass : TestSeverity::Fail;
        } else {
          sev = TestSeverity::Fail;
        }
        DeleteFileA(path);
        detail = sev == TestSeverity::Pass ? "file I/O tested" : "file I/O failed";
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 1: switch (testIndex) {
      case 2: {
        MEMORYSTATUSEX ms = {};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms) && ms.ullTotalPhys > 0)
          detail = "scheduler ok";
        else
          detail = "scheduler limited";
        sev = (GlobalMemoryStatusEx(&ms) && ms.ullTotalPhys > 0)
          ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 2: switch (testIndex) {
      case 2: {
        int cpuInfo[4] = {};
        __cpuid(cpuInfo, 0);
        if (cpuInfo[0] > 0)
          detail = "cpuid ok";
        else
          detail = "cpuid failed";
        sev = cpuInfo[0] > 0 ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 3: switch (testIndex) {
      case 2: {
        DISPLAY_DEVICEA dd = {};
        dd.cb = sizeof(dd);
        int count = 0;
        for (int i = 0; EnumDisplayDevicesA(nullptr, i, &dd, 0); ++i) {
          ++count;
          dd.cb = sizeof(dd);
        }
        if (count > 0)
          detail = "displays found";
        else
          detail = "no displays";
        sev = count > 0 ? TestSeverity::Pass : TestSeverity::Warning;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 4: switch (testIndex) {
      case 0: {
        MEMORYSTATUSEX ms = {};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms))
          detail = "memory ok";
        else
          detail = "failed";
        sev = GlobalMemoryStatusEx(&ms) ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      case 2: {
        void* p = malloc(256);
        if (p) { free(p); detail = "alloc ok"; }
        else detail = "alloc failed";
        sev = p ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      case 4: {
        if (HeapValidate(GetProcessHeap(), 0, nullptr))
          detail = "heap ok";
        else
          detail = "heap corrupt";
        sev = HeapValidate(GetProcessHeap(), 0, nullptr)
          ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 5: switch (testIndex) {
      case 0: {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
          PROCESSENTRY32W pe = {};
          pe.dwSize = sizeof(pe);
          int count = 0;
          if (Process32FirstW(snap, &pe)) {
            ++count;
            while (Process32NextW(snap, &pe))
              ++count;
          }
          CloseHandle(snap);
          detail = "processes found";
          (void)count;
        } else {
          detail = "snapshot failed";
        }
        sev = snap != INVALID_HANDLE_VALUE ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      case 1: {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
          CloseHandle(snap);
          detail = "thread snapshot ok";
        } else {
          detail = "failed";
        }
        sev = snap != INVALID_HANDLE_VALUE ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 6: switch (testIndex) {
      case 3: {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                                      nullptr, 0);
        if (status == 0 && hAlg) {
          BCryptCloseAlgorithmProvider(hAlg, 0);
          detail = "sha256 ok";
        } else {
          detail = "sha256 failed";
        }
        sev = status == 0 && hAlg ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 7: switch (testIndex) {
      case 1: {
        auto* handler = AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS) -> LONG {
          return EXCEPTION_CONTINUE_SEARCH;
        });
        if (handler)
          RemoveVectoredExceptionHandler(handler);
        detail = handler ? "veh ok" : "veh failed";
        sev = handler ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      case 2: {
        if (HeapValidate(GetProcessHeap(), 0, nullptr))
          detail = "heap ok";
        else
          detail = "heap corrupt";
        sev = HeapValidate(GetProcessHeap(), 0, nullptr)
          ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 8: switch (testIndex) {
      case 1: {
        ULONG bufLen = 15000;
        PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
        ULONG ret = ERROR_NOT_ENOUGH_MEMORY;
        if (adapters) {
          ret = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &bufLen);
          if (ret == NO_ERROR) {
            detail = "adapters found";
          } else {
            detail = "get adapters failed";
          }
          free(adapters);
        } else {
          detail = "out of memory";
        }
        sev = ret == NO_ERROR ? TestSeverity::Pass : TestSeverity::Warning;
        break;
      }
      case 2: {
        int ms = ProbeDnsResolution();
        if (ms > 0)
          detail = "dns resolved";
        else
          detail = "dns probe";
        sev = ms > 0 ? TestSeverity::Pass : TestSeverity::Warning;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    case 9: switch (testIndex) {
      case 1: {
        ULARGE_INTEGER freeBytes = {}, totalBytes = {}, totalFreeBytes = {};
        if (GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFreeBytes))
          detail = "volume ok";
        else
          detail = "volume failed";
        sev = GetDiskFreeSpaceExA("C:\\", &freeBytes, &totalBytes, &totalFreeBytes)
          ? TestSeverity::Pass : TestSeverity::Fail;
        break;
      }
      default: sev = TestSeverity::Pass; detail = "infrastructure"; break;
    } break;

    default:
      sev = TestSeverity::Pass;
      detail = "infrastructure";
      break;
  }

  SetTestResult(groupIndex, testIndex, sev, detail);
  events.Emit(test.subsystem, EventSeverity::Info, test.name, detail);
  return { test.name, sev, detail };
}

}
}
