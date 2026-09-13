#include "WindowsPlatform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <wtsapi32.h>
#include <psapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "psapi.lib")

#include <algorithm>

namespace monix::platform::windows {

namespace {
std::string wideToUtf8(const wchar_t* value) {
  if (!value || !*value) return {};
  int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) return {};
  std::string result(static_cast<std::size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
  return result;
}
}

PlatformType WindowsProcessProvider::platform() const { return PlatformType::Windows; }

std::vector<ProcessInfo> WindowsProcessProvider::enumerateProcesses() const {
  std::vector<ProcessInfo> result;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return result;

  PROCESSENTRY32W pe;
  pe.dwSize = sizeof(pe);

  if (Process32FirstW(snap, &pe)) {
    do {
      ProcessInfo info;
      info.pid = pe.th32ProcessID;
      info.parent_pid = pe.th32ParentProcessID;
      info.name = wideToUtf8(pe.szExeFile);
      result.push_back(info);
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return result;
}

ProcessInfo WindowsProcessProvider::getProcessInfo(std::uint64_t pid) const {
  ProcessInfo info;
  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
  if (proc) {
    info.pid = pid;
    WCHAR buf[MAX_PATH];
    if (GetModuleBaseNameW(proc, NULL, buf, MAX_PATH)) {
      info.name = wideToUtf8(buf);
    }
    CloseHandle(proc);
  }
  return info;
}

bool WindowsProcessProvider::isProcessRunning(std::uint64_t pid) const {
  HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (proc) {
    CloseHandle(proc);
    return true;
  }
  return false;
}

PlatformType WindowsFileSystemProvider::platform() const { return PlatformType::Windows; }

std::vector<FileSystemEntry> WindowsFileSystemProvider::listDirectory(const std::string& path) const {
  std::vector<FileSystemEntry> result;
  WIN32_FIND_DATAA fd;
  std::string pattern = path + "\\*";
  HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
  if (find == INVALID_HANDLE_VALUE) return result;

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
    FileSystemEntry entry;
    entry.name = fd.cFileName;
    entry.path = path + "\\" + fd.cFileName;
    entry.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    entry.is_file = !entry.is_directory;
    entry.is_hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    entry.size_bytes = (static_cast<std::uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
    result.push_back(entry);
  } while (FindNextFileA(find, &fd));

  FindClose(find);
  return result;
}

bool WindowsFileSystemProvider::fileExists(const std::string& path) const {
  DWORD attr = GetFileAttributesA(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES;
}

std::uint64_t WindowsFileSystemProvider::fileSize(const std::string& path) const {
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  }
  return 0;
}

bool WindowsFileSystemProvider::isReadable(const std::string& path) const {
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    CloseHandle(h);
    return true;
  }
  return false;
}

PlatformType WindowsNetworkProvider::platform() const { return PlatformType::Windows; }

std::vector<NetworkInterface> WindowsNetworkProvider::enumerateInterfaces() const {
  std::vector<NetworkInterface> result;
  ULONG bufLen = 15000;
 PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(bufLen));
  if (!adapters) return result;

  ULONG ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &bufLen);
  if (ret == NO_ERROR) {
    for (PIP_ADAPTER_ADDRESSES aa = adapters; aa != NULL; aa = aa->Next) {
      NetworkInterface iface;
      iface.name = aa->AdapterName;
      iface.is_up = (aa->OperStatus == IfOperStatusUp);
      if (aa->FirstUnicastAddress && aa->FirstUnicastAddress->Address.lpSockaddr) {
        const auto* address = aa->FirstUnicastAddress->Address.lpSockaddr;
        char ip[INET6_ADDRSTRLEN]{};
        if (address->sa_family == AF_INET) {
          const auto* sa = reinterpret_cast<const sockaddr_in*>(address);
          if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip))) iface.ip_address = ip;
        } else if (address->sa_family == AF_INET6) {
          const auto* sa = reinterpret_cast<const sockaddr_in6*>(address);
          if (inet_ntop(AF_INET6, &sa->sin6_addr, ip, sizeof(ip))) iface.ip_address = ip;
        }
      }
      result.push_back(iface);
    }
  }
  free(adapters);
  return result;
}

PlatformType WindowsUserSessionProvider::platform() const { return PlatformType::Windows; }

std::vector<UserSession> WindowsUserSessionProvider::enumerateSessions() const {
  std::vector<UserSession> result;
  PWTS_SESSION_INFOA sessions = nullptr;
  DWORD count = 0;
  if (WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
    for (DWORD i = 0; i < count; i++) {
      UserSession s;
      s.session_id = std::to_string(sessions[i].SessionId);
      s.is_active = (sessions[i].State == WTSActive);
      result.push_back(s);
    }
    WTSFreeMemory(sessions);
  }
  return result;
}

}  // namespace monix::platform::windows
