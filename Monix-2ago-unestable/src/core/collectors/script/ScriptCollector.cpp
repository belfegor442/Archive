#include "ScriptCollector.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace monix::collectors::script {

ScriptCollectorConfig ScriptCollectorConfig::defaults() {
  ScriptCollectorConfig cfg;
  cfg.detect_powershell = true;
  cfg.detect_cmd = true;
  cfg.detect_python = true;
  cfg.detect_bash = true;
  cfg.detect_wsh = true;
  cfg.detect_other = true;
  cfg.max_events = 10000;
  return cfg;
}

ScriptCollector::ScriptCollector() = default;

ScriptCollector::~ScriptCollector() {
  stop();
}

bool ScriptCollector::start(ScriptCollectorConfig config) {
  if (running_) return false;
  config_ = std::move(config);
  running_ = true;
  events_emitted_ = 0;
  return true;
}

bool ScriptCollector::stop() {
  if (!running_) return false;
  running_ = false;
  return true;
}

bool ScriptCollector::isRunning() const {
  return running_;
}

void ScriptCollector::setCallback(ScriptCallback callback) {
  callback_ = std::move(callback);
}

void ScriptCollector::reportExecution(const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
  if (!running_) return;

  ScriptInterpreter detected = DetectInterpreter(info.interpreter_name, info.raw_command_line);
  if (!shouldDetect(detected)) return;

  ScriptEventInfo enriched = info;
  enriched.interpreter = detected;
  if (enriched.interpreter_name.empty()) {
    enriched.interpreter_name = ScriptInterpreterName(detected);
  }
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  emit(enriched, origin);
}

std::size_t ScriptCollector::eventsEmitted() const {
  return events_emitted_;
}

std::vector<ScriptEventInfo> ScriptCollector::recentEvents(std::size_t count) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t start = event_log_.size() > count ? event_log_.size() - count : 0;
  return std::vector<ScriptEventInfo>(event_log_.begin() + start, event_log_.end());
}

bool ScriptCollector::isScriptInterpreter(const std::string& executable) {
  ScriptInterpreter interp = DetectInterpreter(executable, "");
  return interp != ScriptInterpreter::Unknown && interp != ScriptInterpreter::Other;
}

std::vector<std::string> ScriptCollector::scriptExtensions() {
  return {".ps1", ".psm1", ".psd1", ".bat", ".cmd", ".vbs", ".js", ".py",
          ".pyw", ".sh", ".bash", ".rb", ".pl", ".lua", ".wsf"};
}

bool ScriptCollector::shouldDetect(ScriptInterpreter interp) const {
  switch (interp) {
    case ScriptInterpreter::PowerShell: return config_.detect_powershell;
    case ScriptInterpreter::CMD:        return config_.detect_cmd;
    case ScriptInterpreter::Python:     return config_.detect_python;
    case ScriptInterpreter::Bash:       return config_.detect_bash;
    case ScriptInterpreter::Shell:      return config_.detect_bash;
    case ScriptInterpreter::WSH:        return config_.detect_wsh;
    case ScriptInterpreter::Perl:       return config_.detect_other;
    case ScriptInterpreter::Ruby:       return config_.detect_other;
    case ScriptInterpreter::NodeJS:     return config_.detect_other;
    case ScriptInterpreter::Other:      return config_.detect_other;
    case ScriptInterpreter::Unknown:    return false;
  }
  return false;
}

void ScriptCollector::emit(const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
  events_emitted_++;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (event_log_.size() >= config_.max_events) {
      event_log_.erase(event_log_.begin());
    }
    event_log_.push_back(info);
  }
  if (callback_) {
    callback_(info, origin);
  }
}

}  // namespace monix::collectors::script
