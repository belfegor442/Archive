#include "EnvironmentCollector.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <mmsystem.h>
#include <lm.h>
#include <winsvc.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")

namespace monix {

EnvironmentCollector::EnvironmentCollector() {
  InitializeCom();
  initialized_ = true;
}

EnvironmentCollector::~EnvironmentCollector() {
  UninitializeCom();
}

void EnvironmentCollector::InitializeCom() {
  if (comInitialized_) return;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  comInitialized_ = SUCCEEDED(hr);
}

void EnvironmentCollector::UninitializeCom() {
  if (comInitialized_) {
    CoUninitialize();
    comInitialized_ = false;
  }
}

void EnvironmentCollector::CollectFirmware(FirmwareEvidence& out) {
  out.state = DataState::Valid;

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    wchar_t buf[256]{};
    DWORD bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"BIOSVendor", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.biosVendor = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"BIOSVersion", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.biosVersion = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"BIOSReleaseDate", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.biosReleaseDate = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"SystemManufacturer", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.systemManufacturer = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"SystemProductName", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.systemProductName = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"SystemSerialNumber", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.systemSerialNumber = buf;
    }
    DWORD biosMajor = 0, biosMinor = 0;
    bufSize = sizeof(biosMajor);
    RegQueryValueExW(hKey, L"BiosMajorRelease", nullptr, nullptr, (LPBYTE)&biosMajor, &bufSize);
    bufSize = sizeof(biosMinor);
    RegQueryValueExW(hKey, L"BiosMinorRelease", nullptr, nullptr, (LPBYTE)&biosMinor, &bufSize);
    out.biosMajorVer = static_cast<std::uint16_t>(biosMajor);
    out.biosMinorVer = static_cast<std::uint16_t>(biosMinor);
    RegCloseKey(hKey);
  }

  HKEY hTpm = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Services\\TPM\\WMI", 0, KEY_READ, &hTpm) == ERROR_SUCCESS) {
    DWORD tpmVer = 0;
    DWORD bufSize = sizeof(tpmVer);
    if (RegQueryValueExW(hTpm, L"TPMVersion", nullptr, nullptr, (LPBYTE)&tpmVer, &bufSize) == ERROR_SUCCESS) {
      out.tpmPresent = 1;
      out.tpmReady = 1;
      out.tpmVersion = tpmVer;
    }
    RegCloseKey(hTpm);
  }

  HKEY hTpm2 = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\CI", 0, KEY_READ, &hTpm2) == ERROR_SUCCESS) {
    DWORD hvci = 0;
    DWORD bufSize = sizeof(hvci);
    if (RegQueryValueExW(hTpm2, L"VulnerableKernelBlocklistEnabled", nullptr, nullptr, (LPBYTE)&hvci, &bufSize) == ERROR_SUCCESS) {
      out.platformSecurityDeviceEnabled = hvci;
    }
    RegCloseKey(hTpm2);
  }

  HKEY hSecureBoot = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", 0, KEY_READ, &hSecureBoot) == ERROR_SUCCESS) {
    DWORD sbState = 0;
    DWORD bufSize = sizeof(sbState);
    if (RegQueryValueExW(hSecureBoot, L"UEFISecureBootEnabled", nullptr, nullptr, (LPBYTE)&sbState, &bufSize) == ERROR_SUCCESS) {
      out.secureBootState = static_cast<int>(sbState);
    }
    RegCloseKey(hSecureBoot);
  }

  if (out.tpmPresent == 0) {
    HKEY hTpmBase = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\TPM", 0, KEY_READ, &hTpmBase) == ERROR_SUCCESS) {
      DWORD startType = 0;
      DWORD bufSize = sizeof(startType);
      if (RegQueryValueExW(hTpmBase, L"Start", nullptr, nullptr, (LPBYTE)&startType, &bufSize) == ERROR_SUCCESS) {
        out.tpmPresent = (startType != 4) ? 1 : 0;
      }
      RegCloseKey(hTpmBase);
    }
  }
}

void EnvironmentCollector::CollectSoftwareEnvironment(SoftwareEnvironmentEvidence& out) {
  out.state = DataState::Valid;
  out.majorVersion = 0;
  out.minorVersion = 0;
  out.buildNumber = 0;
  out.platformId = 0;

  OSVERSIONINFOEXW osvi{};
  osvi.dwOSVersionInfoSize = sizeof(osvi);

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    wchar_t buf[512]{};
    DWORD bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"ProductName", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.osEdition = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"DisplayVersion", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.osVersion = buf;
    }
    bufSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"CurrentBuildNumber", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.osBuild = buf;
      out.buildNumber = _wtoi(buf);
    }
    DWORD majorVer = 0, minorVer = 0;
    bufSize = sizeof(majorVer);
    RegQueryValueExW(hKey, L"CurrentMajorNumber", nullptr, nullptr, (LPBYTE)&majorVer, &bufSize);
    bufSize = sizeof(minorVer);
    RegQueryValueExW(hKey, L"CurrentMinorNumber", nullptr, nullptr, (LPBYTE)&minorVer, &bufSize);
    out.majorVersion = majorVer;
    out.minorVersion = minorVer;
    RegCloseKey(hKey);
  }

  HKEY hKernel = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKernel) == ERROR_SUCCESS) {
    wchar_t buf[256]{};
    DWORD bufSize = sizeof(buf);
    if (RegQueryValueExW(hKernel, L"KernelVersion", nullptr, nullptr, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
      out.kernelVersion = buf;
    }
    RegCloseKey(hKernel);
  }

  out.systemUptimeMs = GetTickCount64();
  DWORD sessionId = 0;
  ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
  out.currentSessionId = static_cast<int>(sessionId);
}

void EnvironmentCollector::CollectSecurityState(SecurityStateEvidence& out) {
  out.state = DataState::Valid;

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hKey, L"DisableRealtimeMonitoring", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.defenderRealTimeProtection = val == 0 ? 1 : 0;
    }
    bufSize = sizeof(val);
    if (RegQueryValueExW(hKey, L"DisableBehaviorMonitoring", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.defenderBehaviorMonitoring = val == 0 ? 1 : 0;
    }
    bufSize = sizeof(val);
    if (RegQueryValueExW(hKey, L"DisableOnAccessProtection", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.defenderOnAccessProtection = val == 0 ? 1 : 0;
    }
    RegCloseKey(hKey);
  }

  HKEY hTamper = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows Defender\\Windows Defender", 0, KEY_READ, &hTamper) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hTamper, L"DisableAntiSpyware", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.defenderAntivirusEnabled = val == 0 ? 1 : 0;
    }
    RegCloseKey(hTamper);
  }

  HKEY hUac = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_READ, &hUac) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hUac, L"EnableLUA", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.uacEnabled = static_cast<int>(val);
    }
    bufSize = sizeof(val);
    if (RegQueryValueExW(hUac, L"ConsentPromptBehaviorAdmin", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.uacConsentPromptBehavior = static_cast<int>(val);
    }
    RegCloseKey(hUac);
  }

  HKEY hFirewall = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile", 0, KEY_READ, &hFirewall) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hFirewall, L"EnableFirewall", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.firewallEnabled = static_cast<int>(val);
    }
    RegCloseKey(hFirewall);
  }

  HKEY hCI = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\CI", 0, KEY_READ, &hCI) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hCI, L"VerifiedAndReputablePolicyState", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.codeIntegrityEnabled = (val & 1) ? 1 : 0;
    }
    RegCloseKey(hCI);
  }

  HKEY hDebug = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control", 0, KEY_READ, &hDebug) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hDebug, L"SystemStartOptions", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.debugModeEnabled = (val & 0x1) ? 1 : 0;
    }
    RegCloseKey(hDebug);
  }

  HKEY hSecureBoot = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", 0, KEY_READ, &hSecureBoot) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hSecureBoot, L"UEFISecureBootEnabled", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.secureBootEnabled = static_cast<int>(val);
    }
    RegCloseKey(hSecureBoot);
  }

  HKEY hHvci = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity", 0, KEY_READ, &hHvci) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hHvci, L"Enabled", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.hvciEnabled = static_cast<int>(val);
    }
    RegCloseKey(hHvci);
  }

  HKEY hTestSigning = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\CI", 0, KEY_READ, &hTestSigning) == ERROR_SUCCESS) {
    DWORD val = 0;
    DWORD bufSize = sizeof(val);
    if (RegQueryValueExW(hTestSigning, L"TestSigned", nullptr, nullptr, (LPBYTE)&val, &bufSize) == ERROR_SUCCESS) {
      out.testSigningEnabled = static_cast<int>(val);
    }
    RegCloseKey(hTestSigning);
  }
}

void EnvironmentCollector::CollectDrivers(std::vector<DriverEvidence>& out) {
  out.clear();
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return;

  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);
  if (Module32FirstW(hSnap, &me)) {
    do {
      DriverEvidence drv;
      drv.name = me.szModule;
      drv.path = me.szExePath;
      drv.fileSize = me.modBaseSize;
      drv.state = DataState::Valid;
      out.push_back(std::move(drv));
    } while (Module32NextW(hSnap, &me));
  }
  CloseHandle(hSnap);

  std::set<std::wstring> seenNames;
  std::vector<DriverEvidence> unique;
  for (auto& drv : out) {
    std::wstring lower = drv.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (seenNames.find(lower) == seenNames.end()) {
      seenNames.insert(lower);
      unique.push_back(std::move(drv));
    }
  }
  out = std::move(unique);
}

void EnvironmentCollector::CollectProcesses(std::vector<ProcessEvidence>& out) {
  out.clear();
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return;

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(hSnap, &pe)) {
    do {
      ProcessEvidence proc;
      proc.name = pe.szExeFile;
      proc.pid = static_cast<int>(pe.th32ProcessID);
      proc.parentPid = static_cast<int>(pe.th32ParentProcessID);
      proc.state = DataState::Valid;

      HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                  FALSE, pe.th32ProcessID);
      if (hProc) {
        wchar_t pathBuf[MAX_PATH]{};
        DWORD pathSize = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathSize)) {
          proc.executablePath = pathBuf;
        }

        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
          proc.ramBytes = pmc.WorkingSetSize;
        }

        FILETIME createTime{}, exitTime{}, kernelTime{}, userTime{};
        if (GetProcessTimes(hProc, &createTime, &exitTime, &kernelTime, &userTime)) {
          ULARGE_INTEGER ct{};
          ct.LowPart = createTime.dwLowDateTime;
          ct.HighPart = createTime.dwHighDateTime;
          proc.createTime100ns = ct.QuadPart;
        }

        DWORD integrityLevel = 0;
        HANDLE hToken = nullptr;
        if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
          DWORD tokenInfoLen = 0;
          GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &tokenInfoLen);
          if (tokenInfoLen > 0) {
            std::vector<BYTE> tokenInfo(tokenInfoLen);
            if (GetTokenInformation(hToken, TokenIntegrityLevel, tokenInfo.data(),
                tokenInfoLen, &tokenInfoLen)) {
              auto* sidAttr = reinterpret_cast<PTOKEN_MANDATORY_LABEL>(tokenInfo.data());
              DWORD subAuthCount = *GetSidSubAuthorityCount(sidAttr->Label.Sid);
              if (subAuthCount > 0) {
                integrityLevel = *GetSidSubAuthority(sidAttr->Label.Sid, subAuthCount - 1);
              }
            }
          }
          CloseHandle(hToken);
        }
        proc.integrityLevel = integrityLevel;
        if (integrityLevel >= 0x3000) proc.integrityLevelName = L"SYSTEM";
        else if (integrityLevel >= 0x2000) proc.integrityLevelName = L"HIGH";
        else if (integrityLevel >= 0x1000) proc.integrityLevelName = L"MEDIUM";
        else proc.integrityLevelName = L"LOW";

        DWORD sessionId = 0;
        if (ProcessIdToSessionId(pe.th32ProcessID, &sessionId)) {
          proc.sessionId = static_cast<int>(sessionId);
        }

        CloseHandle(hProc);
      }

      out.push_back(std::move(proc));
    } while (Process32NextW(hSnap, &pe));
  }
  CloseHandle(hSnap);
}

void EnvironmentCollector::CollectServices(std::vector<ServiceEvidence>& out) {
  out.clear();
  SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
  if (!hSCM) return;

  DWORD bytesNeeded = 0, servicesReturned = 0, resumeHandle = 0;
  EnumServicesStatusExW(hSCM, (SC_ENUM_TYPE)0, SERVICE_WIN32, SERVICE_STATE_ALL,
    nullptr, 0, &bytesNeeded, &servicesReturned, &resumeHandle, nullptr);

  if (bytesNeeded == 0) {
    CloseServiceHandle(hSCM);
    return;
  }

  std::vector<BYTE> buffer(bytesNeeded);
  if (EnumServicesStatusExW(hSCM, (SC_ENUM_TYPE)0, SERVICE_WIN32, SERVICE_STATE_ALL,
      buffer.data(), bytesNeeded, &bytesNeeded, &servicesReturned, &resumeHandle, nullptr)) {
    auto* services = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
    for (DWORD i = 0; i < servicesReturned; ++i) {
      ServiceEvidence svc;
      svc.name = services[i].lpServiceName;
      svc.displayName = services[i].lpDisplayName;
      svc.currentState = services[i].ServiceStatusProcess.dwCurrentState;
      svc.serviceType = services[i].ServiceStatusProcess.dwServiceType;
      svc.processId = services[i].ServiceStatusProcess.dwProcessId;
      svc.state = DataState::Valid;

      SC_HANDLE hSvc = OpenServiceW(hSCM, services[i].lpServiceName,
                                     SERVICE_QUERY_CONFIG);
      if (hSvc) {
        DWORD configNeeded = 0;
        QueryServiceConfigW(hSvc, nullptr, 0, &configNeeded);
        if (configNeeded > 0) {
          std::vector<BYTE> configBuf(configNeeded);
          if (QueryServiceConfigW(hSvc, reinterpret_cast<QUERY_SERVICE_CONFIGW*>(configBuf.data()),
              configNeeded, &configNeeded)) {
            auto* config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(configBuf.data());
            svc.path = config->lpBinaryPathName;
            svc.startType = config->dwStartType;
            svc.errorControl = config->dwErrorControl;
            svc.accountName = config->lpServiceStartName ? config->lpServiceStartName : L"";
          }
        }
        CloseServiceHandle(hSvc);
      }

      out.push_back(std::move(svc));
    }
  }

  CloseServiceHandle(hSCM);
}

void EnvironmentCollector::CollectStartupItems(std::vector<StartupItemEvidence>& out) {
  out.clear();

  auto collectFromKey = [&](HKEY root, const std::wstring& subkey, const std::wstring& location) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;

    DWORD index = 0;
    wchar_t valueName[256]{};
    DWORD valueNameSize = 256;
    BYTE data[2048]{};
    DWORD dataSize = sizeof(data);
    DWORD type = 0;

    while (RegEnumValueW(hKey, index, valueName, &valueNameSize, nullptr, &type, data, &dataSize) == ERROR_SUCCESS) {
      if (type == REG_SZ || type == REG_EXPAND_SZ) {
        StartupItemEvidence item;
        item.name = valueName;
        item.command = reinterpret_cast<const wchar_t*>(data);
        item.location = location;
        item.isEnabled = true;
        item.state = DataState::Valid;
        out.push_back(std::move(item));
      }
      ++index;
      valueNameSize = 256;
      dataSize = sizeof(data);
    }

    RegCloseKey(hKey);
  };

  collectFromKey(HKEY_CURRENT_USER,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKCU\\Run");
  collectFromKey(HKEY_CURRENT_USER,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", L"HKCU\\RunOnce");
  collectFromKey(HKEY_LOCAL_MACHINE,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", L"HKLM\\Run");
  collectFromKey(HKEY_LOCAL_MACHINE,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", L"HKLM\\RunOnce");
  collectFromKey(HKEY_LOCAL_MACHINE,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunServices", L"HKLM\\RunServices");
  collectFromKey(HKEY_LOCAL_MACHINE,
    L"Software\\Microsoft\\Windows\\CurrentVersion\\RunServicesOnce", L"HKLM\\RunServicesOnce");

  wchar_t startupPath[MAX_PATH]{};
  if (SHGetFolderPathW(nullptr, CSIDL_STARTUP, nullptr, 0, startupPath) == S_OK) {
    WIN32_FIND_DATAW fd{};
    std::wstring searchPath = std::wstring(startupPath) + L"\\*.*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          StartupItemEvidence item;
          item.name = fd.cFileName;
          item.command = std::wstring(startupPath) + L"\\" + fd.cFileName;
          item.location = L"Startup Folder";
          item.isEnabled = true;
          item.state = DataState::Valid;
          out.push_back(std::move(item));
        }
      } while (FindNextFileW(hFind, &fd));
      FindClose(hFind);
    }
  }
}

void EnvironmentCollector::CollectScheduledTasks(std::vector<ScheduledTaskEvidence>& out) {
  out.clear();

  wchar_t tasksPath[MAX_PATH]{};
  if (SHGetFolderPathW(nullptr, CSIDL_WINDOWS, nullptr, 0, tasksPath) != S_OK) return;
  std::wstring tasksDir = std::wstring(tasksPath) + L"\\System32\\Tasks";

  std::vector<std::wstring> dirs;
  dirs.push_back(tasksDir);

  while (!dirs.empty()) {
    std::wstring currentDir = dirs.back();
    dirs.pop_back();

    WIN32_FIND_DATAW fd{};
    std::wstring searchPath = currentDir + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) continue;

    do {
      std::wstring fullPath = currentDir + L"\\" + fd.cFileName;
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (fd.cFileName[0] != L'.') {
          dirs.push_back(fullPath);
        }
      } else {
        ScheduledTaskEvidence task;
        task.path = fullPath;
        size_t pos = fullPath.find(L"\\Tasks\\");
        if (pos != std::wstring::npos) {
          task.name = fullPath.substr(pos + 7);
        } else {
          task.name = fd.cFileName;
        }
        task.isEnabled = true;
        task.state = DataState::Valid;
        out.push_back(std::move(task));
      }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
  }
}

void EnvironmentCollector::CollectPciDevices(std::vector<HardwareDeviceEvidence>& out) {
  out.clear();
  HDEVINFO devInfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, nullptr, nullptr,
    DIGCF_PRESENT);
  if (devInfo == INVALID_HANDLE_VALUE) return;

  SP_DEVINFO_DATA devInfoData{};
  devInfoData.cbSize = sizeof(devInfoData);

  for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i) {
    HardwareDeviceEvidence dev;
    wchar_t buf[512]{};
    DWORD bufSize = sizeof(buf);

    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_DEVICEDESC,
        nullptr, (PBYTE)buf, bufSize, &bufSize)) {
      dev.description = buf;
    }
    bufSize = sizeof(buf);
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_MFG,
        nullptr, (PBYTE)buf, bufSize, &bufSize)) {
      dev.manufacturer = buf;
    }
    bufSize = sizeof(buf);
    if (SetupDiGetDeviceInstanceIdW(devInfo, &devInfoData, buf, bufSize / sizeof(wchar_t), nullptr)) {
      dev.instanceId = buf;
    }
    dev.state = DataState::Valid;
    out.push_back(std::move(dev));
  }

  SetupDiDestroyDeviceInfoList(devInfo);
}

void EnvironmentCollector::CollectUsbDevices(std::vector<HardwareDeviceEvidence>& out) {
  out.clear();
  HDEVINFO devInfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_USB, nullptr, nullptr,
    DIGCF_PRESENT);
  if (devInfo == INVALID_HANDLE_VALUE) return;

  SP_DEVINFO_DATA devInfoData{};
  devInfoData.cbSize = sizeof(devInfoData);

  for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i) {
    HardwareDeviceEvidence dev;
    wchar_t buf[512]{};
    DWORD bufSize = sizeof(buf);

    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_DEVICEDESC,
        nullptr, (PBYTE)buf, bufSize, &bufSize)) {
      dev.description = buf;
    }
    bufSize = sizeof(buf);
    if (SetupDiGetDeviceInstanceIdW(devInfo, &devInfoData, buf, bufSize / sizeof(wchar_t), nullptr)) {
      dev.instanceId = buf;
    }
    dev.state = DataState::Valid;
    out.push_back(std::move(dev));
  }

  SetupDiDestroyDeviceInfoList(devInfo);
}

void EnvironmentCollector::CollectDisplayDevices(std::vector<HardwareDeviceEvidence>& out) {
  out.clear();
  DISPLAY_DEVICEW dd{};
  dd.cb = sizeof(dd);
  int devNum = 0;

  while (EnumDisplayDevicesW(nullptr, devNum, &dd, 0)) {
    HardwareDeviceEvidence dev;
    dev.description = dd.DeviceString;
    dev.name = dd.DeviceName;
    dev.instanceId = dd.DeviceID;
    dev.present = (dd.StateFlags & DISPLAY_DEVICE_ATTACHED) != 0;
    dev.state = DataState::Valid;
    out.push_back(std::move(dev));
    ++devNum;
    dd.cb = sizeof(dd);
  }
}

void EnvironmentCollector::CollectAudioDevices(std::vector<HardwareDeviceEvidence>& out) {
  out.clear();
  UINT waveOutCount = waveOutGetNumDevs();
  for (UINT i = 0; i < waveOutCount; ++i) {
    HardwareDeviceEvidence dev;
    WAVEOUTCAPSW caps{};
    if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
      dev.description = caps.szPname;
      dev.name = L"WaveOut_" + std::to_wstring(i);
      dev.deviceId = caps.wMid;
      dev.vendorId = caps.wPid;
    }
    dev.state = DataState::Valid;
    out.push_back(std::move(dev));
  }

  UINT waveInCount = waveInGetNumDevs();
  for (UINT i = 0; i < waveInCount; ++i) {
    HardwareDeviceEvidence dev;
    WAVEINCAPSW caps{};
    if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
      dev.description = caps.szPname;
      dev.name = L"WaveIn_" + std::to_wstring(i);
      dev.deviceId = caps.wMid;
      dev.vendorId = caps.wPid;
    }
    dev.state = DataState::Valid;
    out.push_back(std::move(dev));
  }
}

void EnvironmentCollector::CollectStorageDevices(std::vector<HardwareDeviceEvidence>& out) {
  out.clear();
  HDEVINFO devInfo = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr,
    DIGCF_PRESENT);
  if (devInfo == INVALID_HANDLE_VALUE) return;

  SP_DEVINFO_DATA devInfoData{};
  devInfoData.cbSize = sizeof(devInfoData);

  for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devInfoData); ++i) {
    HardwareDeviceEvidence dev;
    wchar_t buf[512]{};
    DWORD bufSize = sizeof(buf);

    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_DEVICEDESC,
        nullptr, (PBYTE)buf, bufSize, &bufSize)) {
      dev.description = buf;
    }
    bufSize = sizeof(buf);
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, &devInfoData, SPDRP_MFG,
        nullptr, (PBYTE)buf, bufSize, &bufSize)) {
      dev.manufacturer = buf;
    }
    bufSize = sizeof(buf);
    if (SetupDiGetDeviceInstanceIdW(devInfo, &devInfoData, buf, bufSize / sizeof(wchar_t), nullptr)) {
      dev.instanceId = buf;
    }
    dev.state = DataState::Valid;
    out.push_back(std::move(dev));
  }

  SetupDiDestroyDeviceInfoList(devInfo);
}

void EnvironmentCollector::CollectNetworkInterfaces(std::vector<NetworkInterfaceEvidence>& out) {
  out.clear();

  ULONG outBufLen = 15000;
  PIP_ADAPTER_ADDRESSES pAdapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
  if (!pAdapterAddresses) return;

  DWORD result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX,
    nullptr, pAdapterAddresses, &outBufLen);
  if (result != ERROR_SUCCESS) {
    free(pAdapterAddresses);
    return;
  }

  for (PIP_ADAPTER_ADDRESSES pAdapter = pAdapterAddresses; pAdapter; pAdapter = pAdapter->Next) {
    if (pAdapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

    NetworkInterfaceEvidence iface;
    if (pAdapter->AdapterName) {
      int wlen = MultiByteToWideChar(CP_UTF8, 0, pAdapter->AdapterName, -1, nullptr, 0);
      if (wlen > 0) {
        std::wstring wname(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, pAdapter->AdapterName, -1, &wname[0], wlen);
        wname.resize(wlen - 1);
        iface.name = wname;
      }
    }
    if (pAdapter->Description) {
      iface.description = pAdapter->Description;
    }
    iface.operStatus = pAdapter->OperStatus;
    iface.type = pAdapter->IfType;
    iface.speed = pAdapter->TransmitLinkSpeed;
    iface.isAdapterEnabled = (pAdapter->OperStatus != IfOperStatusDown);
    iface.isVpn = (pAdapter->IfType == IF_TYPE_TUNNEL);
    iface.isTunnel = (pAdapter->IfType == IF_TYPE_TUNNEL);
    iface.isLoopback = false;

    if (pAdapter->PhysicalAddressLength > 0) {
      wchar_t mac[18]{};
      swprintf_s(mac, L"%02X:%02X:%02X:%02X:%02X:%02X",
        pAdapter->PhysicalAddress[0], pAdapter->PhysicalAddress[1],
        pAdapter->PhysicalAddress[2], pAdapter->PhysicalAddress[3],
        pAdapter->PhysicalAddress[4], pAdapter->PhysicalAddress[5]);
      iface.macAddress = mac;
    }

    for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pAdapter->FirstUnicastAddress;
         pUnicast; pUnicast = pUnicast->Next) {
      char addrBuf[INET6_ADDRSTRLEN]{};
      if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr)->sin_addr,
          addrBuf, sizeof(addrBuf));
        wchar_t waddr[INET6_ADDRSTRLEN]{};
        MultiByteToWideChar(CP_UTF8, 0, addrBuf, -1, waddr, INET6_ADDRSTRLEN);
        iface.ipv4Addresses.insert(waddr);
      } else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(pUnicast->Address.lpSockaddr)->sin6_addr,
          addrBuf, sizeof(addrBuf));
        wchar_t waddr[INET6_ADDRSTRLEN]{};
        MultiByteToWideChar(CP_UTF8, 0, addrBuf, -1, waddr, INET6_ADDRSTRLEN);
        iface.ipv6Addresses.insert(waddr);
      }
    }

    for (PIP_ADAPTER_GATEWAY_ADDRESS pGateway = pAdapter->FirstGatewayAddress;
         pGateway; pGateway = pGateway->Next) {
      char addrBuf[INET6_ADDRSTRLEN]{};
      if (pGateway->Address.lpSockaddr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(pGateway->Address.lpSockaddr)->sin_addr,
          addrBuf, sizeof(addrBuf));
        wchar_t waddr[INET6_ADDRSTRLEN]{};
        MultiByteToWideChar(CP_UTF8, 0, addrBuf, -1, waddr, INET6_ADDRSTRLEN);
        iface.gateways.insert(waddr);
      }
    }

    for (PIP_ADAPTER_DNS_SERVER_ADDRESS pDns = pAdapter->FirstDnsServerAddress;
         pDns; pDns = pDns->Next) {
      char addrBuf[INET6_ADDRSTRLEN]{};
      if (pDns->Address.lpSockaddr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(pDns->Address.lpSockaddr)->sin_addr,
          addrBuf, sizeof(addrBuf));
        wchar_t waddr[INET6_ADDRSTRLEN]{};
        MultiByteToWideChar(CP_UTF8, 0, addrBuf, -1, waddr, INET6_ADDRSTRLEN);
        iface.dnsServers.insert(waddr);
      }
    }

    iface.isDhcpEnabled = (pAdapter->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
    iface.state = DataState::Valid;
    out.push_back(std::move(iface));
  }

  free(pAdapterAddresses);
}

void EnvironmentCollector::CollectVolumes(std::vector<VolumeEvidence>& out) {
  out.clear();

  wchar_t drives[1024]{};
  DWORD driveLen = GetLogicalDriveStringsW(sizeof(drives) / sizeof(wchar_t), drives);
  if (driveLen == 0) return;

  for (wchar_t* drive = drives; *drive; drive += wcslen(drive) + 1) {
    VolumeEvidence vol;
    vol.mountPoint = drive;

    wchar_t label[256]{};
    wchar_t fileSystem[256]{};
    DWORD serial = 0;
    DWORD maxComponent = 0;
    DWORD flags = 0;
    if (GetVolumeInformationW(drive, label, sizeof(label) / sizeof(wchar_t),
        &serial, &maxComponent, &flags, fileSystem, sizeof(fileSystem) / sizeof(wchar_t))) {
      vol.label = label;
      vol.fileSystem = fileSystem;
      vol.serialNumber = serial;
      vol.maxComponentLength = maxComponent;
      vol.fileSystemFlags = flags;
      vol.isDirty = false;
      vol.isReadOnly = (flags & FILE_READ_ONLY_VOLUME) != 0;
    }

    ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFreeBytes{};
    if (GetDiskFreeSpaceExW(drive, &freeBytesAvail, &totalBytes, &totalFreeBytes)) {
      vol.totalBytes = totalBytes.QuadPart;
      vol.freeBytes = totalFreeBytes.QuadPart;
    }

    vol.state = DataState::Valid;
    out.push_back(std::move(vol));
  }
}

EnvironmentBaseline EnvironmentCollector::CollectFullBaseline() {
  EnvironmentBaseline baseline;

  {
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    baseline.createdAtMonotonicNs = static_cast<std::uint64_t>(now.QuadPart) * 1000000000ULL /
      static_cast<std::uint64_t>(freq.QuadPart);
    baseline.createdAtWallMs = GetTickCount64();
  }

  wchar_t hostBuf[MAX_COMPUTERNAME_LENGTH + 1]{};
  DWORD hostSize = MAX_COMPUTERNAME_LENGTH + 1;
  if (GetComputerNameW(hostBuf, &hostSize)) {
    baseline.hostname = hostBuf;
  }

  static std::uint64_t s_nextId = 1;
  baseline.baselineId = s_nextId++;

  CollectFirmware(baseline.firmware);
  CollectSoftwareEnvironment(baseline.software);
  CollectSecurityState(baseline.security);
  CollectDrivers(baseline.drivers);
  CollectProcesses(baseline.processes);
  CollectServices(baseline.services);
  CollectStartupItems(baseline.startupItems);
  CollectScheduledTasks(baseline.scheduledTasks);
  CollectPciDevices(baseline.pciDevices);
  CollectUsbDevices(baseline.usbDevices);
  CollectDisplayDevices(baseline.displayDevices);
  CollectAudioDevices(baseline.audioDevices);
  CollectStorageDevices(baseline.storageDevices);
  CollectNetworkInterfaces(baseline.networkInterfaces);
  CollectVolumes(baseline.volumes);

  moduleDetector_.CollectAllProcessModules(baseline.modules);

  for (const auto& d : baseline.drivers) baseline.driverNameSet.insert(d.name);
  for (const auto& p : baseline.processes) baseline.processNameSet.insert(p.name);
  for (const auto& s : baseline.services) baseline.serviceNameSet.insert(s.name);
  for (const auto& s : baseline.startupItems) baseline.startupNameSet.insert(s.name);
  for (const auto& t : baseline.scheduledTasks) baseline.taskNameSet.insert(t.name);
  for (const auto& m : baseline.modules) baseline.modulePathSet.insert(m.fullPath);

  baseline.totalDriverCount = baseline.drivers.size();
  baseline.totalProcessCount = baseline.processes.size();
  baseline.totalServiceCount = baseline.services.size();
  baseline.totalStartupCount = baseline.startupItems.size();
  baseline.totalTaskCount = baseline.scheduledTasks.size();
  baseline.totalModuleCount = baseline.modules.size();

  baseline.ComputeContentHash();

  return baseline;
}

void EnvironmentCollector::DetectChanges(const EnvironmentBaseline& baseline,
                                         std::vector<EnvironmentChange>& outChanges) {
  std::vector<DriverEvidence> currentDrivers;
  CollectDrivers(currentDrivers);
  std::set<std::wstring> currentDriverNames;
  for (const auto& d : currentDrivers) currentDriverNames.insert(d.name);

  for (const auto& d : currentDriverNames) {
    if (baseline.driverNameSet.find(d) == baseline.driverNameSet.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Driver;
      change.kind = EnvironmentChange::ChangeKind::Added;
      change.entityName = d;
      change.evidence = L"driver_loaded";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }
  for (const auto& d : baseline.driverNameSet) {
    if (currentDriverNames.find(d) == currentDriverNames.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Driver;
      change.kind = EnvironmentChange::ChangeKind::Removed;
      change.entityName = d;
      change.evidence = L"driver_unloaded";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }

  std::vector<ServiceEvidence> currentServices;
  CollectServices(currentServices);
  std::set<std::wstring> currentServiceNames;
  for (const auto& s : currentServices) currentServiceNames.insert(s.name);

  for (const auto& s : currentServiceNames) {
    if (baseline.serviceNameSet.find(s) == baseline.serviceNameSet.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Service;
      change.kind = EnvironmentChange::ChangeKind::Added;
      change.entityName = s;
      change.evidence = L"service_created";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }
  for (const auto& s : baseline.serviceNameSet) {
    if (currentServiceNames.find(s) == currentServiceNames.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Service;
      change.kind = EnvironmentChange::ChangeKind::Removed;
      change.entityName = s;
      change.evidence = L"service_removed";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }

  std::vector<StartupItemEvidence> currentStartup;
  CollectStartupItems(currentStartup);
  std::set<std::wstring> currentStartupNames;
  for (const auto& s : currentStartup) currentStartupNames.insert(s.name);

  for (const auto& s : currentStartupNames) {
    if (baseline.startupNameSet.find(s) == baseline.startupNameSet.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::StartupItem;
      change.kind = EnvironmentChange::ChangeKind::Added;
      change.entityName = s;
      change.evidence = L"startup_added";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }
  for (const auto& s : baseline.startupNameSet) {
    if (currentStartupNames.find(s) == currentStartupNames.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::StartupItem;
      change.kind = EnvironmentChange::ChangeKind::Removed;
      change.entityName = s;
      change.evidence = L"startup_removed";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }

  SecurityStateEvidence currentSecurity;
  CollectSecurityState(currentSecurity);

  if (baseline.security.uacEnabled != currentSecurity.uacEnabled && currentSecurity.uacEnabled >= 0) {
    EnvironmentChange change;
    change.domain = EnvironmentChange::Domain::Security;
    change.kind = EnvironmentChange::ChangeKind::StateChanged;
    change.entityName = L"UAC";
    change.oldValue = std::to_wstring(baseline.security.uacEnabled);
    change.newValue = std::to_wstring(currentSecurity.uacEnabled);
    change.evidence = L"uac_changed";
    change.significant = true;
    outChanges.push_back(std::move(change));
  }

  if (baseline.security.firewallEnabled != currentSecurity.firewallEnabled && currentSecurity.firewallEnabled >= 0) {
    EnvironmentChange change;
    change.domain = EnvironmentChange::Domain::Security;
    change.kind = EnvironmentChange::ChangeKind::StateChanged;
    change.entityName = L"Firewall";
    change.oldValue = std::to_wstring(baseline.security.firewallEnabled);
    change.newValue = std::to_wstring(currentSecurity.firewallEnabled);
    change.evidence = L"firewall_changed";
    change.significant = true;
    outChanges.push_back(std::move(change));
  }

  if (baseline.security.secureBootEnabled != currentSecurity.secureBootEnabled && currentSecurity.secureBootEnabled >= 0) {
    EnvironmentChange change;
    change.domain = EnvironmentChange::Domain::Firmware;
    change.kind = EnvironmentChange::ChangeKind::StateChanged;
    change.entityName = L"SecureBoot";
    change.oldValue = std::to_wstring(baseline.security.secureBootEnabled);
    change.newValue = std::to_wstring(currentSecurity.secureBootEnabled);
    change.evidence = L"secure_boot_changed";
    change.significant = true;
    outChanges.push_back(std::move(change));
  }

  if (baseline.security.defenderRealTimeProtection != currentSecurity.defenderRealTimeProtection &&
      currentSecurity.defenderRealTimeProtection >= 0) {
    EnvironmentChange change;
    change.domain = EnvironmentChange::Domain::Security;
    change.kind = EnvironmentChange::ChangeKind::StateChanged;
    change.entityName = L"DefenderRealTime";
    change.oldValue = std::to_wstring(baseline.security.defenderRealTimeProtection);
    change.newValue = std::to_wstring(currentSecurity.defenderRealTimeProtection);
    change.evidence = L"defender_rt_changed";
    change.significant = true;
    outChanges.push_back(std::move(change));
  }

  std::vector<NetworkInterfaceEvidence> currentInterfaces;
  CollectNetworkInterfaces(currentInterfaces);
  std::set<std::wstring> currentIfaceNames;
  for (const auto& iface : currentInterfaces) currentIfaceNames.insert(iface.name);

  for (const auto& iface : currentInterfaces) {
    bool found = false;
    for (const auto& baseIface : baseline.networkInterfaces) {
      if (baseIface.name == iface.name) {
        found = true;
        if (baseIface.operStatus != iface.operStatus) {
          EnvironmentChange change;
          change.domain = EnvironmentChange::Domain::Network;
          change.kind = EnvironmentChange::ChangeKind::StateChanged;
          change.entityName = iface.name;
          change.oldValue = L"status=" + std::to_wstring(baseIface.operStatus);
          change.newValue = L"status=" + std::to_wstring(iface.operStatus);
          change.evidence = L"adapter_status_changed";
          change.significant = true;
          outChanges.push_back(std::move(change));
        }
        break;
      }
    }
    if (!found) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Network;
      change.kind = EnvironmentChange::ChangeKind::Added;
      change.entityName = iface.name;
      change.entityPath = iface.description;
      change.evidence = L"adapter_added";
      change.significant = true;
      outChanges.push_back(std::move(change));
    }
  }

  moduleDetector_.DetectChanges(baseline.modules, outChanges);

  for (auto& change : outChanges) {
    change.timestampNs = baseline.createdAtMonotonicNs;
    change.baselineId = baseline.baselineId;
  }
}

} // namespace monix
