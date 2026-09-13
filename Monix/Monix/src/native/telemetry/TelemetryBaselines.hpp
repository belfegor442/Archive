#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "../../../sensors/sensors.h"

namespace monix {

// Baseline deltas for telemetry change detection.
// Replaces ~197 prev* fields that were scattered across MonixApp.
struct TelemetryBaselines {
  // --- CPU / TSC ---
  std::uint64_t cpuTsc = 0;
  ::CpuTimes cpuTimes = {};

  // --- Network Adapter / Routing ---
  std::set<std::wstring> netAdapterAddresses;
  std::set<std::wstring> netAdapterGateways;
  std::set<DWORD> netAdapterSpeeds;
  std::set<DWORD> netAdapterOperStatuses;
  std::set<DWORD> netAdapterTypes;
  std::uint64_t routeTableHash = 0;
  int proxyEnabled = 0;
  std::wstring proxyServer;

  // --- Scheduler / Kernel Counters ---
  std::uint64_t totalContextSwitches = 0;
  std::uint64_t totalInterruptCount = 0;
  std::uint64_t totalDpcCount = 0;
  std::uint64_t totalIsrCount = 0;
  int suspendedThreadCount = 0;
  int readyThreadCount = 0;

  // --- System Kernel ---
  std::set<std::wstring> driverNames;
  unsigned long long totalHandles = 0;
  unsigned long long totalObjects = 0;
  unsigned long long pageFaultsDelta = 0;
  unsigned long long ioReadBytesDelta = 0;
  unsigned long long ioWriteBytesDelta = 0;
  unsigned long long systemTime100ns = 0;
  int sessionCount = 0;

  // --- Security ---
  int selfSignatureValid = 0;
  int selfHashComputed = 0;
  int unsignedDriverCount = 0;
  int suspiciousScriptHosts = 0;
  int uacConsentProcesses = 0;
  int lsassAccessCount = 0;
  int debugPortActive = 0;
  int hookModulesDetected = 0;
  int peHeaderTamper = 0;
  int scheduledTaskCount = 0;
  std::set<std::wstring> unsignedDriverNames;
  std::set<std::wstring> suspiciousModules;

  // --- Power / Battery ---
  int acLineStatus = 0;
  int batteryFlag = 0;
  int batteryLifePercent = 0;
  long long batteryLifeTimeSec = 0;
  int batteryChargeRate = 0;
  int batteryChargeState = 0;
  int batteryWearLevel = 0;
  int batteryCycleCount = 0;
  int batteryTemperature = 0;
  int powerPlanIndex = 0;
  int powerSaverActive = 0;
  int highPerfActive = 0;
  int idlePowerDrawHigh = 0;

  // --- Thermal / Cooling ---
  double cpuCoreTempC = 0.0;
  double cpuCoreTempMax = 0.0;
  double motherboardTempC = 0.0;
  double vrmTempC = 0.0;
  double ambientTempC = 0.0;
  int cpuThrottling = 0;
  int fanCount = 0;
  std::vector<int> fanSpeeds;
  int pumpSpeed = 0;
  int thermalSensorCount = 0;
  int thermalSensorFailures = 0;
  double heatSoakBaseline = 0.0;

  // --- Voltage ---
  double voltage12V = 0.0;
  double voltage5V = 0.0;
  double voltage33V = 0.0;
  double voltageVcore = 0.0;

  // --- Hardware Board ---
  int tpmPresent = 0;
  int tpmReady = 0;
  int cmosBatteryOk = 0;
  int sensorPollFailures = 0;

  // --- Filesystem ---
  int fsSystem32FileCount = 0;
  int fsSystem32HiddenCount = 0;
  int fsSystem32SystemCount = 0;
  int fsDriversFileCount = 0;
  int fsDriversHiddenCount = 0;
  int fsRecycleBinContentCount = 0;
  int fsProgramFilesCount = 0;
  int fsVolumeDirtyBit = 0;
  std::wstring fsVolumeLabel;
  int fsReparsePointCount = 0;
  int fsSparseFileCount = 0;
  int fsAdsWithDataCount = 0;
  int fsCorruptionWarnings = 0;
  int fsChkdskPending = 0;

  // --- Registry (19 hives x 3 fields each) ---
  struct RegistryHive {
    int keyCount = 0;
    int valueCount = 0;
    unsigned long hash = 0;
  };

  RegistryHive regRun;
  RegistryHive regRunOnce;
  RegistryHive regShell;
  RegistryHive regPolicies;
  RegistryHive regServices;
  RegistryHive regDrivers;
  RegistryHive regFirewall;
  RegistryHive regUac;
  RegistryHive regTaskSched;
  RegistryHive regCom;
  RegistryHive regTelemetry;
  RegistryHive regAudit;
  RegistryHive regEnv;
  RegistryHive regPath;
  RegistryHive regAppAssoc;
  RegistryHive regShellExt;
  RegistryHive regDefApp;
  RegistryHive regFileAssoc;
  RegistryHive regConfigFile;

  // --- Other System ---
  int startupFolderCount = 0;
  int envVarCount = 0;
  unsigned long regHashEnvPath = 0;

  // --- Audio ---
  int audioOutputDeviceCount = 0;
  int audioInputDeviceCount = 0;
  int audioMixerCount = 0;
  DWORD audioMasterVolume = 0;
  int audioMasterMuted = 0;
  DWORD audioMasterVolumeLeft = 0;
  DWORD audioMasterVolumeRight = 0;
  int audioSampleRate = 0;
  int audioBitsPerSample = 0;
  int audioChannels = 0;
  int audioWaveOutOpen = 0;
  int audioWaveInOpen = 0;
  int audioServiceRunning = 0;
  int audioMicMuted = 0;
  int audioSystemMuted = 0;
  int audioHeadphoneJack = 0;
  int audioSpatialization = 0;
  int audioPlaybackActive = 0;
  int audioRecordingActive = 0;
  unsigned long audioDeviceHash = 0;
  unsigned long audioFormatHash = 0;
  int audioLatencyMs = 0;
  int audioCodecEvents = 0;
  int audioDecoderErrors = 0;

  // --- Reliability / Recovery ---
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
  int mainThreadResponsive = 0;
  int uiResponsivenessMs = 0;
  int threadHealthOk = 0;
};

} // namespace monix
