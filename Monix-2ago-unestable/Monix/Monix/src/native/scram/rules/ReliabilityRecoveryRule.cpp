#include "ReliabilityRecoveryRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void ReliabilityRecoveryRule::Evaluate(const Snapshot& current,
                                       const Snapshot* previous,
                                       std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  // 1. Application crash
  if (current.crashEventsToday > prev.crashEventsToday && prev.crashEventsToday >= 0) {
    findings.push_back({
      L"Application crash detected.",
      L"New crash dump files found in the system Minidump or CrashDumps directory.",
      L"Crashes today: " + std::to_wstring(prev.crashEventsToday) + L" -> " + std::to_wstring(current.crashEventsToday) + L".",
      18
    });
  }

  // 2. Unhandled exception
  if (current.unhandledExceptionCount > prev.unhandledExceptionCount && prev.unhandledExceptionCount >= 0) {
    findings.push_back({
      L"Unhandled exception detected.",
      L"An unhandled exception has been caught by the filter.",
      L"Unhandled: " + std::to_wstring(prev.unhandledExceptionCount) + L" -> " + std::to_wstring(current.unhandledExceptionCount) + L".",
      16
    });
  }

  // 3. Structured exception event
  if (current.sehExceptionCount > prev.sehExceptionCount && prev.sehExceptionCount >= 0) {
    findings.push_back({
      L"Structured exception event.",
      L"SEH exception handler was invoked by the vectored exception handler.",
      L"SEH: " + std::to_wstring(prev.sehExceptionCount) + L" -> " + std::to_wstring(current.sehExceptionCount) + L".",
      14
    });
  }

  // 4. Stack overflow
  if (current.stackOverflowCount > prev.stackOverflowCount && prev.stackOverflowCount >= 0) {
    findings.push_back({
      L"Stack overflow detected.",
      L"A stack overflow exception was caught.",
      L"Stack overflows: " + std::to_wstring(current.stackOverflowCount) + L".",
      20
    });
  }

  // 5. Heap corruption
  if (current.heapCorruptionDetected > prev.heapCorruptionDetected && prev.heapCorruptionDetected >= 0) {
    findings.push_back({
      L"Heap corruption detected.",
      L"Heap corruption exception was caught by the exception handler.",
      L"Heap corruption events: " + std::to_wstring(current.heapCorruptionDetected) + L".",
      22
    });
  }

  // 6. Access violation
  if (current.accessViolationCount > prev.accessViolationCount && prev.accessViolationCount >= 0) {
    findings.push_back({
      L"Access violation detected.",
      L"An access violation exception was caught.",
      L"Access violations: " + std::to_wstring(prev.accessViolationCount) + L" -> " + std::to_wstring(current.accessViolationCount) + L".",
      16
    });
  }

  // 7. Assertion failure
  if (current.assertionFailureCount > prev.assertionFailureCount && prev.assertionFailureCount >= 0) {
    findings.push_back({
      L"Assertion failure detected.",
      L"An invalid parameter or pure virtual function call was detected.",
      L"Assertions: " + std::to_wstring(prev.assertionFailureCount) + L" -> " + std::to_wstring(current.assertionFailureCount) + L".",
      14
    });
  }

  // 8. Deadlock recovery
  if (current.deadlockRecoveryCount > prev.deadlockRecoveryCount && prev.deadlockRecoveryCount >= 0) {
    findings.push_back({
      L"Deadlock recovery event.",
      L"A potential deadlock condition was detected and recovered.",
      L"Deadlock recoveries: " + std::to_wstring(current.deadlockRecoveryCount) + L".",
      16
    });
  }

  // 9. Watchdog reset
  if (current.watchdogResetDetected > prev.watchdogResetDetected && prev.watchdogResetDetected >= 0) {
    findings.push_back({
      L"Watchdog reset detected.",
      L"System watchdog timer reset was detected, indicating unresponsive processing.",
      L"Watchdog resets: " + std::to_wstring(current.watchdogResetDetected) + L".",
      20
    });
  }

  // 10. Service recovery action
  if (current.serviceRecoveryAction > prev.serviceRecoveryAction && prev.serviceRecoveryAction >= 0) {
    findings.push_back({
      L"Service recovery action triggered.",
      L"A Windows service recovery action has been executed.",
      L"Service recoveries: " + std::to_wstring(current.serviceRecoveryAction) + L".",
      14
    });
  }

  // 11. Process restart
  if (current.processHash != prev.processHash && prev.processHash != 0) {
    findings.push_back({
      L"Process set changed.",
      L"The process list hash has changed, indicating process creation or termination.",
      L"Process hash changed.",
      8
    });
  }

  // 12. Module reload
  if (current.moduleHash != prev.moduleHash && prev.moduleHash != 0) {
    findings.push_back({
      L"Module reload detected.",
      L"A loaded module list has changed, indicating DLL load or unload.",
      L"Module hash changed.",
      10
    });
  }

  // 13. UI freeze detection
  if (current.uiResponsivenessMs > 200 && prev.uiResponsivenessMs > 0) {
    findings.push_back({
      L"UI freeze detected.",
      L"The main thread message pump took over 200ms to process.",
      L"UI latency: " + std::to_wstring(current.uiResponsivenessMs) + L"ms.",
      12
    });
  }

  // 14. Hang detection
  if (current.mainThreadResponsive == 0 && prev.mainThreadResponsive == 1) {
    findings.push_back({
      L"Thread hang detected.",
      L"The main thread is no longer responsive to message pump queries.",
      L"Main thread unresponsive.",
      16
    });
  }

  // 15. Timeout exceeded
  if (current.uiResponsivenessMs > 500 && prev.uiResponsivenessMs <= 500 && prev.uiResponsivenessMs > 0) {
    findings.push_back({
      L"Timeout exceeded.",
      L"Message processing timeout exceeded 500ms threshold.",
      L"Timeout: " + std::to_wstring(current.uiResponsivenessMs) + L"ms.",
      14
    });
  }

  // 16. Retry storm
  if (current.sehExceptionCount > prev.sehExceptionCount + 5 && prev.sehExceptionCount >= 0) {
    findings.push_back({
      L"Retry storm detected.",
      L"Multiple SEH exceptions in rapid succession, indicating a failure loop.",
      L"SEH burst: +" + std::to_wstring(current.sehExceptionCount - prev.sehExceptionCount) + L".",
      16
    });
  }

  // 17. Backoff escalation
  if (current.timeoutExceeded > prev.timeoutExceeded + 3 && prev.timeoutExceeded >= 0) {
    findings.push_back({
      L"Backoff escalation detected.",
      L"Multiple timeouts in sequence, indicating escalating delays.",
      L"Timeouts: " + std::to_wstring(prev.timeoutExceeded) + L" -> " + std::to_wstring(current.timeoutExceeded) + L".",
      12
    });
  }

  // 18. Circuit breaker open
  if (current.circuitBreakerOpen > prev.circuitBreakerOpen && prev.circuitBreakerOpen >= 0) {
    findings.push_back({
      L"Circuit breaker opened.",
      L"The failure threshold has been exceeded and the circuit breaker is open.",
      L"Circuit breaker: open.",
      10
    });
  }

  // 19. Circuit breaker close
  if (current.circuitBreakerClose > prev.circuitBreakerClose && prev.circuitBreakerClose >= 0) {
    findings.push_back({
      L"Circuit breaker closed.",
      L"The circuit breaker has recovered and is allowing operations.",
      L"Circuit breaker: closed.",
      4
    });
  }

  // 20. Fallback mode entry
  if (current.fallbackModeEntry > prev.fallbackModeEntry && prev.fallbackModeEntry >= 0) {
    findings.push_back({
      L"Fallback mode entered.",
      L"The system has entered a fallback or degraded mode.",
      L"Fallback mode: active.",
      10
    });
  }

  // 21. Fallback mode exit
  if (current.fallbackModeExit > prev.fallbackModeExit && prev.fallbackModeExit >= 0) {
    findings.push_back({
      L"Fallback mode exited.",
      L"The system has exited fallback mode and resumed normal operation.",
      L"Fallback mode: exited.",
      4
    });
  }

  // 22. Configuration rollback
  if (current.configRollbackDetected > prev.configRollbackDetected && prev.configRollbackDetected >= 0) {
    findings.push_back({
      L"Configuration rollback detected.",
      L"A configuration file has been rolled back to a previous version.",
      L"Config rollback events: " + std::to_wstring(current.configRollbackDetected) + L".",
      14
    });
  }

  // 23. Safe mode entry
  if (current.safeModeActive == 1 && prev.safeModeActive == 0) {
    findings.push_back({
      L"Safe mode entry detected.",
      L"The system has entered safe mode.",
      L"Safe mode: active.",
      18
    });
  }

  // 24. Safe mode exit
  if (current.safeModeActive == 0 && prev.safeModeActive == 1) {
    findings.push_back({
      L"Safe mode exit detected.",
      L"The system has exited safe mode.",
      L"Safe mode: exited.",
      6
    });
  }

  // 25. Telemetry drop detection
  if (current.telemetryDropDetected > prev.telemetryDropDetected && prev.telemetryDropDetected >= 0) {
    findings.push_back({
      L"Telemetry drop detected.",
      L"Telemetry data collection gaps have been detected.",
      L"Telemetry drops: " + std::to_wstring(current.telemetryDropDetected) + L".",
      10
    });
  }
}

} // namespace monix
