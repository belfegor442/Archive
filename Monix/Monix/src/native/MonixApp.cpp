#include "MonixApp.hpp"
#include "crash/CrashHandler.hpp"

#include "app/bootstrap/AppConstants.hpp"
#include "config/MonixConfig.hpp"
#include "core/TextUtils.hpp"
#include "settings/SettingGroups.hpp"
#include "scram/rules/CpuPressureRule.hpp"
#include "scram/rules/RamExhaustionRule.hpp"
#include "scram/rules/ThermalDriftRule.hpp"
#include "scram/rules/GpuUtilizationRule.hpp"
#include "scram/rules/GpuVramRule.hpp"
#include "scram/rules/GpuDriverRule.hpp"
#include "scram/rules/GpuThermalPowerFanRule.hpp"
#include "scram/rules/TdrStabilityRule.hpp"
#include "scram/rules/CpuIdentityRule.hpp"
#include "scram/rules/MemorySubsystemRule.hpp"
#include "scram/rules/NetworkTopologyRule.hpp"
#include "scram/rules/NetworkTrafficRule.hpp"
#include "scram/rules/NetworkLatencyRule.hpp"
#include "scram/rules/VpnTunnelRule.hpp"
#include "scram/rules/DiskStorageRule.hpp"
#include "scram/rules/ThreadSchedulingRule.hpp"
#include "scram/rules/HandleObjectRule.hpp"
#include "scram/rules/DisplayConfigRule.hpp"
#include "scram/rules/FrameTimeRule.hpp"
#include "scram/rules/ProcessTriageRule.hpp"
#include "scram/rules/OsKernelRule.hpp"
#include "scram/rules/SecurityIntegrityRule.hpp"
#include "scram/rules/PowerBatteryRule.hpp"
#include "scram/rules/ThermalCoolingRule.hpp"
#include "scram/rules/HardwareSensorsRule.hpp"
#include "scram/rules/FilesystemDataRule.hpp"
#include "scram/rules/RegistryConfigRule.hpp"
#include "scram/rules/AudioMultimediaRule.hpp"
#include "scram/rules/ReliabilityRecoveryRule.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

using namespace monix;

MonixApp::MonixApp() : paths_(ResolveAppPaths()) {
  QueryPerformanceFrequency(&qpcFrequency_);
  state_.session.id = GenerateSessionId();
  WriteDefaultConfigIfMissing();
  LoadConfig(false);
  LoadRuntimeAssets();

  std::error_code ec;
  std::filesystem::create_directories(paths_.logsDir, ec);
  std::filesystem::create_directories(paths_.exportsDir, ec);
  CleanupStaleLogFiles();
  LoadRecentLogHistory();
  auth_.Init(paths_.configFile);
  kernel_.Initialize(&auth_);

  scramEngine_.AddRule(std::make_unique<monix::CpuPressureRule>());
  scramEngine_.AddRule(std::make_unique<monix::RamExhaustionRule>());
  scramEngine_.AddRule(std::make_unique<monix::ThermalDriftRule>());
  scramEngine_.AddRule(std::make_unique<monix::GpuUtilizationRule>());
  scramEngine_.AddRule(std::make_unique<monix::GpuVramRule>());
  scramEngine_.AddRule(std::make_unique<monix::GpuDriverRule>());
  scramEngine_.AddRule(std::make_unique<monix::GpuThermalPowerFanRule>());
  scramEngine_.AddRule(std::make_unique<monix::TdrStabilityRule>());
  scramEngine_.AddRule(std::make_unique<monix::CpuIdentityRule>());
  scramEngine_.AddRule(std::make_unique<monix::MemorySubsystemRule>());
  scramEngine_.AddRule(std::make_unique<monix::NetworkTopologyRule>());
  scramEngine_.AddRule(std::make_unique<monix::NetworkTrafficRule>());
  scramEngine_.AddRule(std::make_unique<monix::NetworkLatencyRule>());
  scramEngine_.AddRule(std::make_unique<monix::VpnTunnelRule>());
  scramEngine_.AddRule(std::make_unique<monix::DiskStorageRule>());
  scramEngine_.AddRule(std::make_unique<monix::ThreadSchedulingRule>());
  scramEngine_.AddRule(std::make_unique<monix::HandleObjectRule>());
  scramEngine_.AddRule(std::make_unique<monix::DisplayConfigRule>());
  scramEngine_.AddRule(std::make_unique<monix::FrameTimeRule>());
  scramEngine_.AddRule(std::make_unique<monix::ProcessTriageRule>());
  scramEngine_.AddRule(std::make_unique<monix::OsKernelRule>());
  scramEngine_.AddRule(std::make_unique<monix::SecurityIntegrityRule>());
  scramEngine_.AddRule(std::make_unique<monix::PowerBatteryRule>());
  scramEngine_.AddRule(std::make_unique<monix::ThermalCoolingRule>());
  scramEngine_.AddRule(std::make_unique<monix::HardwareSensorsRule>());
  scramEngine_.AddRule(std::make_unique<monix::FilesystemDataRule>());
  scramEngine_.AddRule(std::make_unique<monix::RegistryConfigRule>());
  scramEngine_.AddRule(std::make_unique<monix::AudioMultimediaRule>());
  scramEngine_.AddRule(std::make_unique<monix::ReliabilityRecoveryRule>());

  monix::LogManagerConfig logConfig;
  logConfig.minLevel = config_.logLevel;
  logConfig.deduplicate = config_.logDeduplicate;
  logConfig.logMilliseconds = config_.logMilliseconds;
  logConfig.bufferSize = config_.logBufferSize;
  logConfig.historyCapacity = config_.historyCapacity;
  logConfig.sessionId = state_.session.id;
  logConfig.userId = config_.userId;
  logManager_ = std::make_unique<monix::LogManager>(logConfig);

  monix::NotificationQueueConfig nqConfig;
  nqConfig.enabled = config_.notificationsEnabled;
  nqConfig.durationMs = config_.notificationDurationMs;
  nqConfig.maxStack = config_.notificationMaxStack;
  notifQueue_ = std::make_unique<monix::NotificationQueue>(nqConfig);

  soundPlayer_ = std::make_unique<monix::SoundPlayer>(
      paths_.soundDir, config_.soundEnabled, state_.loggedIn);

  logManager_->onNotification = [this](const LogEntry& entry) {
    notifQueue_->Queue(entry);
  };
  logManager_->onSound = [this]() {
    soundPlayer_->PlayLogSound();
  };
  notifQueue_->onAlertSound = [this](int level) {
    soundPlayer_->PlayAlertSound(level);
  };

  SeedReferenceState();

  {
    auto shadersDir = paths_.rootDir / "Shaders";
    if (std::filesystem::exists(shadersDir)) {
      shaderLibrary_ = std::make_unique<monix::renderer_vk::ShaderLibrary>(shadersDir);
      shaderLibrary_->scan();

      monix::renderer_vk::ShaderRuntimeConfig rtConfig;
      rtConfig.rootDirectory = paths_.rootDir;
      rtConfig.shaderCacheDirectory = paths_.rootDir / "cache" / "shaders";
      rtConfig.slangcPath = paths_.rootDir / "tools" / "slangc.exe";
      rtConfig.includeDirectories = { shadersDir };
      std::filesystem::create_directories(rtConfig.shaderCacheDirectory);

      shaderRuntime_ = std::make_unique<monix::renderer_vk::ShaderRuntime>(std::move(rtConfig));
      shaderRuntime_->initialize();

      shaderCompiler_ = std::make_unique<monix::renderer_vk::ShaderLibraryCompiler>(*shaderLibrary_, *shaderRuntime_);
      shaderBrowserPanel_ = std::make_unique<monix::renderer_vk::ShaderBrowserPanel>(*shaderLibrary_, *shaderCompiler_, *shaderRuntime_, state_.shaderUi.panel);
    }
  }
}

MonixApp::~MonixApp() {
  StopTelemetry();
  FlushLogQueues(true);

  ShutdownOpenGlBootstrap();
  win98Assets_.Clear();
  if (win98Fonts_.msSansSerif) DeleteObject(win98Fonts_.msSansSerif);
  if (win98Fonts_.titleBar) DeleteObject(win98Fonts_.titleBar);
  if (win98Fonts_.menuFont) DeleteObject(win98Fonts_.menuFont);
  if (win98Fonts_.smallFont) DeleteObject(win98Fonts_.smallFont);
  if (win98Fonts_.logFont) DeleteObject(win98Fonts_.logFont);
  DestroyUiFonts();
  if (largeIcon_) {
    DestroyIcon(largeIcon_);
  }
  if (smallIcon_) {
    DestroyIcon(smallIcon_);
  }
  if (privateFontLoaded_) {
    if (!paths_.fontList.empty() && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
      const auto& font = paths_.fontList[state_.currentFontIndex];
      if (!font.filePath.empty()) {
        RemoveFontResourceExW(font.filePath.c_str(), FR_PRIVATE, nullptr);
      }
    } else if (std::filesystem::exists(paths_.fontFile)) {
      RemoveFontResourceExW(paths_.fontFile.c_str(), FR_PRIVATE, nullptr);
    }
  }
}

void MonixApp::WriteDefaultConfigIfMissing() const {
  monix::WriteDefaultConfigIfMissing(paths_.configFile);
}

void MonixApp::LoadConfig(bool logEvent) {
  {
    std::unique_lock<std::shared_mutex> lock(configMutex_);
    config_ = monix::LoadConfigFromFile(paths_.configFile);
  }

  std::lock_guard<std::recursive_mutex> lock(stateMutex_);
  state_.intro.active = false;

  TrimHistoryBuffers();
  if (!config_.notificationsEnabled) {
    state_.notifState.items.clear();
  } else if (static_cast<int>(state_.notifState.items.size()) > config_.notificationMaxStack) {
    state_.notifState.items.resize(config_.notificationMaxStack);
  }

  if (logEvent) {
    PushLog(
      L"CONFIG",
      L"INFO",
      L"Live parameters reloaded. Delay=" + std::to_wstring(config_.telemetryIntervalMs) +
        L"ms, font scale=" + FormatDecimal(config_.fontScale) +
        L", log lines=" + std::to_wstring(config_.logVisibleLines) +
        L", view=" + LogViewModeText(config_.logViewMode),
      ColorRole::UserInput,
      L"config",
      L"runtime",
      L"reload=true"
    );
    SetToast(L"monix.ini reloaded");
  }
}

void MonixApp::SaveConfig() const {
  monix::SaveConfigToFile(paths_.configFile, config_);
}

void MonixApp::TrimHistoryBuffers() {
  const auto trim = [this](std::vector<double>& values) {
    if (config_.historyCapacity > 0 && static_cast<int>(values.size()) > config_.historyCapacity) {
      values.erase(values.begin(), values.begin() + (values.size() - config_.historyCapacity));
    }
  };

  trim(state_.history.cpu);
  trim(state_.history.ram);
  trim(state_.history.gpu);
  trim(state_.history.netUpload);
  trim(state_.history.net);
  trim(state_.history.latency);
  trim(state_.logState.warnHistory);
  trim(state_.logState.errHistory);
  trim(state_.logState.critHistory);
  trim(state_.logState.kernelHistory);
  trim(state_.logState.netHistory);
}

int MonixApp::Run(HINSTANCE instance, int showCommand) {
  AddVectoredExceptionHandler(0, CrashVehHandler);
  instance_ = instance;
  if (!CreateMainWindow(instance, showCommand)) {
    return 1;
  }

  {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    if (openGl_.available) {
      PushLog(L"OPENGL", L"INFO", L"Window context ready: " + openGl_.version + L" on " + openGl_.renderer + L".", ColorRole::UserInput, L"opengl", L"bootstrap", L"ready=true");
      PushLog(L"OPENGL", L"INFO", openGl_.status, ColorRole::Primary, L"opengl", L"postprocess", L"ready=true");
    } else {
      PushLog(L"OPENGL", L"WARNING", L"Window OpenGL context is not available yet. Falling back to raw GDI.", ColorRole::Warning, L"opengl", L"bootstrap", L"ready=false");
    }
  }

  StartTelemetry();
  SetFrameTimer();

  MSG message {};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return static_cast<int>(message.wParam);
}

void MonixApp::SetFrameTimer() {
  if (!hwnd_) {
    return;
  }
  KillTimer(hwnd_, kFrameTimerId);
  SetTimer(hwnd_, kFrameTimerId, config_.frameIntervalMs, nullptr);
}

void MonixApp::TransitionToLoggedIn() {
  state_.loggedIn = true;
  state_.intro.active = config_.introEnabled;
  PushLog(L"SYSTEM", L"SUCCESS", L"Kernel initialized — terminal unlocked", ColorRole::Success);
}
