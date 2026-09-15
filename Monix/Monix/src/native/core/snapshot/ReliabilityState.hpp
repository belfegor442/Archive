#pragma once

namespace monix {

struct ReliabilityState {
  int sehExceptionCount = 0;
  int unhandledExceptionCount = 0;
  int accessViolationCount = 0;
  int heapCorruptionDetected = 0;
  int assertionFailureCount = 0;
  int stackOverflowCount = 0;
  int deadlockRecoveryCount = 0;
  int processRestartCount = 0;
  int moduleReloadCount = 0;
  int uiFreezeDetected = 0;
  int hangDetected = 0;
  int timeoutExceeded = 0;
  int retryStormDetected = 0;
  int backoffEscalation = 0;
  int circuitBreakerOpen = 0;
  int circuitBreakerClose = 0;
  int fallbackModeEntry = 0;
  int fallbackModeExit = 0;
  int configRollbackDetected = 0;
  int safeModeActive = 0;
  int telemetryDropDetected = 0;
  int watchdogResetDetected = 0;
  int serviceRecoveryAction = 0;
  int crashEventsToday = 0;
  int exceptionLogCount = 0;
  unsigned long processHash = 0;
  unsigned long moduleHash = 0;
  int mainThreadResponsive = 1;
  int uiResponsivenessMs = 0;
  int threadHealthOk = 1;
};

}
