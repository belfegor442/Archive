#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace monix::collectors::script {

enum class ScriptInterpreter : std::uint8_t {
  PowerShell,
  CMD,
  Python,
  Bash,
  Shell,
  WSH,
  Perl,
  Ruby,
  NodeJS,
  Other,
  Unknown
};

const char* ScriptInterpreterName(ScriptInterpreter i);
ScriptInterpreter ScriptInterpreterFromName(const std::string& name);
ScriptInterpreter DetectInterpreter(const std::string& executable, const std::string& command_line);

struct ScriptEventInfo {
  ScriptInterpreter interpreter = ScriptInterpreter::Unknown;
  std::string interpreter_name;
  std::string script;
  std::string arguments;
  std::string actor;
  std::uint32_t actor_pid = 0;
  std::string parent_process;
  std::uint32_t parent_pid = 0;
  std::string working_directory;
  std::string raw_command_line;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

enum class ScriptDetectionOrigin : std::uint8_t {
  ProcessCollector,
  Manual,
  Polling
};

const char* ScriptDetectionOriginName(ScriptDetectionOrigin o);

}  // namespace monix::collectors::script
