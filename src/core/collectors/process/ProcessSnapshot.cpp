#include "ProcessSnapshot.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace monix::collectors::proc {

static std::string wideToUtf8(const wchar_t* wstr) {
  if (!wstr || wstr[0] == L'\0') return "";
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 0) return "";
  std::string result(static_cast<std::size_t>(size) - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), size, nullptr, nullptr);
  return result;
}

ProcessSnapshot::ProcessSnapshot() = default;

bool ProcessSnapshot::capture() {
  entries_.clear();
  pid_to_id_.clear();
  capture_time_ms_ = nowMs();

  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;

  PROCESSENTRY32W pe;
  pe.dwSize = sizeof(pe);

  if (!Process32FirstW(snap, &pe)) {
    CloseHandle(snap);
    return false;
  }

  do {
    ProcessInfo info;
    info.pid = pe.th32ProcessID;
    info.parent_pid = pe.th32ParentProcessID;
    info.image_name = extractBaseName(
      wideToUtf8(pe.szExeFile));

    HANDLE hProc = OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
      FALSE, info.pid);

    if (hProc) {
      DWORD exitCode = 0;
      if (GetExitCodeProcess(hProc, &exitCode)) {
        info.exit_code = exitCode;
      }

      FILETIME creation_time{}, exit_time{}, kernel_time{}, user_time{};
      if (GetProcessTimes(hProc, &creation_time, &exit_time, &kernel_time, &user_time)) {
        auto toMs = [](const FILETIME& ft) -> std::int64_t {
          ULARGE_INTEGER li;
          li.LowPart = ft.dwLowDateTime;
          li.HighPart = ft.dwHighDateTime;
          return static_cast<std::int64_t>(li.QuadPart / 10000);
        };
        info.creation_time_ms = toMs(creation_time);
        info.exit_time_ms = toMs(exit_time);
        info.kernel_time_100ns = (static_cast<std::uint64_t>(kernel_time.dwHighDateTime) << 32) |
                                 static_cast<std::uint64_t>(kernel_time.dwLowDateTime);
        info.user_time_100ns = (static_cast<std::uint64_t>(user_time.dwHighDateTime) << 32) |
                               static_cast<std::uint64_t>(user_time.dwLowDateTime);
      }

      PROCESS_MEMORY_COUNTERS pmc{};
      if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
        info.peak_working_set_size = pmc.PeakWorkingSetSize;
        info.working_set_size = pmc.WorkingSetSize;
      }

      wchar_t path_buf[MAX_PATH]{};
      DWORD path_len = MAX_PATH;
      if (QueryFullProcessImageNameW(hProc, 0, path_buf, &path_len)) {
        info.full_path = wideToUtf8(path_buf);
      }

      DWORD session_id = 0;
      if (ProcessIdToSessionId(info.pid, &session_id)) {
        info.session_id = session_id;
      }

      HANDLE hToken = nullptr;
      if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
        DWORD len = 0;
        GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &len);
        if (len > 0) {
          std::vector<BYTE> buf(len);
          if (GetTokenInformation(hToken, TokenIntegrityLevel, buf.data(), len, &len)) {
            auto* tid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buf.data());
            PSID sid = tid->Label.Sid;
            PUCHAR count = sid && IsValidSid(sid) ? GetSidSubAuthorityCount(sid) : nullptr;
            DWORD subAuth = 0;
            if (count && *count > 0) {
              PDWORD authority = GetSidSubAuthority(sid, *count - 1);
              if (authority) subAuth = *authority;
            }
            if (subAuth < SECURITY_MANDATORY_UNTRUSTED_RID)
              info.integrity_level = ProcessIntegrityLevel::Untrusted;
            else if (subAuth < SECURITY_MANDATORY_LOW_RID)
              info.integrity_level = ProcessIntegrityLevel::Low;
            else if (subAuth < SECURITY_MANDATORY_MEDIUM_RID)
              info.integrity_level = ProcessIntegrityLevel::Medium;
            else if (subAuth < SECURITY_MANDATORY_HIGH_RID)
              info.integrity_level = ProcessIntegrityLevel::Medium;
            else if (subAuth < SECURITY_MANDATORY_SYSTEM_RID)
              info.integrity_level = ProcessIntegrityLevel::High;
            else
              info.integrity_level = ProcessIntegrityLevel::System;
          }
        }

        DWORD user_len = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &user_len);
        if (user_len > 0) {
          std::vector<BYTE> ubuf(user_len);
          if (GetTokenInformation(hToken, TokenUser, ubuf.data(), user_len, &user_len)) {
            auto* tu = reinterpret_cast<TOKEN_USER*>(ubuf.data());
            wchar_t name[256]{}, domain[256]{};
            DWORD name_size = 256, domain_size = 256, sid_type = 0;
            if (LookupAccountSidW(nullptr, tu->User.Sid, name, &name_size,
                                   domain, &domain_size, (SID_NAME_USE*)&sid_type)) {
              info.user_name = wideToUtf8(domain)
                + "\\" + wideToUtf8(name);
            }
          }
        }
        CloseHandle(hToken);
      }

      info.architecture = detectArch();
      CloseHandle(hProc);
    }

    info.instance_id = ProcessIdentity::compute(info.pid, info.creation_time_ms);
    if (info.instance_id == 0 && info.pid != 0) {
      info.instance_id = static_cast<ProcessInstanceId>(info.pid) | (static_cast<ProcessInstanceId>(1) << 48);
    }
    entries_[info.instance_id] = info;
    pid_to_id_[info.pid] = info.instance_id;

  } while (Process32NextW(snap, &pe));

  CloseHandle(snap);
  return true;
}

bool ProcessSnapshot::captureSingle(ProcessId pid) {
  HANDLE hProc = OpenProcess(
    PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
    FALSE, pid);
  if (!hProc) return false;

  ProcessInfo info;
  info.pid = pid;

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    if (Process32FirstW(snap, &pe)) {
      do {
        if (pe.th32ProcessID == pid) {
          info.parent_pid = pe.th32ParentProcessID;
          info.image_name = extractBaseName(
            wideToUtf8(pe.szExeFile));
          break;
        }
      } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
  }

  FILETIME creation_time{}, exit_time{}, kernel_time{}, user_time{};
  if (GetProcessTimes(hProc, &creation_time, &exit_time, &kernel_time, &user_time)) {
    auto toMs = [](const FILETIME& ft) -> std::int64_t {
      ULARGE_INTEGER li;
      li.LowPart = ft.dwLowDateTime;
      li.HighPart = ft.dwHighDateTime;
      return static_cast<std::int64_t>(li.QuadPart / 10000);
    };
    info.creation_time_ms = toMs(creation_time);
    info.exit_time_ms = toMs(exit_time);
  }

  PROCESS_MEMORY_COUNTERS pmc{};
  if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
    info.working_set_size = pmc.WorkingSetSize;
  }

  wchar_t path_buf[MAX_PATH]{};
  DWORD path_len = MAX_PATH;
  if (QueryFullProcessImageNameW(hProc, 0, path_buf, &path_len)) {
    info.full_path = wideToUtf8(path_buf);
    if (info.image_name.empty()) {
      info.image_name = extractBaseName(info.full_path);
    }
  }

  info.instance_id = ProcessIdentity::compute(info.pid, info.creation_time_ms);
  info.architecture = detectArch();

  CloseHandle(hProc);

  entries_[info.instance_id] = info;
  pid_to_id_[info.pid] = info.instance_id;
  return true;
}

std::vector<ProcessInfo> ProcessSnapshot::processes() const {
  std::vector<ProcessInfo> result;
  result.reserve(entries_.size());
  for (const auto& [id, info] : entries_) {
    result.push_back(info);
  }
  return result;
}

std::size_t ProcessSnapshot::processCount() const {
  return entries_.size();
}

bool ProcessSnapshot::contains(ProcessInstanceId id) const {
  return entries_.find(id) != entries_.end();
}

bool ProcessSnapshot::containsPid(ProcessId pid) const {
  return pid_to_id_.find(pid) != pid_to_id_.end();
}

const ProcessInfo* ProcessSnapshot::find(ProcessInstanceId id) const {
  auto it = entries_.find(id);
  return it != entries_.end() ? &it->second : nullptr;
}

const ProcessInfo* ProcessSnapshot::findByPid(ProcessId pid) const {
  auto it = pid_to_id_.find(pid);
  if (it == pid_to_id_.end()) return nullptr;
  auto infoIt = entries_.find(it->second);
  return infoIt != entries_.end() ? &infoIt->second : nullptr;
}

std::vector<ProcessInfo> ProcessSnapshot::diffsFrom(const ProcessSnapshot& previous) const {
  std::vector<ProcessInfo> result;

  for (const auto& [id, info] : entries_) {
    if (!previous.contains(id)) {
      result.push_back(info);
    }
  }

  for (const auto& [id, info] : previous.entries_) {
    if (!contains(id)) {
      result.push_back(info);
    }
  }

  return result;
}

std::vector<ProcessId> ProcessSnapshot::newPids(const ProcessSnapshot& previous) const {
  std::vector<ProcessId> result;
  for (const auto& [id, info] : entries_) {
    if (!previous.contains(id)) {
      result.push_back(info.pid);
    }
  }
  return result;
}

std::vector<ProcessId> ProcessSnapshot::removedPids(const ProcessSnapshot& previous) const {
  std::vector<ProcessId> result;
  for (const auto& [id, info] : previous.entries_) {
    if (!contains(id)) {
      result.push_back(info.pid);
    }
  }
  return result;
}

void ProcessSnapshot::clear() {
  entries_.clear();
  pid_to_id_.clear();
  capture_time_ms_ = 0;
}

std::int64_t ProcessSnapshot::captureTimeMs() const {
  return capture_time_ms_;
}

ProcessInfo ProcessSnapshot::selfInfo() {
  return currentProcessInfo();
}

ProcessInfo ProcessSnapshot::currentProcessInfo() {
  ProcessInfo info;
  info.pid = static_cast<ProcessId>(GetCurrentProcessId());
  info.parent_pid = 0;
  info.image_name = "test_process.exe";
  info.architecture = detectArch();

  info.instance_id = ProcessIdentity::compute(info.pid,
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());

  return info;
}

std::int64_t ProcessSnapshot::nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string ProcessSnapshot::extractBaseName(const std::string& path) {
  auto pos = path.find_last_of("/\\");
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

ProcessArchitecture ProcessSnapshot::detectArch() {
#if defined(_M_X64)
  return ProcessArchitecture::X64;
#elif defined(_M_IX86)
  return ProcessArchitecture::X86;
#elif defined(_M_ARM64)
  return ProcessArchitecture::ARM64;
#elif defined(_M_ARM)
  return ProcessArchitecture::ARM;
#else
  return ProcessArchitecture::Unknown;
#endif
}

}  // namespace monix::collectors::proc
