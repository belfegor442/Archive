#include "../../../core/collectors/script/ScriptTypes.hpp"
#include "../../../core/collectors/script/ScriptCollector.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace monix::collectors::script;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-62s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) { FAIL(#a " < " #b); return; } } while(0)

// ==================== Interpreter Name Tests ====================

static void test_interpreter_names() {
  TEST("ScriptInterpreter: all names");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::PowerShell)), "PowerShell");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::CMD)), "CMD");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Python)), "Python");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Bash)), "Bash");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Shell)), "Shell");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::WSH)), "WSH");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Perl)), "Perl");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Ruby)), "Ruby");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::NodeJS)), "NodeJS");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Other)), "Other");
  ASSERT_EQ(std::string(ScriptInterpreterName(ScriptInterpreter::Unknown)), "Unknown");
  PASS();
}

// ==================== InterpreterFromName Tests ====================

static void test_interpreter_from_name_powershell() {
  TEST("InterpreterFromName: powershell variants");
  ASSERT_EQ(ScriptInterpreterFromName("powershell"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(ScriptInterpreterFromName("PowerShell"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(ScriptInterpreterFromName("pwsh"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(ScriptInterpreterFromName("PWSH"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(ScriptInterpreterFromName("powershell_ise"), ScriptInterpreter::PowerShell);
  PASS();
}

static void test_interpreter_from_name_cmd() {
  TEST("InterpreterFromName: cmd variants");
  ASSERT_EQ(ScriptInterpreterFromName("cmd"), ScriptInterpreter::CMD);
  ASSERT_EQ(ScriptInterpreterFromName("CMD"), ScriptInterpreter::CMD);
  ASSERT_EQ(ScriptInterpreterFromName("cmd.exe"), ScriptInterpreter::CMD);
  PASS();
}

static void test_interpreter_from_name_python() {
  TEST("InterpreterFromName: python variants");
  ASSERT_EQ(ScriptInterpreterFromName("python"), ScriptInterpreter::Python);
  ASSERT_EQ(ScriptInterpreterFromName("Python"), ScriptInterpreter::Python);
  ASSERT_EQ(ScriptInterpreterFromName("python3"), ScriptInterpreter::Python);
  ASSERT_EQ(ScriptInterpreterFromName("python.exe"), ScriptInterpreter::Python);
  PASS();
}

static void test_interpreter_from_name_other() {
  TEST("InterpreterFromName: bash, sh, wsh, perl, ruby, node");
  ASSERT_EQ(ScriptInterpreterFromName("bash"), ScriptInterpreter::Bash);
  ASSERT_EQ(ScriptInterpreterFromName("sh"), ScriptInterpreter::Shell);
  ASSERT_EQ(ScriptInterpreterFromName("zsh"), ScriptInterpreter::Shell);
  ASSERT_EQ(ScriptInterpreterFromName("cscript"), ScriptInterpreter::WSH);
  ASSERT_EQ(ScriptInterpreterFromName("wscript"), ScriptInterpreter::WSH);
  ASSERT_EQ(ScriptInterpreterFromName("perl"), ScriptInterpreter::Perl);
  ASSERT_EQ(ScriptInterpreterFromName("ruby"), ScriptInterpreter::Ruby);
  ASSERT_EQ(ScriptInterpreterFromName("node"), ScriptInterpreter::NodeJS);
  ASSERT_EQ(ScriptInterpreterFromName("node.exe"), ScriptInterpreter::NodeJS);
  PASS();
}

static void test_interpreter_from_name_unknown() {
  TEST("InterpreterFromName: unknown interpreter");
  ASSERT_EQ(ScriptInterpreterFromName("rustc"), ScriptInterpreter::Unknown);
  ASSERT_EQ(ScriptInterpreterFromName(""), ScriptInterpreter::Unknown);
  PASS();
}

// ==================== DetectInterpreter Tests ====================

static void test_detect_powershell_exe() {
  TEST("DetectInterpreter: PowerShell from executable name");
  ASSERT_EQ(DetectInterpreter("powershell.exe", ""), ScriptInterpreter::PowerShell);
  ASSERT_EQ(DetectInterpreter("pwsh.exe", "-Command Get-Process"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(DetectInterpreter("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", ""),
            ScriptInterpreter::PowerShell);
  PASS();
}

static void test_detect_cmd_exe() {
  TEST("DetectInterpreter: CMD from executable name");
  ASSERT_EQ(DetectInterpreter("cmd.exe", ""), ScriptInterpreter::CMD);
  ASSERT_EQ(DetectInterpreter("cmd", "/c dir"), ScriptInterpreter::CMD);
  PASS();
}

static void test_detect_python_exe() {
  TEST("DetectInterpreter: Python from executable name");
  ASSERT_EQ(DetectInterpreter("python.exe", "script.py"), ScriptInterpreter::Python);
  ASSERT_EQ(DetectInterpreter("python3", "script.py"), ScriptInterpreter::Python);
  ASSERT_EQ(DetectInterpreter("C:\\Python39\\python.exe", "main.py"), ScriptInterpreter::Python);
  PASS();
}

static void test_detect_from_command_line() {
  TEST("DetectInterpreter: detect from command line fallback");
  ASSERT_EQ(DetectInterpreter("unknown.exe", "powershell -Command Get-Date"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(DetectInterpreter("unknown.exe", "python -m pytest"), ScriptInterpreter::Python);
  ASSERT_EQ(DetectInterpreter("unknown.exe", "node server.js"), ScriptInterpreter::NodeJS);
  PASS();
}

static void test_detect_powershell_flags() {
  TEST("DetectInterpreter: PowerShell flags in command line");
  ASSERT_EQ(DetectInterpreter("app.exe", "-File script.ps1"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(DetectInterpreter("app.exe", "-Command Write-Host"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(DetectInterpreter("app.exe", "-ep bypass"), ScriptInterpreter::PowerShell);
  ASSERT_EQ(DetectInterpreter("app.exe", "-EncodedCommand dABlAHMAdAA="), ScriptInterpreter::PowerShell);
  PASS();
}

// ==================== ScriptEventInfo Tests ====================

static void test_script_event_info_defaults() {
  TEST("ScriptEventInfo: defaults are safe");
  ScriptEventInfo info;
  ASSERT_EQ(info.interpreter, ScriptInterpreter::Unknown);
  ASSERT_TRUE(info.script.empty());
  ASSERT_TRUE(info.arguments.empty());
  ASSERT_TRUE(info.actor.empty());
  ASSERT_EQ(info.actor_pid, 0u);
  ASSERT_TRUE(info.parent_process.empty());
  ASSERT_EQ(info.parent_pid, 0u);
  ASSERT_TRUE(info.working_directory.empty());
  ASSERT_EQ(info.timestamp_ms, 0);
  PASS();
}

static void test_script_event_info_is_valid() {
  TEST("ScriptEventInfo: isValid checks fields");
  ScriptEventInfo empty;
  ASSERT_FALSE(empty.isValid());

  ScriptEventInfo withName;
  withName.interpreter_name = "PowerShell";
  ASSERT_TRUE(withName.isValid());

  ScriptEventInfo withCmd;
  withCmd.raw_command_line = "powershell -Command Get-Date";
  ASSERT_TRUE(withCmd.isValid());
  PASS();
}

static void test_script_event_info_summary() {
  TEST("ScriptEventInfo: summary format");
  ScriptEventInfo info;
  info.interpreter_name = "PowerShell";
  info.script = "Get-Process";
  ASSERT_EQ(info.summary(), "PowerShell -> Get-Process");

  ScriptEventInfo noScript;
  noScript.interpreter_name = "CMD";
  ASSERT_EQ(noScript.summary(), "CMD");
  PASS();
}

// ==================== Origin Tests ====================

static void test_origin_names() {
  TEST("ScriptDetectionOrigin: all names");
  ASSERT_EQ(std::string(ScriptDetectionOriginName(ScriptDetectionOrigin::ProcessCollector)), "ProcessCollector");
  ASSERT_EQ(std::string(ScriptDetectionOriginName(ScriptDetectionOrigin::Manual)), "Manual");
  ASSERT_EQ(std::string(ScriptDetectionOriginName(ScriptDetectionOrigin::Polling)), "Polling");
  PASS();
}

// ==================== Collector Lifecycle Tests ====================

static void test_collector_start_stop() {
  TEST("ScriptCollector: start and stop lifecycle");
  ScriptCollector collector;
  ASSERT_FALSE(collector.isRunning());
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  PASS();
}

static void test_collector_double_start() {
  TEST("ScriptCollector: double start returns false");
  ScriptCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("ScriptCollector: stop without start returns false");
  ScriptCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_config_defaults() {
  TEST("ScriptCollectorConfig: defaults are safe");
  auto cfg = ScriptCollectorConfig::defaults();
  ASSERT_TRUE(cfg.detect_powershell);
  ASSERT_TRUE(cfg.detect_cmd);
  ASSERT_TRUE(cfg.detect_python);
  ASSERT_TRUE(cfg.detect_bash);
  ASSERT_TRUE(cfg.detect_wsh);
  ASSERT_TRUE(cfg.detect_other);
  ASSERT_TRUE(cfg.max_events > 0);
  PASS();
}

// ==================== PowerShell Detection Tests ====================

static void test_powershell_detection() {
  TEST("PowerShell: detect execution via executable");
  ScriptCollector collector;
  std::atomic<int> count{0};
  ScriptInterpreter detectedInterp = ScriptInterpreter::Unknown;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
    detectedInterp = info.interpreter;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "powershell.exe";
  info.script = "Get-Process";
  info.arguments = "-Command Get-Process";
  info.actor = "test_user";
  info.actor_pid = 1234;
  info.parent_process = "explorer.exe";
  info.parent_pid = 5678;
  info.working_directory = "C:\\Users\\test";
  info.raw_command_line = "powershell.exe -Command Get-Process";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(detectedInterp, ScriptInterpreter::PowerShell);
  collector.stop();
  PASS();
}

static void test_powershell_ise_detection() {
  TEST("PowerShell ISE: detect via pwsh.exe");
  ScriptCollector collector;
  std::atomic<int> count{0};
  ScriptInterpreter detectedInterp = ScriptInterpreter::Unknown;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
    detectedInterp = info.interpreter;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "pwsh.exe";
  info.script = "deploy.ps1";
  info.raw_command_line = "pwsh.exe -File deploy.ps1";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(detectedInterp, ScriptInterpreter::PowerShell);
  collector.stop();
  PASS();
}

// ==================== CMD Detection Tests ====================

static void test_cmd_detection() {
  TEST("CMD: detect execution via cmd.exe");
  ScriptCollector collector;
  std::atomic<int> count{0};
  ScriptInterpreter detectedInterp = ScriptInterpreter::Unknown;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
    detectedInterp = info.interpreter;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "cmd.exe";
  info.script = "build.bat";
  info.arguments = "/c build.bat";
  info.raw_command_line = "cmd.exe /c build.bat";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(detectedInterp, ScriptInterpreter::CMD);
  collector.stop();
  PASS();
}

// ==================== Python Detection Tests ====================

static void test_python_detection() {
  TEST("Python: detect execution via python.exe");
  ScriptCollector collector;
  std::atomic<int> count{0};
  ScriptInterpreter detectedInterp = ScriptInterpreter::Unknown;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
    detectedInterp = info.interpreter;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "python.exe";
  info.script = "main.py";
  info.arguments = "main.py --verbose";
  info.raw_command_line = "python.exe main.py --verbose";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(detectedInterp, ScriptInterpreter::Python);
  collector.stop();
  PASS();
}

// ==================== Unknown Interpreter Tests ====================

static void test_unknown_interpreter_ignored() {
  TEST("Other interpreter: non-script detected as Other");
  ScriptCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "rustc.exe";
  info.raw_command_line = "rustc.exe main.rs";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 1);
  collector.stop();
  PASS();
}

static void test_truly_unknown_interpreter() {
  TEST("Unknown interpreter: empty both not detected");
  ScriptCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
  });

  collector.start();

  ScriptEventInfo info;
  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 0);
  collector.stop();
  PASS();
}

// ==================== Missing Script Tests ====================

static void test_missing_script_detected() {
  TEST("Missing script: interpreter still detected");
  ScriptCollector collector;
  std::atomic<int> count{0};
  ScriptInterpreter detectedInterp = ScriptInterpreter::Unknown;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
    detectedInterp = info.interpreter;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "powershell.exe";
  info.raw_command_line = "powershell.exe";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(detectedInterp, ScriptInterpreter::PowerShell);
  collector.stop();
  PASS();
}

static void test_empty_interpreter_and_command() {
  TEST("Empty interpreter and command: not detected");
  ScriptCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
  });

  collector.start();

  ScriptEventInfo info;
  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 0);
  collector.stop();
  PASS();
}

// ==================== Permission Denied Tests ====================

static void test_report_when_not_running() {
  TEST("Permission denied: report when not running ignored");
  ScriptCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
  });

  ScriptEventInfo info;
  info.interpreter_name = "powershell.exe";
  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 0);
  PASS();
}

// ==================== Correlation Tests ====================

static void test_correlation_with_process() {
  TEST("Correlation: script event carries process info");
  ScriptCollector collector;
  std::string capturedActor;
  std::uint32_t capturedPid = 0;
  std::string capturedParent;
  std::uint32_t capturedParentPid = 0;
  std::string capturedWorkDir;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    capturedActor = info.actor;
    capturedPid = info.actor_pid;
    capturedParent = info.parent_process;
    capturedParentPid = info.parent_pid;
    capturedWorkDir = info.working_directory;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "powershell.exe";
  info.actor = "admin";
  info.actor_pid = 1001;
  info.parent_process = "explorer.exe";
  info.parent_pid = 500;
  info.working_directory = "C:\\Scripts";
  info.raw_command_line = "powershell.exe -Command Test";

  collector.reportExecution(info);

  ASSERT_EQ(capturedActor, "admin");
  ASSERT_EQ(capturedPid, 1001u);
  ASSERT_EQ(capturedParent, "explorer.exe");
  ASSERT_EQ(capturedParentPid, 500u);
  ASSERT_EQ(capturedWorkDir, "C:\\Scripts");
  collector.stop();
  PASS();
}

static void test_correlation_with_user() {
  TEST("Correlation: script event carries user reference");
  ScriptCollector collector;
  std::string capturedActor;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    capturedActor = info.actor;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "cmd.exe";
  info.actor = "DOMAIN\\user";
  info.raw_command_line = "cmd.exe /c script.bat";

  collector.reportExecution(info);

  ASSERT_EQ(capturedActor, "DOMAIN\\user");
  collector.stop();
  PASS();
}

// ==================== Config Filtering Tests ====================

static void test_config_disable_powershell() {
  TEST("Config: disable PowerShell detection");
  ScriptCollectorConfig cfg = ScriptCollectorConfig::defaults();
  cfg.detect_powershell = false;

  ScriptCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
  });

  collector.start(cfg);

  ScriptEventInfo info;
  info.interpreter_name = "powershell.exe";
  info.raw_command_line = "powershell.exe -Command Get-Date";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 0);
  collector.stop();
  PASS();
}

static void test_config_disable_python() {
  TEST("Config: disable Python detection");
  ScriptCollectorConfig cfg = ScriptCollectorConfig::defaults();
  cfg.detect_python = false;

  ScriptCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    count++;
  });

  collector.start(cfg);

  ScriptEventInfo info;
  info.interpreter_name = "python.exe";
  info.raw_command_line = "python.exe script.py";

  collector.reportExecution(info);

  ASSERT_EQ(count.load(), 0);
  collector.stop();
  PASS();
}

// ==================== Event Log Tests ====================

static void test_event_log_accumulates() {
  TEST("Event log: events accumulate in log");
  ScriptCollector collector;
  collector.start();

  for (int i = 0; i < 5; i++) {
    ScriptEventInfo info;
    info.interpreter_name = "powershell.exe";
    info.raw_command_line = "powershell.exe -Command Test";
    collector.reportExecution(info);
  }

  auto recent = collector.recentEvents(10);
  ASSERT_TRUE(recent.size() == 5);
  ASSERT_EQ(collector.eventsEmitted(), 5u);
  collector.stop();
  PASS();
}

static void test_event_log_max_events() {
  TEST("Event log: respects max_events limit");
  ScriptCollectorConfig cfg = ScriptCollectorConfig::defaults();
  cfg.max_events = 3;

  ScriptCollector collector;
  collector.start(cfg);

  for (int i = 0; i < 5; i++) {
    ScriptEventInfo info;
    info.interpreter_name = "powershell.exe";
    info.raw_command_line = "powershell.exe";
    collector.reportExecution(info);
  }

  auto recent = collector.recentEvents(100);
  ASSERT_TRUE(recent.size() == 3);
  ASSERT_EQ(collector.eventsEmitted(), 5u);
  collector.stop();
  PASS();
}

// ==================== Static Helper Tests ====================

static void test_is_script_interpreter() {
  TEST("Static: isScriptInterpreter recognizes scripts");
  ASSERT_TRUE(ScriptCollector::isScriptInterpreter("powershell.exe"));
  ASSERT_TRUE(ScriptCollector::isScriptInterpreter("cmd.exe"));
  ASSERT_TRUE(ScriptCollector::isScriptInterpreter("python.exe"));
  ASSERT_TRUE(ScriptCollector::isScriptInterpreter("node.exe"));
  ASSERT_FALSE(ScriptCollector::isScriptInterpreter("notepad.exe"));
  ASSERT_FALSE(ScriptCollector::isScriptInterpreter("explorer.exe"));
  PASS();
}

static void test_script_extensions() {
  TEST("Static: scriptExtensions returns list");
  auto exts = ScriptCollector::scriptExtensions();
  ASSERT_TRUE(exts.size() > 0);

  bool hasPs1 = false, hasBat = false, hasPy = false, hasSh = false;
  for (const auto& e : exts) {
    if (e == ".ps1") hasPs1 = true;
    if (e == ".bat") hasBat = true;
    if (e == ".py") hasPy = true;
    if (e == ".sh") hasSh = true;
  }
  ASSERT_TRUE(hasPs1);
  ASSERT_TRUE(hasBat);
  ASSERT_TRUE(hasPy);
  ASSERT_TRUE(hasSh);
  PASS();
}

// ==================== Multiple Interpreters Tests ====================

static void test_multiple_interpreters() {
  TEST("Multiple interpreters: each detected correctly");
  ScriptCollector collector;
  std::vector<ScriptInterpreter> detected;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    detected.push_back(info.interpreter);
  });

  collector.start();

  const char* interpreters[] = {
    "powershell.exe", "cmd.exe", "python.exe", "node.exe", "ruby.exe"
  };
  for (const char* interp : interpreters) {
    ScriptEventInfo info;
    info.interpreter_name = interp;
    info.raw_command_line = std::string(interp);
    collector.reportExecution(info);
  }

  ASSERT_EQ(detected.size(), 5u);
  ASSERT_EQ(detected[0], ScriptInterpreter::PowerShell);
  ASSERT_EQ(detected[1], ScriptInterpreter::CMD);
  ASSERT_EQ(detected[2], ScriptInterpreter::Python);
  ASSERT_EQ(detected[3], ScriptInterpreter::NodeJS);
  ASSERT_EQ(detected[4], ScriptInterpreter::Ruby);
  collector.stop();
  PASS();
}

static void test_origin_forwarded() {
  TEST("Origin: correctly forwarded to callback");
  ScriptCollector collector;
  ScriptDetectionOrigin capturedOrigin = ScriptDetectionOrigin::Manual;

  collector.setCallback([&](const ScriptEventInfo& info, ScriptDetectionOrigin origin) {
    capturedOrigin = origin;
  });

  collector.start();

  ScriptEventInfo info;
  info.interpreter_name = "powershell.exe";
  info.raw_command_line = "powershell.exe";
  collector.reportExecution(info, ScriptDetectionOrigin::ProcessCollector);

  ASSERT_EQ(capturedOrigin, ScriptDetectionOrigin::ProcessCollector);
  collector.stop();
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Script Collector Tests ===\n\n");

  printf("[Interpreter Names]\n");
  test_interpreter_names();

  printf("[InterpreterFromName]\n");
  test_interpreter_from_name_powershell();
  test_interpreter_from_name_cmd();
  test_interpreter_from_name_python();
  test_interpreter_from_name_other();
  test_interpreter_from_name_unknown();

  printf("[DetectInterpreter]\n");
  test_detect_powershell_exe();
  test_detect_cmd_exe();
  test_detect_python_exe();
  test_detect_from_command_line();
  test_detect_powershell_flags();

  printf("[ScriptEventInfo]\n");
  test_script_event_info_defaults();
  test_script_event_info_is_valid();
  test_script_event_info_summary();

  printf("[Origin Names]\n");
  test_origin_names();

  printf("[Collector Lifecycle]\n");
  test_collector_start_stop();
  test_collector_double_start();
  test_collector_stop_without_start();
  test_collector_config_defaults();

  printf("[PowerShell Detection]\n");
  test_powershell_detection();
  test_powershell_ise_detection();

  printf("[CMD Detection]\n");
  test_cmd_detection();

  printf("[Python Detection]\n");
  test_python_detection();

  printf("[Unknown Interpreter]\n");
  test_unknown_interpreter_ignored();
  test_truly_unknown_interpreter();

  printf("[Missing Script]\n");
  test_missing_script_detected();
  test_empty_interpreter_and_command();

  printf("[Permission Denied]\n");
  test_report_when_not_running();

  printf("[Correlation]\n");
  test_correlation_with_process();
  test_correlation_with_user();

  printf("[Config Filtering]\n");
  test_config_disable_powershell();
  test_config_disable_python();

  printf("[Event Log]\n");
  test_event_log_accumulates();
  test_event_log_max_events();

  printf("[Static Helpers]\n");
  test_is_script_interpreter();
  test_script_extensions();

  printf("[Multiple Interpreters]\n");
  test_multiple_interpreters();
  test_origin_forwarded();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_ScriptCollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"ScriptInterpreter: names", test_interpreter_names},
  {"InterpreterFromName: powershell", test_interpreter_from_name_powershell},
  {"InterpreterFromName: cmd", test_interpreter_from_name_cmd},
  {"InterpreterFromName: python", test_interpreter_from_name_python},
  {"InterpreterFromName: other", test_interpreter_from_name_other},
  {"InterpreterFromName: unknown", test_interpreter_from_name_unknown},
  {"DetectInterpreter: PowerShell exe", test_detect_powershell_exe},
  {"DetectInterpreter: CMD exe", test_detect_cmd_exe},
  {"DetectInterpreter: Python exe", test_detect_python_exe},
  {"DetectInterpreter: from command line", test_detect_from_command_line},
  {"DetectInterpreter: PowerShell flags", test_detect_powershell_flags},
  {"ScriptEventInfo: defaults", test_script_event_info_defaults},
  {"ScriptEventInfo: isValid", test_script_event_info_is_valid},
  {"ScriptEventInfo: summary", test_script_event_info_summary},
  {"ScriptDetectionOrigin: names", test_origin_names},
  {"ScriptCollector: start stop", test_collector_start_stop},
  {"ScriptCollector: double start", test_collector_double_start},
  {"ScriptCollector: stop without start", test_collector_stop_without_start},
  {"ScriptCollectorConfig: defaults", test_collector_config_defaults},
  {"PowerShell: detection", test_powershell_detection},
  {"PowerShell ISE: detection", test_powershell_ise_detection},
  {"CMD: detection", test_cmd_detection},
  {"Python: detection", test_python_detection},
  {"Other interpreter: detected", test_unknown_interpreter_ignored},
  {"Unknown interpreter: empty both", test_truly_unknown_interpreter},
  {"Missing script: detected", test_missing_script_detected},
  {"Empty interpreter and command", test_empty_interpreter_and_command},
  {"Report when not running: ignored", test_report_when_not_running},
  {"Correlation: process info", test_correlation_with_process},
  {"Correlation: user reference", test_correlation_with_user},
  {"Config: disable PowerShell", test_config_disable_powershell},
  {"Config: disable Python", test_config_disable_python},
  {"Event log: accumulates", test_event_log_accumulates},
  {"Event log: max events", test_event_log_max_events},
  {"Static: isScriptInterpreter", test_is_script_interpreter},
  {"Static: scriptExtensions", test_script_extensions},
  {"Multiple interpreters: detected", test_multiple_interpreters},
  {"Origin: forwarded", test_origin_forwarded},
};

const KTestEntry* GetKTests_ScriptCollector() { return s_ktests; }
std::size_t GetKTestCount_ScriptCollector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
