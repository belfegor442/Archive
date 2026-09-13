#include "ScriptTypes.hpp"

#include <algorithm>
#include <cctype>

namespace monix::collectors::script {

const char* ScriptInterpreterName(ScriptInterpreter i) {
  switch (i) {
    case ScriptInterpreter::PowerShell: return "PowerShell";
    case ScriptInterpreter::CMD:        return "CMD";
    case ScriptInterpreter::Python:     return "Python";
    case ScriptInterpreter::Bash:       return "Bash";
    case ScriptInterpreter::Shell:      return "Shell";
    case ScriptInterpreter::WSH:        return "WSH";
    case ScriptInterpreter::Perl:       return "Perl";
    case ScriptInterpreter::Ruby:       return "Ruby";
    case ScriptInterpreter::NodeJS:     return "NodeJS";
    case ScriptInterpreter::Other:      return "Other";
    case ScriptInterpreter::Unknown:    return "Unknown";
  }
  return "Unknown";
}

ScriptInterpreter ScriptInterpreterFromName(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lower == "powershell" || lower == "pwsh" || lower == "powershell_ise")
    return ScriptInterpreter::PowerShell;
  if (lower == "cmd" || lower == "cmd.exe")
    return ScriptInterpreter::CMD;
  if (lower == "python" || lower == "python3" || lower == "python.exe" || lower == "python3.exe")
    return ScriptInterpreter::Python;
  if (lower == "bash" || lower == "bash.exe")
    return ScriptInterpreter::Bash;
  if (lower == "sh" || lower == "dash" || lower == "zsh" || lower == "ksh")
    return ScriptInterpreter::Shell;
  if (lower == "cscript" || lower == "wscript")
    return ScriptInterpreter::WSH;
  if (lower == "perl" || lower == "perl.exe")
    return ScriptInterpreter::Perl;
  if (lower == "ruby" || lower == "ruby.exe")
    return ScriptInterpreter::Ruby;
  if (lower == "node" || lower == "node.exe")
    return ScriptInterpreter::NodeJS;

  return ScriptInterpreter::Unknown;
}

ScriptInterpreter DetectInterpreter(const std::string& executable, const std::string& command_line) {
  std::string exeLower = executable;
  std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (exeLower.find("powershell") != std::string::npos ||
      exeLower.find("pwsh") != std::string::npos ||
      exeLower.find("powershell_ise") != std::string::npos)
    return ScriptInterpreter::PowerShell;
  if (exeLower.find("cmd.exe") != std::string::npos ||
      exeLower == "cmd")
    return ScriptInterpreter::CMD;
  if (exeLower.find("python") != std::string::npos)
    return ScriptInterpreter::Python;
  if (exeLower.find("bash") != std::string::npos)
    return ScriptInterpreter::Bash;
  if (exeLower == "sh" || exeLower == "dash" || exeLower == "zsh" || exeLower == "ksh")
    return ScriptInterpreter::Shell;
  if (exeLower.find("cscript") != std::string::npos ||
      exeLower.find("wscript") != std::string::npos)
    return ScriptInterpreter::WSH;
  if (exeLower.find("perl") != std::string::npos)
    return ScriptInterpreter::Perl;
  if (exeLower.find("ruby") != std::string::npos)
    return ScriptInterpreter::Ruby;
  if (exeLower.find("node") != std::string::npos)
    return ScriptInterpreter::NodeJS;

  std::string cmdLower = command_line;
  std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (cmdLower.find("powershell") != std::string::npos ||
      cmdLower.find("pwsh ") != std::string::npos)
    return ScriptInterpreter::PowerShell;
  if (cmdLower.find("python") != std::string::npos)
    return ScriptInterpreter::Python;
  if (cmdLower.find("node ") != std::string::npos ||
      cmdLower.find("nodejs") != std::string::npos)
    return ScriptInterpreter::NodeJS;

  if (!cmdLower.empty()) {
    if (cmdLower.find("-file ") == 0 ||
        cmdLower.find("-command ") == 0 ||
        cmdLower.find("-ep ") == 0 ||
        cmdLower.find("-encodedcommand ") == 0 ||
        cmdLower.find(" -file ") != std::string::npos ||
        cmdLower.find(" -command ") != std::string::npos ||
        cmdLower.find(" -ep ") != std::string::npos ||
        cmdLower.find(" -encodedcommand ") != std::string::npos)
      return ScriptInterpreter::PowerShell;
  }

  if (exeLower.empty() && cmdLower.empty()) {
    return ScriptInterpreter::Unknown;
  }

  return ScriptInterpreter::Other;
}

bool ScriptEventInfo::isValid() const {
  return !interpreter_name.empty() || !raw_command_line.empty();
}

std::string ScriptEventInfo::summary() const {
  std::string result = interpreter_name;
  if (!script.empty()) {
    result += " -> " + script;
  }
  return result;
}

const char* ScriptDetectionOriginName(ScriptDetectionOrigin o) {
  switch (o) {
    case ScriptDetectionOrigin::ProcessCollector: return "ProcessCollector";
    case ScriptDetectionOrigin::Manual:           return "Manual";
    case ScriptDetectionOrigin::Polling:          return "Polling";
  }
  return "Unknown";
}

}  // namespace monix::collectors::script
