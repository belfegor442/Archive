#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlobj.h>

#include <cstdint>
#include <string>

#include "RegistryCollector.hpp"

namespace monix {

unsigned long HashRegistryValues(HKEY hKey) {
  unsigned long hash = 2166136261u;
  wchar_t valueName[128];
  BYTE data[256];
  DWORD valueNameLen, dataLen, valueType;
  DWORD index = 0;
  const DWORD kMaxValues = 128;
  while (index < kMaxValues) {
    valueNameLen = 128;
    dataLen = 256;
    if (RegEnumValueW(hKey, index, valueName, &valueNameLen, nullptr, &valueType, data, &dataLen) != ERROR_SUCCESS) break;
    for (DWORD i = 0; i < valueNameLen; ++i) {
      hash ^= static_cast<unsigned char>(valueName[i]);
      hash *= 16777619u;
    }
    if (dataLen > 0 && dataLen <= 256) {
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
    }
  }

  HKEY hEnv;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &hEnv) == ERROR_SUCCESS) {
    DWORD valueCount = 0;
    RegQueryInfoKeyW(hEnv, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &valueCount, nullptr, nullptr, nullptr, nullptr);
    snapshot.regKeyCountEnv = static_cast<int>(valueCount);
    snapshot.regHashEnv = HashRegistryValues(hEnv);
    RegCloseKey(hEnv);
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

}
