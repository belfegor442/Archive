#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace monix::collectors::proc {

using ProcessInstanceId = std::uint64_t;
using ProcessId = std::uint32_t;
using ProcessId32 = std::uint32_t;
using ProcessId64 = std::uint64_t;

enum class ProcessArchitecture : std::uint8_t {
  Unknown,
  X86,
  X64,
  ARM,
  ARM64
};

const char* ProcessArchitectureName(ProcessArchitecture a);

enum class ProcessIntegrityLevel : std::uint8_t {
  Unknown,
  Untrusted,
  Low,
  Medium,
  High,
  System
};

const char* ProcessIntegrityLevelName(ProcessIntegrityLevel l);

enum class ProcessEventKind : std::uint8_t {
  Started,
  Terminated,
  Suspended,
  Resumed
};

const char* ProcessEventKindName(ProcessEventKind k);
std::string ProcessEventKindAction(ProcessEventKind k);

enum class ObservationOrigin : std::uint8_t {
  InitialSnapshot,
  Polling,
  Manual
};

const char* ObservationOriginName(ObservationOrigin o);

struct ProcessInfo {
  ProcessInstanceId instance_id = 0;
  ProcessId pid = 0;
  ProcessId parent_pid = 0;
  std::string image_name;
  std::string full_path;
  std::string command_line;
  std::string user_name;
  std::uint32_t session_id = 0;
  ProcessArchitecture architecture = ProcessArchitecture::Unknown;
  ProcessIntegrityLevel integrity_level = ProcessIntegrityLevel::Unknown;
  std::string working_directory;
  std::int64_t creation_time_ms = 0;
  std::int64_t exit_time_ms = 0;
  std::uint32_t exit_code = 0;
  std::uint64_t kernel_time_100ns = 0;
  std::uint64_t user_time_100ns = 0;
  std::uint64_t peak_working_set_size = 0;
  std::uint64_t working_set_size = 0;

  std::int64_t cpu_time_ms() const;
  bool is_alive() const;
  bool is_64bit() const;
};

}  // namespace monix::collectors::proc
