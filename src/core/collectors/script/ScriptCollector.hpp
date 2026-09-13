#pragma once

#include "ScriptTypes.hpp"

#include <functional>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_set>

namespace monix::collectors::script {

using ScriptCallback = std::function<void(
  const ScriptEventInfo&, ScriptDetectionOrigin)>;

struct ScriptCollectorConfig {
  bool detect_powershell = true;
  bool detect_cmd = true;
  bool detect_python = true;
  bool detect_bash = true;
  bool detect_wsh = true;
  bool detect_other = true;
  std::size_t max_events = 10000;

  static ScriptCollectorConfig defaults();
};

class ScriptCollector {
public:
  ScriptCollector();
  ~ScriptCollector();

  ScriptCollector(const ScriptCollector&) = delete;
  ScriptCollector& operator=(const ScriptCollector&) = delete;

  bool start(ScriptCollectorConfig config = ScriptCollectorConfig::defaults());
  bool stop();
  bool isRunning() const;

  void setCallback(ScriptCallback callback);

  void reportExecution(const ScriptEventInfo& info,
                       ScriptDetectionOrigin origin = ScriptDetectionOrigin::Manual);

  std::size_t eventsEmitted() const;
  std::vector<ScriptEventInfo> recentEvents(std::size_t count = 100) const;

  static bool isScriptInterpreter(const std::string& executable);
  static std::vector<std::string> scriptExtensions();

private:
  bool shouldDetect(ScriptInterpreter interp) const;
  void emit(const ScriptEventInfo& info, ScriptDetectionOrigin origin);

  ScriptCollectorConfig config_;
  ScriptCallback callback_;
  std::vector<ScriptEventInfo> event_log_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::script
