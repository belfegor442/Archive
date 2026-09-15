#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>
#include <windowsx.h>

#include "vulkan_renderer.h"
#include "render_backend.h"
#include "renderer_vk/public/ShaderRenderer.hpp"
#include "renderer_vk/opengl/GlBackend.hpp"

#include <mmsystem.h>
#include <shellscalingapi.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <iptypes.h>
#include <netioapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <wintrust.h>
#include <softpub.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <wincrypt.h>
#include <powrprof.h>
#include <pdh.h>
#include <unordered_map>
#include <comdef.h>
#include <Wbemidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <iomanip>
#include <string>
#include <thread>
#include <vector>


#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "gdiplus.lib")

#include "../../login/AuthManager.hpp"
#include "../../login/MonixKernel.hpp"
#include "../../login/KernelDisplay.hpp"
#include "../../sensors/sensors.h"
#include "renderer_vk/ui/ShaderBrowserPanel.hpp"
#include "settings/SettingsRegistry.hpp"
#include "core/TextUtils.hpp"
#include "telemetry/Snapshot.hpp"
#include "telemetry/Collectors.hpp"
#include "logging/LogEntry.hpp"
#include "ui/AppState.hpp"
#include "ui/CoreMonitorTheme.hpp"
#include "ui/Win98Theme.hpp"
#include "config/MonixConfig.hpp"

using namespace monix;

namespace {

constexpr UINT_PTR kFrameTimerId = 1;
constexpr UINT WM_MONIX_UPDATE = WM_APP + 1;
constexpr int kMainIconResourceId = 101;
constexpr int kWindowMinWidth = 1320;
constexpr int kWindowMinHeight = 840;
constexpr int kTaskMenuWidth = 320;
constexpr int kTaskMenuItemHeight = 38;
constexpr int kNotificationWidth = 370;
constexpr int kNotificationHeight = 82;





// MULTI-PASS SHADER PIPELINE
// ============================================================================

std::string ReadTextFile(const std::filesystem::path& path);

// ============================================================================
// OPENGL ERROR CHECKING (Pipeline)
// ============================================================================


// Create a 1x1 black placeholder texture (for missing external textures)








static std::string TrimQuoted(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n\"");
  size_t end = s.find_last_not_of(" \t\r\n\"");
  if (start == std::string::npos) return "";
  return s.substr(start, end - start + 1);
}




#include "runtime_state.h"
#include "Win32RAII.hpp"


struct OpenGlState {
  bool available = false;
  bool functionsLoaded = false;
  HWND hwnd = nullptr;
  HDC dc = nullptr;
  HGLRC rc = nullptr;
  // Vulkan renderer
  VulkanRenderer vk;
  bool vkAvailable = false;
  bool vkFailed = false;
  // Vulkan shader preset renderer
  std::unique_ptr<monix::renderer_vk::ShaderRenderer> shaderRenderer;
  bool presetLoaded = false;
  // OpenGL backend for GLSL presets (.glslp)
  monix::renderer_vk::GlBackend glBackend;
  bool glPresetActive = false;
  HDC uiDc = nullptr;
  HBITMAP uiBitmap = nullptr;
  HGDIOBJ oldUiBitmap = nullptr;
  void* uiPixels = nullptr;
  int uiWidth = 0;
  int uiHeight = 0;
  std::uint64_t frameCount = 0;
  float elapsedTime = 0.0f;
  std::wstring vendor = L"Unavailable";
  std::wstring renderer = L"Unavailable";
  std::wstring version = L"Unavailable";
  std::wstring status = L"Pending";
  int debugViewMode = 0;
  int debugPassIndex = 0;
  bool screenshotRequested = false;
  int screenshotWidth = 0;
  int screenshotHeight = 0;
};

struct AppState {
  Tab activeTab = Tab::Log;
  int coreMonitorMenuIndex = 0;
  Snapshot snapshot;
  std::unique_ptr<Snapshot> previousSnapshot;
  bool hasPreviousSnapshot = false;
  std::vector<LogEntry> logs;
  SessionCounters counters;
  bool initialized = false;
  int sampleCount = 0;
  int logScroll = 0;
  int taskScroll = 0;
  int selectedTaskIndex = 0;
  int trackedPid = 0;
  std::wstring sessionId;
  std::uint64_t nextEventId = 1;
  std::wstring scramHeadline = L"Passive analysis engine waiting for telemetry.";
  std::wstring scramInsight = L"No anomalies detected yet.";
  std::vector<std::wstring> scramDiagnostics;
  int scramRiskScore = 12;
  std::vector<double> cpuHistory;
  std::vector<double> ramHistory;
  std::vector<double> gpuHistory;
  std::vector<double> netUploadHistory;
  std::vector<double> netHistory;
  std::vector<double> latencyHistory;
  std::vector<double> logWarnHistory;
  std::vector<double> logErrHistory;
  std::vector<double> logCritHistory;
  std::vector<double> logKernelHistory;
  std::vector<double> logNetHistory;
  std::map<int, std::wstring> knownProcesses;
  std::map<int, int> processSeenCount;
  std::map<int, int> processGoneCount;
  bool processEventsInitialized = false;
  double prevScramRisk = 0.0;
  double smoothedScramRisk = 8.0;
  int scramSeverityHold = 0;
  int scramCurrentSeverity = 0;
  int scramErrorHold = 0;
  int previousHandleCount = 0;
  std::map<std::wstring, int> knownFlowCounts;
  std::map<std::wstring, ULONGLONG> flowFirstDeltaMs;
  std::map<std::wstring, int> flowBurstStart;
  std::map<std::wstring, int> flowBurstEnd;
  std::vector<int> pingRttSamples;
  double pingJitterStddev = 0.0;
  std::vector<NotificationItem> notifications;
  std::wstring toastMessage;
  ULONGLONG toastUntilMs = 0;
  IntroState intro;
  ContextMenuState taskMenu;
  int selectedSettingIndex = 0;
  int currentShaderIndex = 0;
  monix::renderer_vk::ShaderBrowserPanelState shaderBrowser;
  std::wstring shaderSearchText;
  bool shaderSearchFocused = false;
  bool shaderShowFavoritesOnly = false;
  int currentFontIndex = 0;
  LogFilter activeLogFilter = LogFilter::All;
  bool livePaused = false;
  bool loggedIn = false;
  RECT viewport_ { 0, 0, 0, 0 };
  bool clickDebugMode = false;
  POINT clickDebugWnd { -1, -1 };
  POINT clickDebugBmp { -1, -1 };
  ULONGLONG clickDebugMs = 0;
  UpdateState updateState;
};

bool HasRepoMarkers(const std::filesystem::path& candidate) {
  return std::filesystem::exists(candidate / "src" / "native" / "collect.ps1") &&
         std::filesystem::exists(candidate / "tools" / "vhs-gothic.ttf");
}

AppPaths ResolveAppPaths() {
  wchar_t modulePath[MAX_PATH] {};
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  const std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();

  std::vector<std::filesystem::path> candidates;
  candidates.push_back(exeDir);
  if (!exeDir.parent_path().empty()) {
    candidates.push_back(exeDir.parent_path());
  }
  candidates.push_back(std::filesystem::current_path());
  if (!std::filesystem::current_path().parent_path().empty()) {
    candidates.push_back(std::filesystem::current_path().parent_path());
  }

  AppPaths paths;
  for (const auto& candidate : candidates) {
    if (HasRepoMarkers(candidate)) {
      paths.rootDir = candidate;
      break;
    }
  }

  if (paths.rootDir.empty()) {
    paths.rootDir = !exeDir.parent_path().empty() ? exeDir.parent_path() : exeDir;
  }

  paths.collectorScript = paths.rootDir / "src" / "native" / "collect.ps1";

  paths.shaderFiles.clear();
  auto shadersDir = paths.rootDir / "Shaders";
  if (std::filesystem::exists(shadersDir)) {
    for (auto& entry : std::filesystem::recursive_directory_iterator(shadersDir, std::filesystem::directory_options::skip_permission_denied)) {
      if (!entry.is_regular_file()) continue;
      auto ext = entry.path().extension().string();
      for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (ext == ".slangp" || ext == ".glslp") {
        paths.shaderFiles.push_back(entry.path());
      }
    }
    std::sort(paths.shaderFiles.begin(), paths.shaderFiles.end());
  }
  if (!paths.shaderFiles.empty()) {
    paths.crtShaderFile = paths.shaderFiles[0];
    for (auto& f : paths.shaderFiles) {
      if (f.stem() == "crt-geom") {
        paths.crtShaderFile = f;
        break;
      }
    }
  }
  paths.fontFile = paths.rootDir / "tools" / "vhs-gothic.ttf";
  paths.fontsDir = paths.rootDir / "fonts";
  paths.fontList = ScanFontDirectory(paths.fontsDir);
  if (paths.fontList.empty()) {
    paths.fontList.push_back({ L"Terminal", L"", L"Terminal" });
  }
  paths.soundDir = paths.rootDir / "Sound";
  paths.introWave = paths.rootDir / "intro.wav";
  paths.iconFile = paths.rootDir / "ico.ico";
  paths.configFile = paths.rootDir / "monix.ini";
  paths.logsDir = paths.rootDir / "logs";
  paths.exportsDir = paths.rootDir / "exports";
  const auto bordersDir = paths.rootDir / "borders";
  if (std::filesystem::exists(bordersDir) && std::filesystem::is_directory(bordersDir)) {
    for (const auto& entry : std::filesystem::directory_iterator(bordersDir)) {
      if (entry.is_regular_file()) {
        auto ext = entry.path().extension().wstring();
        for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
        if (ext == L".png" || ext == L".jpg" || ext == L".bmp") {
          paths.borderFiles.push_back(entry.path());
        }
      }
    }
    std::sort(paths.borderFiles.begin(), paths.borderFiles.end());
  }
  return paths;
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return "";
  }

  std::ostringstream stream;
  stream << input.rdbuf();
  return stream.str();
}

std::string StripUnsupportedShaderDirectives(const std::string& source) {
  std::istringstream input(source);
  std::ostringstream output;
  std::string line;
  bool firstLine = true;
  while (std::getline(input, line)) {
    if (firstLine && line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
      line.erase(0, 3);
    }
    firstLine = false;
    if (line.rfind("#pragma parameter", 0) == 0) {
      continue;
    }
    output << line << "\n";
  }
  return output.str();
}

std::string BuildLottesShaderSource(const std::filesystem::path& path, bool vertexShader) {
  const std::string filtered = StripUnsupportedShaderDirectives(ReadTextFile(path));
  if (filtered.empty()) {
    return "";
  }

  std::ostringstream output;
  // No #version - GLSL 110 compat (AMD compat profile forces all versions to 110)
  output << "#define PARAMETER_UNIFORM 1\n";
  output << (vertexShader ? "#define VERTEX 1\n" : "#define FRAGMENT 1\n");
  output << filtered;
  return output.str();
}

// Load Monix Common GLSL modules (geometry, common, normals) and return them
// as a single string. These are prepended to CRT shaders so they can use
// the Monix curvature, vignette, corner mask, and normal mapping functions.
std::string LoadCommonModules(const std::filesystem::path& rootDir) {
  static std::string cached;
  static std::filesystem::path cachedRootDir;
  static std::atomic<bool> loaded{false};
  if (loaded.load(std::memory_order_acquire) && cachedRootDir == rootDir) return cached;

  auto commonDir = rootDir / "Shaders" / "CRT" / "Common";
  std::ostringstream out;

  // Order matters: geometry first (standalone), common next (uses geometry types),
  // normals last (standalone, for lighting)
  const wchar_t* files[] = {
    L"geometry.glsl",
    L"common.glsl",
    L"normals.glsl"
  };
  for (const auto& f : files) {
    auto path = commonDir / f;
    std::string src = ReadTextFile(path);
    if (!src.empty()) {
      // Convert wchar_t filename to char for ostringstream
      char fname[64];
      size_t n = 0;
      for (const wchar_t* p = f; *p && n < 63; ++p) fname[n++] = static_cast<char>(*p);
      fname[n] = '\0';
      out << "// === " << fname << " ===\n";
      out << src << "\n";
    }
  }
  cached = out.str();
  cachedRootDir = rootDir;
  loaded.store(true, std::memory_order_release);
  return cached;
}

// Prepend Common modules to a fragment shader source.
// Inserts modules after the #version line (or at the start if no #version).
std::string PrependCommonModules(const std::string& shaderSource,
                                 const std::filesystem::path& rootDir) {
  std::string modules = LoadCommonModules(rootDir);
  if (modules.empty()) return shaderSource;

  // Find where #version ends (first newline after #version)
  std::string versionPrefix;
  std::string rest = shaderSource;
  auto pos = shaderSource.find("#version");
  if (pos != std::string::npos) {
    auto eol = shaderSource.find('\n', pos);
    if (eol != std::string::npos) {
      versionPrefix = shaderSource.substr(0, eol + 1);
      rest = shaderSource.substr(eol + 1);
    }
  }
  return versionPrefix + "// --- Monix Common Modules ---\n" + modules + "\n" + rest;
}

std::wstring JoinPathForCommand(const std::filesystem::path& path) {
  return L"\"" + path.wstring() + L"\"";
}

std::wstring GenerateSessionId() {
  SYSTEMTIME time {};
  GetLocalTime(&time);
  wchar_t buffer[96];
  swprintf(
    buffer,
    96,
    L"MONIX-%04d%02d%02d-%02d%02d%02d-%lu",
    time.wYear,
    time.wMonth,
    time.wDay,
    time.wHour,
    time.wMinute,
    time.wSecond,
    GetCurrentProcessId()
  );
  return buffer;
}

static std::wstring FormatIpv4(DWORD address) {
  IN_ADDR addr {};
  addr.S_un.S_addr = address;
  char buffer[INET_ADDRSTRLEN] {};
  if (!inet_ntop(AF_INET, &addr, buffer, static_cast<DWORD>(sizeof(buffer)))) {
    return L"0.0.0.0";
  }
  return Utf8ToWide(buffer);
}

std::array<std::wstring, 8> IntroLogo() {
  return {
    L" ██████   ██████                      ███             ",
    L"░░██████ ██████                      ░░░              ",
    L" ░███░█████░███   ██████  ████████   ████  █████ █████",
    L" ░███░░███ ░███  ███░░███░░███░░███ ░░███ ░░███ ░░███ ",
    L" ░███ ░░░  ░███ ░███ ░███ ░███ ░███  ░███  ░░░█████░  ",
    L" ░███      ░███ ░███ ░███ ░███ ░███  ░███   ███░░░███ ",
    L" █████     █████░░██████  ████ █████ █████ █████ █████",
    L"░░░░░     ░░░░░  ░░░░░░  ░░░░ ░░░░░ ░░░░░ ░░░░░ ░░░░░ "
  };
}

const std::array<SettingId, 10>& RuntimeSettings() {
  static const std::array<SettingId, 10> settings {
    SettingId::TelemetryIntervalMs,
    SettingId::FrameIntervalMs,
    SettingId::FontScale,
    SettingId::IntroEnabled,
    SettingId::NotificationsEnabled,
    SettingId::SoundEnabled,
    SettingId::NotificationDurationMs,
    SettingId::NotificationMaxStack,
    SettingId::AnalyticsHistoryEnabled,
    SettingId::HistoryCapacity
  };
  return settings;
}

const std::array<SettingId, 10>& LoggingSettings() {
  static const std::array<SettingId, 10> settings {
    SettingId::LogVisibleLines,
    SettingId::LogLevel,
    SettingId::LogViewMode,
    SettingId::LogMilliseconds,
    SettingId::LogJsonEnabled,
    SettingId::LogPlainEnabled,
    SettingId::LogFlushIntervalMs,
    SettingId::LogRetentionDays,
    SettingId::LogDeduplicate,
    SettingId::PauseLiveLogs
  };
  return settings;
}

const std::array<SettingId, 11>& FullCrtSettings() {
  static const std::array<SettingId, 11> settings {
    SettingId::CrtEnabled,
    SettingId::CrtCurvatureStrength,
    SettingId::CrtScanlineIntensity,
    SettingId::CrtScanlineSpacing,
    SettingId::CrtChromaticAberration,
    SettingId::CrtPhosphorGlow,
    SettingId::CrtFlickerAmount,
    SettingId::CrtNoiseAmount,
    SettingId::CrtVignetteStrength,
    SettingId::CrtSharpness,
    SettingId::CrtGhostOffset
  };
  return settings;
}

const std::array<SettingId, 5>& AdvancedCrtSettings() {
  static const std::array<SettingId, 5> settings {
    SettingId::CrtRgbShift,
    SettingId::CrtGrain,
    SettingId::CrtJitter,
    SettingId::CrtSubpixelMode,
    SettingId::CrtBloom
  };
  return settings;
}

const std::array<SettingId, 3>& BurnInSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::CrtBurnInEnabled,
    SettingId::CrtBurnInIntensity,
    SettingId::CrtBurnInDecayRate
  };
  return settings;
}

const std::array<SettingId, 6>& BorderSettings() {
  static const std::array<SettingId, 6> settings {
    SettingId::BorderEnabled,
    SettingId::BorderImagePreset,
    SettingId::BorderChromaKeyR,
    SettingId::BorderChromaKeyG,
    SettingId::BorderChromaKeyB,
    SettingId::BorderChromaKeyTolerance
  };
  return settings;
}

const std::array<SettingId, 3>& IntroAnimSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::IntroStepPx,
    SettingId::IntroHoldMs,
    SettingId::IntroCreditDelayMs
  };
  return settings;
}

const std::array<SettingId, 2>& ThemeSettings() {
  static const std::array<SettingId, 2> settings {
    SettingId::AccentColorPreset,
    SettingId::FontFaceIndex
  };
  return settings;
}

const std::array<SettingId, 4>& WindowStartupSettings() {
  static const std::array<SettingId, 4> settings {
    SettingId::DefaultTab,
    SettingId::MinimizeToTray,
    SettingId::StartMaximized,
    SettingId::SaveWindowPosition
  };
  return settings;
}

const std::array<SettingId, 2>& HotkeySettings() {
  static const std::array<SettingId, 2> settings {
    SettingId::HotkeyToggleCrt,
    SettingId::HotkeyReloadConfig
  };
  return settings;
}

const std::array<SettingId, 4>& ColorCorrectionSettings() {
  static const std::array<SettingId, 4> settings {
    SettingId::CrtBrightness,
    SettingId::CrtContrast,
    SettingId::CrtSaturation,
    SettingId::CrtGamma
  };
  return settings;
}

const std::array<SettingId, 2>& ScanlineStyleSettings() {
  static const std::array<SettingId, 2> settings {
    SettingId::CrtScanlineMode,
    SettingId::CrtInterlace
  };
  return settings;
}

const std::array<SettingId, 3>& GeneralExtendedSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::Language,
    SettingId::ShowFps,
    SettingId::WindowOpacity
  };
  return settings;
}

const std::array<SettingId, 3>& SystemExtendedSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::AlwaysOnTop,
    SettingId::StartWithWindows,
    SettingId::ThemeMode
  };
  return settings;
}

const std::array<SettingId, 3>& PerformanceSettings() {
  static const std::array<SettingId, 3> settings {
    SettingId::FrameTargetFps,
    SettingId::VSyncEnabled,
    SettingId::ProcessPriority
  };
  return settings;
}

const std::array<SettingId, 71>& EditableSettings() {
  static const std::array<SettingId, 71> settings {
    SettingId::TelemetryIntervalMs,
    SettingId::FrameIntervalMs,
    SettingId::FontScale,
    SettingId::IntroEnabled,
    SettingId::IntroStepPx,
    SettingId::IntroHoldMs,
    SettingId::IntroCreditDelayMs,
    SettingId::NotificationsEnabled,
    SettingId::SoundEnabled,
    SettingId::NotificationDurationMs,
    SettingId::NotificationMaxStack,
    SettingId::AnalyticsHistoryEnabled,
    SettingId::HistoryCapacity,
    SettingId::LogVisibleLines,
    SettingId::LogLevel,
    SettingId::LogViewMode,
    SettingId::LogMilliseconds,
    SettingId::LogJsonEnabled,
    SettingId::LogPlainEnabled,
    SettingId::LogFlushIntervalMs,
    SettingId::LogRetentionDays,
    SettingId::LogDeduplicate,
    SettingId::PauseLiveLogs,
    SettingId::CrtEnabled,
    SettingId::CrtCurvatureStrength,
    SettingId::CrtScanlineIntensity,
    SettingId::CrtScanlineSpacing,
    SettingId::CrtChromaticAberration,
    SettingId::CrtPhosphorGlow,
    SettingId::CrtFlickerAmount,
    SettingId::CrtNoiseAmount,
    SettingId::CrtVignetteStrength,
    SettingId::CrtSharpness,
    SettingId::CrtGhostOffset,
    SettingId::CrtRgbShift,
    SettingId::CrtGrain,
    SettingId::CrtJitter,
    SettingId::CrtSubpixelMode,
    SettingId::CrtBloom,
    SettingId::CrtBurnInEnabled,
    SettingId::CrtBurnInIntensity,
    SettingId::CrtBurnInDecayRate,
    SettingId::BorderEnabled,
    SettingId::BorderChromaKeyR,
    SettingId::BorderChromaKeyG,
    SettingId::BorderChromaKeyB,
    SettingId::BorderChromaKeyTolerance,
    SettingId::BorderImagePreset,
    SettingId::AccentColorPreset,
    SettingId::FontFaceIndex,
    SettingId::DefaultTab,
    SettingId::MinimizeToTray,
    SettingId::StartMaximized,
    SettingId::SaveWindowPosition,
    SettingId::AlwaysOnTop,
    SettingId::StartWithWindows,
    SettingId::ThemeMode,
    SettingId::HotkeyToggleCrt,
    SettingId::HotkeyReloadConfig,
    SettingId::CrtBrightness,
    SettingId::CrtContrast,
    SettingId::CrtSaturation,
    SettingId::CrtGamma,
    SettingId::CrtScanlineMode,
    SettingId::CrtInterlace,
    SettingId::Language,
    SettingId::ShowFps,
    SettingId::WindowOpacity,
    SettingId::FrameTargetFps,
    SettingId::VSyncEnabled,
    SettingId::ProcessPriority
  };
  return settings;
}


class MonixApp {
 public:
  MonixApp();
  ~MonixApp();

  int Run(HINSTANCE instance, int showCommand);
  void SetTestMode() { testMode_ = true; }
  void SetParityMode(bool capture, bool compare, const std::wstring& refDir) {
    parityCapture_ = capture;
    parityCompare_ = compare;
    parityRefDir_ = refDir;
  }
  void SetRuntimeStateMode(bool enabled, const std::wstring& retroarchSnap) {
    runtimeStateEnabled_ = enabled;
    retroarchSnapshotPath_ = retroarchSnap;
  }
  void SetRegressionTestMode(bool enabled, const std::wstring& presetsDir, int frames) {
    regressionTestMode_ = enabled;
    regressionPresetsDir_ = presetsDir;
    regressionFrames_ = frames;
  }

 private:
  static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

  bool CreateMainWindow(HINSTANCE instance, int showCommand);
  bool InitializeOpenGlBootstrap();
  void ShutdownOpenGlBootstrap();
  void LoadConfig(bool logEvent);
  void SaveConfig() const;
  void WriteDefaultConfigIfMissing() const;
  void LoadRuntimeAssets();
  void SwitchFont(int direction);
  void LoadRecentLogHistory();
  void CreateUiFonts();
  void DestroyUiFonts();
  void MeasureFontMetrics();
  bool EnsureOpenGlUiSurface(int width, int height);
  void DestroyOpenGlUiSurface();
  void DestroyOpenGlResources();
  bool RenderOpenGlFrame(const RECT& clientRect);
  void SetFrameTimer();
  void FlushLogQueues(bool force);
  void CleanupLogFiles();
  void QueueNotification(const LogEntry& entry);
  void PlayAlertSound(LogLevel level);
  void PlayLogSound();
  void PlayClickSound();
  void AppendHistoryPoint(const Snapshot& snapshot);
  void ExportLogsJson() const;
  void ExportLogsCsv() const;
  std::filesystem::path ResolveLogFilePath(const std::wstring& extension) const;

  void StartTelemetry();
  void StopTelemetry();
public:
  void TelemetryLoop();
private:
  Snapshot PollSnapshot();
  Snapshot PollSnapshotWithState(std::unique_ptr<Snapshot> prevSnap, int sampleCount);
  std::vector<ProcessInfo> BuildFallbackProcesses(const Snapshot& snapshot);
  std::string RunProcessCapture(const std::wstring& commandLine) const;
  void ConsumeSnapshot(Snapshot snapshot);
  void PushLog(
    std::wstring domain,
    std::wstring severity,
    std::wstring message,
    ColorRole color,
    std::wstring module = L"core",
    std::wstring service = L"monix",
    std::wstring metadata = L""
  );
  void IncrementCounter(ColorRole color);
  void UpdateScramSummary(const Snapshot& snapshot);
  void SeedReferenceState();
  void RequestRefresh();
  void SetToast(const std::wstring& message);
  void TrimHistoryBuffers();
  SettingMutation AdjustSetting(SettingId id, int direction);
  void CommitSettingMutation(const SettingMutation& mutation);
  std::wstring SettingLabel(SettingId id) const;
  std::wstring SettingValueText(SettingId id) const;
  std::vector<SettingActionRect> BuildSettingActionRects(const RECT& clientRect) const;
  bool HandleSettingsClick(const RECT& clientRect, POINT point);
  bool HandleSettingsKey(WPARAM key);

  bool HitTestTabs(const RECT& clientRect, POINT point, Tab& tab) const;
  int HitTestTaskRow(const RECT& clientRect, POINT point) const;
  void OpenTaskMenu(const RECT& clientRect, POINT point, int processIndex);
  bool HandleTaskMenuClick(POINT point);
  void ExecuteTaskMenuAction(int itemIndex);
  void SpawnDemoLogs();
  void CopyToClipboard(const std::wstring& text) const;
  bool ApplyPriorityToProcess(int pid, DWORD priorityClass);
  bool TerminateProcessById(int pid);
  void TickAnimations(const RECT& clientRect);
  void ComputeViewport(const RECT& client);
  POINT MapToViewport(POINT pt) const;
  void ComputeIntroLogoMetrics(const RECT& clientRect, int& cellW, int& cellH, int& logoWidth, int& logoHeight, int& maxColumns) const;

  void Render(HDC dc, const RECT& clientRect);
  void RenderCoreMonitorTheme(HDC dc, const RECT& clientRect);
  void RenderWin98Theme(HDC dc, const RECT& clientRect);
  bool IsCoreMonitorThemeActive() const;
  bool IsWin98ThemeActive() const;
  void DrawBackground(HDC dc, const RECT& clientRect);
  void DrawSparkline(HDC dc, const RECT& rect, const std::vector<double>& values, double maxValue, ColorRole color) const;
  void DrawTabs(HDC dc, const RECT& clientRect);
  void DrawFooter(HDC dc, const RECT& clientRect);
  void DrawToast(HDC dc, const RECT& clientRect);
  void DrawClickDebug(HDC dc, const RECT& clientRect);
  void DrawNotifications(HDC dc, const RECT& clientRect);
  void DrawIntro(HDC dc, const RECT& clientRect);
  void DrawPanel(HDC dc, const RECT& rect, const std::wstring& title, ColorRole accent, HFONT titleFont) const;
  void DrawTextRect(HDC dc, const RECT& rect, const std::wstring& text, ColorRole color, HFONT font, UINT format) const;
  void DrawTextLine(HDC dc, int x, int y, int width, const std::wstring& text, ColorRole color, HFONT font, UINT format = 0) const;
  void DrawProgressBar(HDC dc, const RECT& rect, double pct, ColorRole accent) const;
  void DrawLogView(HDC dc, const RECT& clientRect);
  void DrawLogToolbar(HDC dc, const RECT& clientRect);
  void DrawLogFilterBadges(HDC dc, const RECT& clientRect);
  void DrawLogColumnHeaders(HDC dc, const RECT& logArea);
  void DrawLogSidebar(HDC dc, const RECT& sidebarRect);
  bool HitTestLogToolbar(const RECT& clientRect, POINT point);
  bool HitTestLogFilters(const RECT& clientRect, POINT point);
  std::array<RECT, 6> LogFilterRects(const RECT& clientRect) const;
  LogToolbarRect LogToolbarRects(const RECT& clientRect) const;
  int CountFilteredLogs() const;
  void DrawTasksView(HDC dc, const RECT& clientRect);
  void DrawHardwareView(HDC dc, const RECT& clientRect);
  void DrawNetworkView(HDC dc, const RECT& clientRect);
  void DrawScramView(HDC dc, const RECT& clientRect);
  void DrawSettingsView(HDC dc, const RECT& clientRect);
  void DrawTaskContextMenu(HDC dc) const;
  RECT ContentRect(const RECT& clientRect) const;
  std::array<RECT, 6> TabRects(const RECT& clientRect) const;
  std::wstring ComposeLogLine(const LogEntry& entry) const;
  int VisibleLogLines(const RECT& logRect) const;
  int VisibleTaskRows(const RECT& tableRect) const;

  HWND hwnd_ = nullptr;
  winraii::GdiIcon largeIcon_;
  winraii::GdiIcon smallIcon_;
  winraii::GdiFont tabFont_;
  winraii::GdiFont bodyFont_;
  winraii::GdiFont smallFont_;
  winraii::GdiFont titleFont_;
  winraii::GdiFont logFont_;
  winraii::GdiFont logoFont_;
  int bodyLineHeight_ = 32;
  int smallLineHeight_ = 24;
  int logLineHeight_ = 28;
  int logoLineHeight_ = 34;
  HINSTANCE instance_ = nullptr;
  Monix::Security::AuthManager auth_;
  Monix::Kernel::MonixKernel kernel_;
  Monix::Kernel::KernelDisplay kernelDisplay_;
  std::unique_ptr<monix::renderer_vk::ShaderLibrary> shaderLibrary_;
  std::unique_ptr<monix::renderer_vk::ShaderRuntime> shaderRuntime_;
  std::unique_ptr<monix::renderer_vk::ShaderLibraryCompiler> shaderCompiler_;
  std::unique_ptr<monix::renderer_vk::ShaderBrowserPanel> shaderBrowserPanel_;
  monix::renderer_vk::TransactionalShaderState txState_;
  int dpiScale_ = 96;
  AppPaths paths_;
  Config config_;
  std::wstring fontFace_ = L"Terminal";
  bool privateFontLoaded_ = false;
  ULONGLONG lastLogFlushAtMs_ = 0;
  ULONGLONG lastRetentionSweepAtMs_ = 0;
  std::vector<std::wstring> pendingPlainLogs_;
  std::vector<std::wstring> pendingJsonLogs_;
  std::atomic<bool> running_ = false;
  std::atomic<bool> refreshRequested_ = false;
  bool testMode_ = false;
  bool parityCapture_ = false;
  bool parityCompare_ = false;
  std::wstring parityRefDir_;
  bool runtimeStateEnabled_ = false;
  std::wstring retroarchSnapshotPath_;
  bool regressionTestMode_ = false;
  std::wstring regressionPresetsDir_;
  int regressionFrames_ = 3;
  winraii::ThreadHandle telemetryHandle_;
  OpenGlState openGl_;
  ULONG_PTR gdiplusToken_ = 0;
  monix::ui::Win98ThemeFonts win98Fonts_;
  monix::ui::Win98Assets win98Assets_;
  bool win98AssetsLoaded_ = false;
  int win98SettingsCategory_ = 0;
  int win98SelectedTaskPid_ = 0;
  int currentBorderIndex_ = 0;
  LARGE_INTEGER qpcFrequency_ {};
  LARGE_INTEGER lastNativeSampleQpc_ {};
  bool nativeBaselineReady_ = false;
  std::uint64_t lastSystemIdleTime_ = 0;
  std::uint64_t lastSystemKernelTime_ = 0;
  std::uint64_t lastSystemUserTime_ = 0;
  std::uint64_t lastNetworkInBytes_ = 0;
  std::uint64_t lastNetworkOutBytes_ = 0;
  std::uint64_t lastDiskReadBytes_ = 0;
  std::uint64_t lastDiskWriteBytes_ = 0;
  std::map<int, NativeProcessSample> previousProcessSamples_;
  CpuInfo cpuBaseline_ = {};
  bool cpuBaselineCaptured_ = false;
  uint64_t cpuBaseTscPerSec_ = 0;
  uint64_t prevCpuTsc_ = 0;
  uint64_t lastPageFaultCount_ = 0;
  uint64_t lastDiskReadBytesPerSec_ = 0;
  uint64_t lastDiskWriteBytesPerSec_ = 0;
  uint64_t lastDiskReadIops_ = 0;
  uint64_t lastDiskWriteIops_ = 0;
  CpuTimes prevCpuTimes_ = {};
  bool cpuTimesInitialized_ = false;
  std::set<std::wstring> prevNetAdapterAddresses_;
  std::set<std::wstring> prevNetAdapterGateways_;
  std::set<DWORD> prevNetAdapterSpeeds_;
  std::set<DWORD> prevNetAdapterOperStatuses_;
  std::set<DWORD> prevNetAdapterTypes_;
  uint64_t prevRouteTableHash_ = 0;
  int prevProxyEnabled_ = -1;
  std::wstring prevProxyServer_;
  uint64_t prevTotalContextSwitches_ = 0;
  uint64_t prevTotalInterruptCount_ = 0;
  uint64_t prevTotalDpcCount_ = 0;
  uint64_t prevTotalIsrCount_ = 0;
  int prevSuspendedThreadCount_ = 0;
  int prevReadyThreadCount_ = 0;
  std::set<std::wstring> prevDriverNames_;
  unsigned long long prevTotalHandles_ = 0;
  unsigned long long prevTotalObjects_ = 0;
  unsigned long long prevPageFaultsDelta_ = 0;
  unsigned long long prevIoReadBytesDelta_ = 0;
  unsigned long long prevIoWriteBytesDelta_ = 0;
  unsigned long long prevSystemTime100ns_ = 0;
  int prevSessionCount_ = 0;
  int prevSelfSignatureValid_ = -1;
  int prevSelfHashVerified_ = -1;
  int prevUnsignedDriverCount_ = 0;
  int prevSuspiciousScriptHosts_ = 0;
  int prevUacConsentProcesses_ = 0;
  int prevLsassAccessCount_ = 0;
  int prevDebugPortActive_ = 0;
  int prevHookModulesDetected_ = 0;
  int prevPeHeaderTamper_ = 0;
  int prevScheduledTaskCount_ = 0;
  std::set<std::wstring> prevUnsignedDriverNames_;
  std::set<std::wstring> prevSuspiciousModules_;
  int prevAcLineStatus_ = -1;
  int prevBatteryFlag_ = -1;
  int prevBatteryLifePercent_ = -1;
  long long prevBatteryLifeTimeSec_ = -1;
  int prevBatteryChargeRate_ = 0;
  int prevBatteryChargeState_ = 0;
  int prevBatteryWearLevel_ = -1;
  int prevBatteryCycleCount_ = -1;
  int prevBatteryTemperature_ = -1;
  int prevPowerPlanIndex_ = -1;
  int prevPowerSaverActive_ = 0;
  int prevHighPerfActive_ = 0;
  int prevIdlePowerDrawHigh_ = 0;
  double prevCpuCoreTempC_ = 0.0;
  double prevCpuCoreTempMax_ = 0.0;
  double prevMotherboardTempC_ = 0.0;
  double prevVrmTempC_ = 0.0;
  double prevAmbientTempC_ = 0.0;
  int prevCpuThrottling_ = 0;
  int prevFanCount_ = 0;
  std::vector<int> prevFanSpeeds_;
  int prevPumpSpeed_ = 0;
  int prevThermalSensorCount_ = 0;
  int prevThermalSensorFailures_ = 0;
  double heatSoakBaseline_ = 0.0;
  double prevVoltage12V_ = 0.0;
  double prevVoltage5V_ = 0.0;
  double prevVoltage33V_ = 0.0;
  double prevVoltageVcore_ = 0.0;
  int prevTpmPresent_ = 0;
  int prevTpmReady_ = 0;
  int prevCmosBatteryOk_ = 1;
  int prevSensorPollFailures_ = 0;
  int prevFsSystem32FileCount_ = 0;
  int prevFsSystem32HiddenCount_ = 0;
  int prevFsSystem32SystemCount_ = 0;
  int prevFsDriversFileCount_ = 0;
  int prevFsDriversHiddenCount_ = 0;
  int prevFsRecycleBinContentCount_ = 0;
  int prevFsProgramFilesCount_ = 0;
  int prevFsVolumeDirtyBit_ = 0;
  std::wstring prevFsVolumeLabel_;
  int prevFsReparsePointCount_ = 0;
  int prevFsSparseFileCount_ = 0;
  int prevFsAdsWithDataCount_ = 0;
  int prevFsCorruptionWarnings_ = 0;
  int prevFsChkdskPending_ = 0;
  int prevRegKeyCountRun_ = 0;
  int prevRegValueCountRun_ = 0;
  unsigned long prevRegHashRun_ = 0;
  int prevRegKeyCountRunOnce_ = 0;
  int prevRegValueCountRunOnce_ = 0;
  unsigned long prevRegHashRunOnce_ = 0;
  int prevRegKeyCountShell_ = 0;
  int prevRegValueCountShell_ = 0;
  unsigned long prevRegHashShell_ = 0;
  int prevRegKeyCountPolicies_ = 0;
  int prevRegValueCountPolicies_ = 0;
  unsigned long prevRegHashPolicies_ = 0;
  int prevRegKeyCountServices_ = 0;
  int prevRegValueCountServices_ = 0;
  unsigned long prevRegHashServices_ = 0;
  int prevRegKeyCountDrivers_ = 0;
  int prevRegValueCountDrivers_ = 0;
  unsigned long prevRegHashDrivers_ = 0;
  int prevRegKeyCountFirewall_ = 0;
  int prevRegValueCountFirewall_ = 0;
  unsigned long prevRegHashFirewall_ = 0;
  int prevRegKeyCountUac_ = 0;
  int prevRegValueCountUac_ = 0;
  unsigned long prevRegHashUac_ = 0;
  int prevRegKeyCountTaskSched_ = 0;
  int prevRegValueCountTaskSched_ = 0;
  unsigned long prevRegHashTaskSched_ = 0;
  int prevRegKeyCountCom_ = 0;
  int prevRegValueCountCom_ = 0;
  unsigned long prevRegHashCom_ = 0;
  int prevRegKeyCountTelemetry_ = 0;
  int prevRegValueCountTelemetry_ = 0;
  unsigned long prevRegHashTelemetry_ = 0;
  int prevRegKeyCountAudit_ = 0;
  int prevRegValueCountAudit_ = 0;
  unsigned long prevRegHashAudit_ = 0;
  int prevRegKeyCountEnv_ = 0;
  int prevRegValueCountEnv_ = 0;
  unsigned long prevRegHashEnv_ = 0;
  int prevRegKeyCountPath_ = 0;
  int prevRegValueCountPath_ = 0;
  unsigned long prevRegHashPath_ = 0;
  int prevRegKeyCountAppAssoc_ = 0;
  int prevRegValueCountAppAssoc_ = 0;
  unsigned long prevRegHashAppAssoc_ = 0;
  int prevRegKeyCountShellExt_ = 0;
  int prevRegValueCountShellExt_ = 0;
  unsigned long prevRegHashShellExt_ = 0;
  int prevRegKeyCountDefApp_ = 0;
  int prevRegValueCountDefApp_ = 0;
  unsigned long prevRegHashDefApp_ = 0;
  int prevRegKeyCountFileAssoc_ = 0;
  int prevRegValueCountFileAssoc_ = 0;
  unsigned long prevRegHashFileAssoc_ = 0;
  int prevRegKeyCountConfigFile_ = 0;
  int prevRegValueCountConfigFile_ = 0;
  unsigned long prevRegHashConfigFile_ = 0;
  int prevStartupFolderCount_ = 0;
  int prevEnvVarCount_ = 0;
  unsigned long prevRegHashEnvPath_ = 0;
  int prevAudioOutputDeviceCount_ = 0;
  int prevAudioInputDeviceCount_ = 0;
  int prevAudioMixerCount_ = 0;
  DWORD prevAudioMasterVolume_ = 0;
  int prevAudioMasterMuted_ = 0;
  DWORD prevAudioMasterVolumeLeft_ = 0;
  DWORD prevAudioMasterVolumeRight_ = 0;
  int prevAudioSampleRate_ = 0;
  int prevAudioBitsPerSample_ = 0;
  int prevAudioChannels_ = 0;
  int prevAudioWaveOutOpen_ = 0;
  int prevAudioWaveInOpen_ = 0;
  int prevAudioServiceRunning_ = 1;
  int prevAudioMicMuted_ = 0;
  int prevAudioSystemMuted_ = 0;
  int prevAudioHeadphoneJack_ = -1;
  int prevAudioSpatialization_ = 0;
  int prevAudioPlaybackActive_ = 0;
  int prevAudioRecordingActive_ = 0;
  unsigned long prevAudioDeviceHash_ = 0;
  unsigned long prevAudioFormatHash_ = 0;
  int prevAudioLatencyMs_ = 0;
  int prevAudioCodecEvents_ = 0;
  int prevAudioDecoderErrors_ = 0;
  int prevSehExceptionCount_ = 0;
  int prevUnhandledExceptionCount_ = 0;
  int prevAccessViolationCount_ = 0;
  int prevHeapCorruptionDetected_ = 0;
  int prevAssertionFailureCount_ = 0;
  int prevStackOverflowCount_ = 0;
  int prevDeadlockRecoveryCount_ = 0;
  int prevProcessRestartCount_ = 0;
  int prevModuleReloadCount_ = 0;
  int prevUiFreezeDetected_ = 0;
  int prevHangDetected_ = 0;
  int prevTimeoutExceeded_ = 0;
  int prevRetryStormDetected_ = 0;
  int prevBackoffEscalation_ = 0;
  int prevCircuitBreakerOpen_ = 0;
  int prevCircuitBreakerClose_ = 0;
  int prevFallbackModeEntry_ = 0;
  int prevFallbackModeExit_ = 0;
  int prevConfigRollbackDetected_ = 0;
  int prevSafeModeActive_ = -1;
  int prevTelemetryDropDetected_ = 0;
  int prevWatchdogResetDetected_ = 0;
  int prevServiceRecoveryAction_ = 0;
  int prevCrashEventsToday_ = 0;
  int prevExceptionLogCount_ = 0;
  unsigned long prevProcessHash_ = 0;
  unsigned long prevModuleHash_ = 0;
  int prevMainThreadResponsive_ = 1;
  int prevUiResponsivenessMs_ = 0;
  int prevThreadHealthOk_ = 1;
  mutable std::mutex stateMutex_;
  AppState state_;
};

MonixApp::MonixApp() : paths_(ResolveAppPaths()) {
  QueryPerformanceFrequency(&qpcFrequency_);
  state_.sessionId = GenerateSessionId();
  WriteDefaultConfigIfMissing();
  LoadConfig(false);
  LoadRuntimeAssets();
  std::filesystem::create_directories(paths_.logsDir);
  std::filesystem::create_directories(paths_.exportsDir);
  LoadRecentLogHistory();
  auth_.Init(paths_.configFile);
  kernel_.Initialize(&auth_);

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
      shaderBrowserPanel_ = std::make_unique<monix::renderer_vk::ShaderBrowserPanel>(*shaderLibrary_, *shaderCompiler_, *shaderRuntime_, state_.shaderBrowser);
    }
  }
}

MonixApp::~MonixApp() {
  StopTelemetry();
  FlushLogQueues(true);
  ShutdownOpenGlBootstrap();
  DestroyUiFonts();
  win98Fonts_.Destroy();
  win98Assets_.Clear();
  largeIcon_.reset();
  smallIcon_.reset();
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
  if (std::filesystem::exists(paths_.configFile)) {
    return;
  }

  std::ofstream output(paths_.configFile);
  output << "; Monix runtime configuration\n";
  output << "; Settings can be edited live from the Settings tab or reloaded with F5.\n";
  output << "delay_ms=75\n";
  output << "frame_interval_ms=33\n";
  output << "font_scale=1.12\n";
  output << "log_buffer_size=480\n";
  output << "log_visible_lines=26\n";
  output << "log_level=debug\n";
  output << "log_view=structured\n";
  output << "log_milliseconds=true\n";
  output << "log_json_enabled=true\n";
  output << "log_plain_enabled=true\n";
  output << "log_flush_interval_ms=1200\n";
  output << "log_max_file_bytes=1048576\n";
  output << "log_retention_days=7\n";
  output << "log_deduplicate=true\n";
  output << "pause_live_logs=false\n";
  output << "notifications_enabled=true\n";
  output << "sound_enabled=true\n";
  output << "notification_duration_ms=4200\n";
  output << "notification_max_stack=4\n";
  output << "analytics_history_enabled=true\n";
  output << "history_capacity=120\n";
  output << "user_id=local\n";
  output << "intro_enabled=true\n";
  output << "intro_step_px=18\n";
  output << "intro_hold_ms=1050\n";
  output << "intro_credit_delay_ms=260\n";
  output << "crt_enabled=false\n";
  output << "crt_scanline_spacing=3\n";
  output << "crt_ghost_offset=1\n";
  output << "crt_curvature_strength=0.0\n";
  output << "crt_scanline_intensity=0.0\n";
  output << "crt_chromatic_aberration=0.0\n";
  output << "crt_phosphor_glow=0.0\n";
  output << "crt_flicker_amount=0.0\n";
  output << "crt_noise_amount=0.0\n";
  output << "crt_vignette_strength=0.0\n";
  output << "crt_sharpness=0.0\n";
  output << "crt_rgb_shift=0.0\n";
  output << "crt_grain=0.0\n";
  output << "crt_jitter=0.0\n";
  output << "crt_subpixel_mode=0.0\n";
  output << "crt_bloom=0.0\n";
  output << "crt_burn_in_enabled=false\n";
  output << "crt_burn_in_intensity=0.0\n";
  output << "crt_burn_in_decay_rate=1.0\n";
}

void MonixApp::LoadRuntimeAssets() {
  if (IsCoreMonitorThemeActive() && !paths_.fontList.empty()) {
    for (size_t i = 0; i < paths_.fontList.size(); ++i) {
      const auto& candidate = paths_.fontList[i];
      const std::wstring marker = ToUpper(
        candidate.displayName + L" " + candidate.faceName + L" " + candidate.filePath
      );
      if (
        marker.find(L"OLDSCHOOL") != std::wstring::npos ||
        marker.find(L"VGA") != std::wstring::npos ||
        marker.find(L"437") != std::wstring::npos ||
        marker.find(L"DOS") != std::wstring::npos
      ) {
        state_.currentFontIndex = static_cast<int>(i);
        break;
      }
    }
  }

  if (!paths_.fontList.empty() && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
    const auto& font = paths_.fontList[state_.currentFontIndex];
    if (!font.filePath.empty() && std::filesystem::exists(font.filePath)) {
      if (AddFontResourceExW(font.filePath.c_str(), FR_PRIVATE, nullptr) > 0) {
        privateFontLoaded_ = true;
        fontFace_ = font.faceName;
        return;
      }
    }
  }
  if (std::filesystem::exists(paths_.fontFile)) {
    if (AddFontResourceExW(paths_.fontFile.c_str(), FR_PRIVATE, nullptr) > 0) {
      privateFontLoaded_ = true;
      fontFace_ = L"VHS Gothic";
    }
  }
}

void MonixApp::SwitchFont(int direction) {
  if (paths_.fontList.size() <= 1) return;
  if (privateFontLoaded_) {
    const auto& old = paths_.fontList[state_.currentFontIndex];
    if (!old.filePath.empty()) {
      RemoveFontResourceExW(old.filePath.c_str(), FR_PRIVATE, nullptr);
    }
    privateFontLoaded_ = false;
  }
  state_.currentFontIndex = (state_.currentFontIndex + direction + static_cast<int>(paths_.fontList.size())) % static_cast<int>(paths_.fontList.size());
  LoadRuntimeAssets();
  CreateUiFonts();
}

void MonixApp::LoadRecentLogHistory() {
  if (!std::filesystem::exists(paths_.logsDir)) {
    return;
  }

  std::vector<std::filesystem::directory_entry> logFiles;
  for (const auto& entry : std::filesystem::directory_iterator(paths_.logsDir)) {
    if (entry.is_regular_file() && entry.path().extension() == L".log") {
      logFiles.push_back(entry);
    }
  }
  if (logFiles.empty()) {
    return;
  }

  std::sort(logFiles.begin(), logFiles.end(), [](const auto& left, const auto& right) {
    return left.last_write_time() > right.last_write_time();
  });

  const int limit = std::max(48, config_.logBufferSize - 24);
  std::ifstream input(logFiles.front().path(), std::ios::binary);
  if (!input) {
    return;
  }

  std::vector<std::string> lines;
  lines.reserve(limit);
  std::string line;
  while (std::getline(input, line)) {
    line = TrimAscii(line);
    if (line.empty()) {
      continue;
    }
    if (static_cast<int>(lines.size()) == limit) {
      lines.erase(lines.begin());
    }
    lines.push_back(line);
  }

  const auto resolveColor = [](const std::wstring& domain, LogLevel level) {
    switch (level) {
      case LogLevel::Critical: return ColorRole::Fatal;
      case LogLevel::Error: return ColorRole::Error;
      case LogLevel::Warn: return ColorRole::Warning;
      default: break;
    }

    const std::wstring upper = ToUpper(domain);
    if (upper == L"NETWORK") return ColorRole::Network;
    if (upper == L"SCRAM") return ColorRole::Scram;
    if (upper == L"KERNEL") return ColorRole::Kernel;
    if (upper == L"CPU" || upper == L"GPU" || upper == L"RAM" || upper == L"DISK") return ColorRole::Warning;
    if (upper == L"SYSTEM" || upper == L"CONFIG") return ColorRole::Success;
    if (upper == L"OPENGL" || upper == L"LOG") return ColorRole::UserInput;
    return ColorRole::Primary;
  };

  auto extractBracketValue = [](const std::wstring& source, std::size_t& cursor) {
    while (cursor < source.size() && source[cursor] == L' ') {
      ++cursor;
    }
    if (cursor >= source.size() || source[cursor] != L'[') {
      return std::wstring();
    }
    const std::size_t end = source.find(L']', cursor);
    if (end == std::wstring::npos) {
      return std::wstring();
    }
    const std::wstring value = source.substr(cursor + 1, end - cursor - 1);
    cursor = end + 1;
    return value;
  };

  std::uint64_t highestEventId = 0;
  for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
    const std::wstring raw = Utf8ToWide(*it);
    const std::size_t firstSpace = raw.find(L' ');
    if (firstSpace == std::wstring::npos) {
      continue;
    }

    LogEntry entry;
    entry.fullTimestamp = raw.substr(0, firstSpace);
    if (entry.fullTimestamp.size() >= 23) {
      entry.time = entry.fullTimestamp.substr(11, 12);
    } else if (entry.fullTimestamp.size() >= 19) {
      entry.time = entry.fullTimestamp.substr(11);
    } else {
      entry.time = entry.fullTimestamp;
    }

    std::size_t cursor = firstSpace + 1;
    entry.severity = extractBracketValue(raw, cursor);
    entry.domain = extractBracketValue(raw, cursor);
    const std::wstring modulePart = extractBracketValue(raw, cursor);
    const std::wstring servicePart = extractBracketValue(raw, cursor);
    const std::wstring sessionPart = extractBracketValue(raw, cursor);
    const std::wstring eventPart = extractBracketValue(raw, cursor);
    const std::wstring pidPart = extractBracketValue(raw, cursor);
    const std::wstring tidPart = extractBracketValue(raw, cursor);
    entry.module = modulePart.rfind(L"module=", 0) == 0 ? modulePart.substr(7) : L"history";
    entry.service = servicePart.rfind(L"service=", 0) == 0 ? servicePart.substr(8) : L"history";
    entry.sessionId = sessionPart.rfind(L"session=", 0) == 0 ? sessionPart.substr(8) : L"history";

    if (eventPart.rfind(L"event=", 0) == 0) {
      entry.eventId = static_cast<std::uint64_t>(_wtoi64(eventPart.substr(6).c_str()));
    }
    if (pidPart.rfind(L"pid=", 0) == 0) {
      entry.processId = static_cast<DWORD>(_wtoi(pidPart.substr(4).c_str()));
    }
    if (tidPart.rfind(L"tid=", 0) == 0) {
      entry.threadId = static_cast<DWORD>(_wtoi(tidPart.substr(4).c_str()));
    }

    while (cursor < raw.size() && raw[cursor] == L' ') {
      ++cursor;
    }
    std::wstring payload = cursor < raw.size() ? raw.substr(cursor) : L"";
    const std::size_t metadataStart = payload.rfind(L" { ");
    if (metadataStart != std::wstring::npos && !payload.empty() && payload.back() == L'}') {
      entry.message = payload.substr(0, metadataStart);
      entry.metadata = payload.substr(metadataStart + 3, payload.size() - metadataStart - 4);
    } else {
      entry.message = payload;
    }

    entry.level = ParseLogLevel(entry.severity);
    entry.severity = LogLevelText(entry.level);
    entry.color = resolveColor(entry.domain, entry.level);
    highestEventId = std::max(highestEventId, entry.eventId);
    state_.logs.push_back(std::move(entry));
  }

  state_.nextEventId = std::max(state_.nextEventId, highestEventId + 1);
}

void MonixApp::LoadConfig(bool logEvent) {
  Config next = config_;
  std::ifstream input(paths_.configFile);

  if (input) {
    std::string line;
    while (std::getline(input, line)) {
      line = TrimAscii(line);
      if (line.empty() || line[0] == ';' || line[0] == '#') {
        continue;
      }
      const auto separator = line.find('=');
      if (separator == std::string::npos) {
        continue;
      }

      const std::string key = ToLowerAscii(TrimAscii(line.substr(0, separator)));
      const std::string value = TrimAscii(line.substr(separator + 1));

      if (key == "border_image") {
        next.borderImage = Utf8ToWide(value);
      } else if (key == "log_buffer_size") {
        int parsed = 0;
        if (TryParseInt(value, parsed)) {
          next.logBufferSize = std::clamp(parsed, 64, 800);
        }
      } else if (key == "log_max_file_bytes") {
        UINT parsed = 0;
        if (TryParseUInt(value, parsed)) {
          next.logMaxFileBytes = std::clamp<std::uint64_t>(parsed, 65536ull, 32ull * 1024ull * 1024ull);
        }
      } else if (key == "user_id") {
        next.userId = Utf8ToWide(value);
      } else {
        monix::RegistryLoadSetting(key, value, next);
      }
    }
  }

  std::lock_guard<std::mutex> lock(stateMutex_);
  config_ = next;
  state_.intro.active = false;

  TrimHistoryBuffers();
  if (!config_.notificationsEnabled) {
    state_.notifications.clear();
  } else if (static_cast<int>(state_.notifications.size()) > config_.notificationMaxStack) {
    state_.notifications.resize(config_.notificationMaxStack);
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
  std::ofstream output(paths_.configFile, std::ios::trunc);
  if (!output) {
    return;
  }

  output << "; Monix runtime configuration\n";
  output << "; Settings can be edited live from the Settings tab or reloaded with F5.\n\n";

  const auto& registry = monix::GetSettingsRegistry();
  for (const auto& def : registry) {
    monix::RegistrySaveSetting(def.id, config_, output);
  }

  output << "\nborder_image=" << WideToUtf8(config_.borderImage) << "\n";
  output << "log_buffer_size=" << config_.logBufferSize << "\n";
  output << "log_max_file_bytes=" << config_.logMaxFileBytes << "\n";
  output << "user_id=" << WideToUtf8(config_.userId) << "\n";
}

void MonixApp::TrimHistoryBuffers() {
  const auto trim = [this](std::vector<double>& values) {
    if (config_.historyCapacity > 0 && static_cast<int>(values.size()) > config_.historyCapacity) {
      values.erase(values.begin(), values.begin() + (values.size() - config_.historyCapacity));
    }
  };

  trim(state_.cpuHistory);
  trim(state_.ramHistory);
  trim(state_.gpuHistory);
  trim(state_.netUploadHistory);
  trim(state_.netHistory);
  trim(state_.latencyHistory);
  trim(state_.logWarnHistory);
  trim(state_.logErrHistory);
  trim(state_.logCritHistory);
  trim(state_.logKernelHistory);
  trim(state_.logNetHistory);
}

std::wstring MonixApp::SettingLabel(SettingId id) const {
  return monix::RegistrySettingLabel(id);
}

std::wstring MonixApp::SettingValueText(SettingId id) const {
  std::wstring registryText = monix::RegistrySettingValueText(id, config_);
  if (!registryText.empty()) return registryText;

  const auto yesNo = [](bool value) {
    return std::wstring(value ? L"YES" : L"NO");
  };

  switch (id) {
    case SettingId::LogLevel: return LogLevelText(config_.logLevel);
    case SettingId::LogViewMode: return LogViewModeText(config_.logViewMode);
    case SettingId::FontFaceIndex: {
      if (!paths_.fontList.empty()) {
        const auto& font = paths_.fontList[state_.currentFontIndex % paths_.fontList.size()];
        return font.displayName;
      }
      return L"DEFAULT";
    }
    case SettingId::FrameTargetFps: {
      if (config_.frameTargetFps == 0) return L"UNLIMITED";
      return std::to_wstring(config_.frameTargetFps) + L" FPS";
    }
    case SettingId::WindowOpacity: {
      wchar_t buf[16]; swprintf(buf, 16, L"%d%%", static_cast<int>(config_.windowOpacity * 100));
      return buf;
    }
    default: return L"-";
  }
}

SettingMutation MonixApp::AdjustSetting(SettingId id, int direction) {
  SettingMutation mutation;
  mutation.id = id;
  mutation.label = SettingLabel(id);
  const int step = direction < 0 ? -1 : 1;

  const auto changeBool = [&](bool& value) {
    value = !value;
    mutation.changed = true;
  };
  const auto changeUInt = [&](UINT& value, UINT delta, UINT minimum, UINT maximum) {
    const UINT candidate = step < 0 ?
      (value > minimum ? value - std::min(delta, value - minimum) : value) :
      (value < maximum ? std::min(maximum, value + delta) : value);
    if (candidate != value) {
      value = candidate;
      mutation.changed = true;
    }
  };
  const auto changeInt = [&](int& value, int delta, int minimum, int maximum) {
    const int candidate = std::clamp(value + (step * delta), minimum, maximum);
    if (candidate != value) {
      value = candidate;
      mutation.changed = true;
    }
  };
  const auto changeDouble = [&](double& value, double delta, double minimum, double maximum) {
    const double candidate = std::clamp(value + (step * delta), minimum, maximum);
    if (std::fabs(candidate - value) > 0.0005) {
      value = candidate;
      mutation.changed = true;
    }
  };

  switch (id) {
    case SettingId::TelemetryIntervalMs:
      changeUInt(config_.telemetryIntervalMs, 25u, 50u, 5000u);
      mutation.requestRefresh = mutation.changed;
      break;
    case SettingId::FrameIntervalMs:
      changeUInt(config_.frameIntervalMs, 1u, 16u, 100u);
      mutation.resetFrameTimer = mutation.changed;
      break;
    case SettingId::FontScale:
      changeDouble(config_.fontScale, 0.05, 0.85, 1.8);
      mutation.recreateFonts = mutation.changed;
      break;
    case SettingId::IntroEnabled:
      changeBool(config_.introEnabled);
      break;
    case SettingId::NotificationsEnabled:
      changeBool(config_.notificationsEnabled);
      mutation.clearNotifications = mutation.changed && !config_.notificationsEnabled;
      break;
    case SettingId::SoundEnabled:
      changeBool(config_.soundEnabled);
      break;
    case SettingId::NotificationDurationMs:
      changeUInt(config_.notificationDurationMs, 200u, 1000u, 12000u);
      break;
    case SettingId::NotificationMaxStack:
      changeInt(config_.notificationMaxStack, 1, 1, 8);
      break;
    case SettingId::AnalyticsHistoryEnabled:
      changeBool(config_.analyticsHistoryEnabled);
      break;
    case SettingId::HistoryCapacity:
      changeInt(config_.historyCapacity, 12, 24, 720);
      mutation.trimHistory = mutation.changed;
      break;
    case SettingId::LogVisibleLines:
      changeInt(config_.logVisibleLines, 1, 8, 48);
      break;
    case SettingId::LogLevel: {
      int index = LogLevelRank(config_.logLevel);
      index = std::clamp(index + step, 0, 4);
      const LogLevel next = static_cast<LogLevel>(index);
      if (next != config_.logLevel) {
        config_.logLevel = next;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::LogViewMode: {
      int index = static_cast<int>(config_.logViewMode);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      const LogViewMode next = static_cast<LogViewMode>(index);
      if (next != config_.logViewMode) {
        config_.logViewMode = next;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::LogMilliseconds:
      changeBool(config_.logMilliseconds);
      break;
    case SettingId::LogJsonEnabled:
      changeBool(config_.logJsonEnabled);
      break;
    case SettingId::LogPlainEnabled:
      changeBool(config_.logPlainEnabled);
      break;
    case SettingId::LogFlushIntervalMs:
      changeUInt(config_.logFlushIntervalMs, 100u, 250u, 10000u);
      break;
    case SettingId::LogRetentionDays:
      changeUInt(config_.logRetentionDays, 1u, 1u, 90u);
      break;
    case SettingId::LogDeduplicate:
      changeBool(config_.logDeduplicate);
      break;
    case SettingId::PauseLiveLogs:
      changeBool(config_.pauseLiveLogs);
      break;
    case SettingId::CrtEnabled:
      changeBool(config_.crtEnabled);
      break;
    case SettingId::CrtCurvatureStrength:
      changeDouble(config_.crtCurvatureStrength, 0.02, 0.0, 0.6);
      break;
    case SettingId::CrtScanlineIntensity:
      changeDouble(config_.crtScanlineIntensity, 0.02, 0.0, 0.6);
      break;
    case SettingId::CrtScanlineSpacing:
      changeInt(config_.crtScanlineSpacing, 1, 2, 8);
      break;
    case SettingId::CrtChromaticAberration:
      changeDouble(config_.crtChromaticAberration, 0.005, 0.0, 0.16);
      break;
    case SettingId::CrtPhosphorGlow:
      changeDouble(config_.crtPhosphorGlow, 0.01, 0.0, 0.5);
      break;
    case SettingId::CrtFlickerAmount:
      changeDouble(config_.crtFlickerAmount, 0.002, 0.0, 0.08);
      break;
    case SettingId::CrtNoiseAmount:
      changeDouble(config_.crtNoiseAmount, 0.002, 0.0, 0.08);
      break;
    case SettingId::CrtVignetteStrength:
      changeDouble(config_.crtVignetteStrength, 0.02, 0.0, 0.6);
      break;
    case SettingId::CrtSharpness:
      changeDouble(config_.crtSharpness, 0.02, 0.0, 1.0);
      break;
    case SettingId::CrtGhostOffset:
      changeInt(config_.crtGhostOffset, 1, 0, 2);
      break;
    case SettingId::IntroStepPx:
      changeInt(config_.introStepPx, 2, 4, 64);
      break;
    case SettingId::IntroHoldMs:
      changeUInt(config_.introHoldMs, 100u, 200u, 5000u);
      break;
    case SettingId::IntroCreditDelayMs:
      changeUInt(config_.introCreditDelayMs, 50u, 0u, 2000u);
      break;
    case SettingId::BorderEnabled:
      changeBool(config_.borderEnabled);
      break;
    case SettingId::BorderChromaKeyR:
      changeDouble(config_.borderChromaKeyR, 0.05, 0.0, 1.0);
      break;
    case SettingId::BorderChromaKeyG:
      changeDouble(config_.borderChromaKeyG, 0.05, 0.0, 1.0);
      break;
    case SettingId::BorderChromaKeyB:
      changeDouble(config_.borderChromaKeyB, 0.05, 0.0, 1.0);
      break;
    case SettingId::BorderChromaKeyTolerance:
      changeDouble(config_.borderChromaKeyTolerance, 0.05, 0.0, 1.0);
      break;
    case SettingId::CrtRgbShift:
      changeDouble(config_.crtRgbShift, 0.005, 0.0, 0.1);
      break;
    case SettingId::CrtGrain:
      changeDouble(config_.crtGrain, 0.005, 0.0, 0.1);
      break;
    case SettingId::CrtJitter:
      changeDouble(config_.crtJitter, 0.01, 0.0, 1.0);
      break;
    case SettingId::CrtSubpixelMode:
      changeDouble(config_.crtSubpixelMode, 0.1, 0.0, 1.0);
      break;
    case SettingId::CrtBloom:
      changeDouble(config_.crtBloom, 0.02, 0.0, 0.5);
      break;
    case SettingId::CrtBurnInEnabled:
      changeBool(config_.crtBurnInEnabled);
      break;
    case SettingId::CrtBurnInIntensity:
      changeDouble(config_.crtBurnInIntensity, 0.02, 0.0, 1.0);
      break;
    case SettingId::CrtBurnInDecayRate:
      changeDouble(config_.crtBurnInDecayRate, 0.1, 0.01, 10.0);
      break;
    case SettingId::AccentColorPreset: {
      int index = std::clamp(config_.accentColorPreset, 0, 5);
      index += step;
      if (index < 0) index = 5;
      if (index > 5) index = 0;
      if (index != config_.accentColorPreset) {
        config_.accentColorPreset = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::FontFaceIndex: {
      SwitchFont(step);
      const auto& font = paths_.fontList[state_.currentFontIndex];
      PushLog(L"ENGINE", L"INFO", L"Font switched to " + font.displayName + L" (" + font.faceName + L").", ColorRole::UserInput, L"engine", L"font", L"name=" + font.displayName);
      mutation.changed = true;
      mutation.recreateFonts = true;
      break;
    }
    case SettingId::DefaultTab: {
      int index = std::clamp(config_.defaultTab, 0, 5);
      index += step;
      if (index < 0) index = 5;
      if (index > 5) index = 0;
      if (index != config_.defaultTab) {
        config_.defaultTab = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::MinimizeToTray:
      changeBool(config_.minimizeToTray);
      break;
    case SettingId::StartMaximized:
      changeBool(config_.startMaximized);
      break;
    case SettingId::SaveWindowPosition:
      changeBool(config_.saveWindowPosition);
      break;
    case SettingId::HotkeyToggleCrt: {
      int vk = config_.hotkeyToggleCrt;
      vk += (step > 0) ? 1 : -1;
      if (vk < 0) vk = 0;
      if (vk > 0xFF) vk = 0;
      if (vk != config_.hotkeyToggleCrt) {
        config_.hotkeyToggleCrt = vk;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::HotkeyReloadConfig: {
      int vk = config_.hotkeyReloadConfig;
      vk += (step > 0) ? 1 : -1;
      if (vk < 0) vk = 0;
      if (vk > 0xFF) vk = 0;
      if (vk != config_.hotkeyReloadConfig) {
        config_.hotkeyReloadConfig = vk;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::CrtBrightness:
      changeDouble(config_.crtBrightness, 0.05, 0.1, 2.0);
      break;
    case SettingId::CrtContrast:
      changeDouble(config_.crtContrast, 0.05, 0.1, 2.0);
      break;
    case SettingId::CrtSaturation:
      changeDouble(config_.crtSaturation, 0.05, 0.0, 3.0);
      break;
    case SettingId::CrtGamma:
      changeDouble(config_.crtGamma, 0.05, 0.1, 3.0);
      break;
    case SettingId::CrtScanlineMode: {
      int index = std::clamp(config_.crtScanlineMode, 0, 2);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      if (index != config_.crtScanlineMode) {
        config_.crtScanlineMode = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::CrtInterlace:
      config_.crtInterlace = (config_.crtInterlace != 0) ? 0 : 1;
      mutation.changed = true;
      break;
    case SettingId::BorderImagePreset: {
      int index = std::clamp(config_.borderImagePreset, 0, 4);
      index += step;
      if (index < 0) index = 4;
      if (index > 4) index = 0;
      if (index != config_.borderImagePreset) {
        config_.borderImagePreset = index;
        mutation.changed = true;
      }
      break;
    }
    case SettingId::Language: {
      int index = std::clamp(config_.language, 0, 2);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      if (index != config_.language) {
        config_.language = index;
        mutation.changed = true;
        mutation.recreateFonts = true;
      }
      break;
    }
    case SettingId::ShowFps:
      config_.showFps = !config_.showFps;
      mutation.changed = true;
      break;
    case SettingId::WindowOpacity:
      changeDouble(config_.windowOpacity, 0.05, 0.3, 1.0);
      break;
    case SettingId::AlwaysOnTop:
      config_.alwaysOnTop = !config_.alwaysOnTop;
      mutation.changed = true;
      break;
    case SettingId::StartWithWindows:
      config_.startWithWindows = !config_.startWithWindows;
      mutation.changed = true;
      break;
    case SettingId::ThemeMode: {
      int index = std::clamp(config_.themeMode, 0, monix::ui::kWin98ThemeMode);
      index += step;
      if (index < 0) index = monix::ui::kWin98ThemeMode;
      if (index > monix::ui::kWin98ThemeMode) index = 0;
      if (index != config_.themeMode) {
        config_.themeMode = index;
        mutation.changed = true;
        mutation.recreateFonts = true;
      }
      break;
    }
    case SettingId::FrameTargetFps: {
      int index = std::clamp(config_.frameTargetFps, 0, 120);
      if (index == 0) index = 30;
      else if (index == 30) index = 60;
      else if (index == 60) index = 120;
      else index = 0;
      config_.frameTargetFps = index;
      mutation.changed = true;
      break;
    }
    case SettingId::VSyncEnabled:
      config_.vSyncEnabled = !config_.vSyncEnabled;
      mutation.changed = true;
      break;
    case SettingId::ProcessPriority: {
      int index = std::clamp(config_.processPriority, 0, 2);
      index += step;
      if (index < 0) index = 2;
      if (index > 2) index = 0;
      if (index != config_.processPriority) {
        config_.processPriority = index;
        mutation.changed = true;
      }
      break;
    }
  }

  return mutation;
}

void MonixApp::CommitSettingMutation(const SettingMutation& mutation) {
  if (!mutation.changed) {
    return;
  }

  if (mutation.clearNotifications) {
    state_.notifications.clear();
  }
  if (mutation.trimHistory) {
    TrimHistoryBuffers();
  }
  if (config_.notificationMaxStack > 0 && static_cast<int>(state_.notifications.size()) > config_.notificationMaxStack) {
    state_.notifications.resize(config_.notificationMaxStack);
  }

  SaveConfig();
  if (mutation.recreateFonts) {
    CreateUiFonts();
  }
  if (mutation.resetFrameTimer) {
    SetFrameTimer();
  }
  if (mutation.requestRefresh) {
    RequestRefresh();
  }

  PushLog(
    L"CONFIG",
    L"INFO",
    mutation.label + L" updated to " + SettingValueText(mutation.id) + L".",
    ColorRole::UserInput,
    L"config",
    L"settings-panel",
    L"live=true"
  );
  SetToast(mutation.label + L" saved");
}

void MonixApp::DestroyUiFonts() {
  tabFont_.reset();
  bodyFont_.reset();
  smallFont_.reset();
  titleFont_.reset();
  logFont_.reset();
  logoFont_.reset();
}

void MonixApp::MeasureFontMetrics() {
  HDC screen = GetDC(nullptr);
  const auto measure = [&](HFONT font) {
    TEXTMETRICW metrics {};
    HGDIOBJ old = SelectObject(screen, font);
    GetTextMetricsW(screen, &metrics);
    SelectObject(screen, old);
    return metrics.tmHeight + metrics.tmExternalLeading;
  };

  if (bodyFont_) bodyLineHeight_ = measure(bodyFont_.get());
  if (smallFont_) smallLineHeight_ = measure(smallFont_.get());
  if (logFont_) logLineHeight_ = measure(logFont_.get());
  if (logoFont_) logoLineHeight_ = measure(logoFont_.get());
  ReleaseDC(nullptr, screen);
}

void MonixApp::CreateUiFonts() {
  if (IsCoreMonitorThemeActive()) {
    LoadRuntimeAssets();
  }
  DestroyUiFonts();

  const auto scale = [&](int base) {
    return std::max(8, static_cast<int>(base * config_.fontScale));
  };

  const LONG fontCharset = DEFAULT_CHARSET;
  const DWORD quality = IsCoreMonitorThemeActive() ? NONANTIALIASED_QUALITY : ANTIALIASED_QUALITY;
  tabFont_.reset(CreateFontW(-scale(30), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str()));
  bodyFont_.reset(CreateFontW(-scale(22), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str()));
  smallFont_.reset(CreateFontW(-scale(17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str()));
  titleFont_.reset(CreateFontW(-scale(34), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str()));
  logFont_.reset(CreateFontW(-scale(18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str()));
  logoFont_.reset(CreateFontW(-scale(20), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, OEM_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Terminal"));

  MeasureFontMetrics();
}

static std::atomic<const char*> g_phase{"INIT"};
static std::atomic<DWORD> g_telTid{0};

static std::string GetCrashLogPath() {
  char tempPath[MAX_PATH];
  if (GetTempPathA(MAX_PATH, tempPath)) {
    return std::string(tempPath) + "monix_crash.txt";
  }
  return "C:\\Windows\\Temp\\monix_crash.txt";
}

static LONG CALLBACK CrashVehHandler(EXCEPTION_POINTERS* ep) {
  if (ep && ep->ExceptionRecord &&
      (ep->ExceptionRecord->ExceptionCode == 0xC0000005 ||
       ep->ExceptionRecord->ExceptionCode == 0xC0000409)) {
    HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[512];
      auto addr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
      auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
      auto offset = addr - base;
      const char* phase = g_phase.load();
      DWORD crashTid = GetCurrentThreadId();
      DWORD telTid = g_telTid.load();
      const char* codeName = ep->ExceptionRecord->ExceptionCode == 0xC0000005 ? "ACCESS_VIOLATION" : "STACK_BUFFER_OVERRUN";

      HMODULE faultModule = nullptr;
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(addr), &faultModule);
      char modName[MAX_PATH] = {};
      if (faultModule) GetModuleFileNameA(faultModule, modName, MAX_PATH);

      int len = snprintf(buf, sizeof(buf), "CRASH code=%s(0x%lX) OFFSET=0x%llx Module=%s Phase=%s FaultAddr=0x%llx CrashTID=%lu TelTID=%lu\n",
        codeName, (unsigned long)ep->ExceptionRecord->ExceptionCode,
        (unsigned long long)offset, modName, phase ? phase : "?",
        (unsigned long long)(ep->ExceptionRecord->ExceptionCode == 0xC0000005
          ? ep->ExceptionRecord->ExceptionInformation[1] : addr),
        (unsigned long)crashTid, (unsigned long)telTid);
      DWORD written = 0;
      WriteFile(h, buf, len, &written, nullptr);
      FlushFileBuffers(h);
      CloseHandle(h);
    }
    return EXCEPTION_CONTINUE_SEARCH;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

int MonixApp::Run(HINSTANCE instance, int showCommand) {
  AddVectoredExceptionHandler(0, CrashVehHandler);
  instance_ = instance;
  if (!CreateMainWindow(instance, showCommand)) {
    return 1;
  }

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
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

static monix::renderer_vk::GlPresetConfig parseGlslpPreset(
    const std::filesystem::path& glslpPath,
    const std::filesystem::path& rootDir) {
  monix::renderer_vk::GlPresetConfig config;
  std::ifstream file(glslpPath);
  if (!file.is_open()) return config;

  auto basePath = glslpPath.parent_path();
  int shaderCount = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto key = line.substr(0, eq);
    auto val = line.substr(eq + 1);
    auto trim = [](std::string& s) {
      while (!s.empty() && s.front() == ' ') s.erase(s.begin());
      while (!s.empty() && s.back() == ' ') s.pop_back();
      if (!s.empty() && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
      }
    };
    trim(key);
    trim(val);

    if (key == "shaders") {
      shaderCount = std::stoi(val);
      config.passes.resize(shaderCount);
    } else if (key == "textures") {
      std::istringstream ss(val);
      std::string tok;
      while (std::getline(ss, tok, ';')) {
        trim(tok);
        if (!tok.empty()) {
          config.textures[tok] = {};
        }
      }
    } else if (key.find("shader") == 0 && key.size() > 6) {
      int idx = std::stoi(key.substr(6));
      if (idx >= 0 && idx < (int)config.passes.size()) {
        auto shaderPath = basePath / val;
        if (!std::filesystem::exists(shaderPath)) {
          shaderPath = rootDir / "Shaders" / val;
        }
        if (!std::filesystem::exists(shaderPath)) {
          auto parent2 = basePath.parent_path();
          shaderPath = parent2 / val;
        }
        config.passes[idx].shaderPath = shaderPath;
      }
    } else if (key.find("filter_linear") == 0 && key.size() > 13) {
      int idx = std::stoi(key.substr(13));
      if (idx >= 0 && idx < (int)config.passes.size()) {
        config.passes[idx].linearFilter = (val == "true");
      }
    } else if (key.find("float_framebuffer") == 0 && key.size() > 17) {
      int idx = std::stoi(key.substr(17));
      if (idx >= 0 && idx < (int)config.passes.size()) {
        config.passes[idx].floatFramebuffer = (val == "true");
      }
    }
  }

  for (auto& [name, tex] : config.textures) {
    auto texPath = basePath / tex.path;
    if (!std::filesystem::exists(texPath)) {
      texPath = rootDir / "Shaders" / tex.path;
    }
    tex.path = texPath;
  }

  return config;
}

// Standalone SEH wrapper — MSVC forbids __try/__except in functions with C++
// destructors, so we isolate the Vulkan init call here to catch access violations
// from broken/incompatible Vulkan drivers without killing the process.
static bool TryInitVulkanSafe(VulkanRenderer& vk, HWND hwnd, uint32_t w, uint32_t h) {
  BOOL crashed = FALSE;
  __try {
    if (!vk.initialize(hwnd, w, h)) {
      return false;
    }
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    crashed = TRUE;
  }
  if (crashed) {
    vk.shutdown();
    return false;
  }
  return true;
}

bool MonixApp::InitializeOpenGlBootstrap() {
  if (openGl_.available || openGl_.vkAvailable) {
    return true;
  }
  if (openGl_.vkFailed) {
    return false;
  }
  if (!hwnd_) {
    openGl_.status = L"Window handle is not ready";
    return false;
  }

  if (gdiplusToken_ == 0) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr);
  }

  // Initialize Vulkan renderer — wrap in standalone SEH function to catch
  // access violations from broken Vulkan drivers (MSVC disallows __try in
  // functions with C++ destructors).
  RECT rc;
  GetClientRect(hwnd_, &rc);
  uint32_t width = static_cast<uint32_t>(rc.right - rc.left);
  uint32_t height = static_cast<uint32_t>(rc.bottom - rc.top);

  if (!TryInitVulkanSafe(openGl_.vk, hwnd_, width, height)) {
    openGl_.status = L"Vulkan initialization failed — running in GDI-only mode";
    openGl_.vkFailed = true;
    return false;
  }

  openGl_.vkAvailable = true;
  openGl_.available = true;
  openGl_.hwnd = hwnd_;

  // We still need a window DC for GDI UI rendering (Render() draws to a GDI bitmap)
  if (!openGl_.dc) {
    openGl_.dc = GetDC(hwnd_);
  }

  // Store device info strings
  std::string vendor = openGl_.vk.getVendor();
  std::string rendererName = openGl_.vk.getRenderer();
  std::string versionStr = openGl_.vk.getVersion();
  openGl_.vendor = Utf8ToWide(vendor);
  openGl_.renderer = Utf8ToWide(rendererName);
  openGl_.version = Utf8ToWide(versionStr);
  openGl_.status = L"Vulkan renderer active";

  PushLog(L"VULKAN", L"INFO",
    L"Vulkan renderer: " + openGl_.renderer +
    L" (" + openGl_.vendor + L") " + openGl_.version,
    ColorRole::Primary, L"vulkan", L"init", L"action=init");

  // Initialize shader preset renderer and load the first preset
  if (!paths_.crtShaderFile.empty() && std::filesystem::exists(paths_.crtShaderFile)) {
    FILE* slog = nullptr;
    fopen_s(&slog, "preset_load.log", "w");
    auto slogf = [&](const char* msg) { if (slog) { fprintf(slog, "%s\n", msg); fflush(slog); } };

    slogf("Starting ShaderRenderer init...");
    char pbuf[512];
    sprintf_s(pbuf, "rootDir = %s", paths_.rootDir.string().c_str());
    slogf(pbuf);
    sprintf_s(pbuf, "slangcPath = %s", (paths_.rootDir / "tools" / "slangc.exe").string().c_str());
    slogf(pbuf);
    sprintf_s(pbuf, "crtShaderFile = %s", paths_.crtShaderFile.string().c_str());
    slogf(pbuf);

    bool isGlslp = (paths_.crtShaderFile.extension() == ".glslp");
    if (isGlslp) {
      slogf("Detected .glslp preset — using OpenGL backend");
      if (!openGl_.glBackend.initialize(hwnd_)) {
        slogf("ERROR: GL backend init failed");
        if (slog) fclose(slog);
        return false;
      }
      auto glConfig = parseGlslpPreset(paths_.crtShaderFile, paths_.rootDir);
      slogf("Parsed .glslp: loading into GL backend...");
      if (openGl_.glBackend.loadPreset(glConfig)) {
        openGl_.glPresetActive = true;
        openGl_.presetLoaded = true;
        slogf("SUCCESS: GL backend loaded .glslp preset");
        openGl_.status = L"OpenGL GLSL preset active";
        PushLog(L"SHADER", L"INFO", L"Loaded .glslp preset via OpenGL",
          ColorRole::Primary, L"shader", L"load", L"action=load_gl");
      } else {
        slogf("ERROR: GL backend loadPreset failed");
        openGl_.glPresetActive = false;
      }
      if (slog) fclose(slog);
      return true;
    }

    monix::renderer_vk::RendererConfig cfg;
    cfg.rootDirectory = paths_.rootDir;
    cfg.slangcPath = paths_.rootDir / "tools" / "slangc.exe";
    cfg.compileShadersToSpirv = true;
    cfg.enableDebugDumps = true;

    openGl_.shaderRenderer = std::make_unique<monix::renderer_vk::ShaderRenderer>(std::move(cfg));
    openGl_.shaderRenderer->setRenderer(&openGl_.vk);
    if (shaderCompiler_) {
      shaderCompiler_->setRenderer(openGl_.shaderRenderer.get());
    }
    if (shaderRuntime_ && openGl_.shaderRenderer->shaderCache()) {
      shaderRuntime_->setCache(openGl_.shaderRenderer->shaderCache());
    }
    slogf("ShaderRenderer created, calling loadPreset...");

    if (openGl_.vk.isInitialized()) {
      openGl_.shaderRenderer->resize(
        openGl_.vk.swapchainWidth(), openGl_.vk.swapchainHeight());
      slogf("Output extent set for ShaderRenderer");
    }

    auto loadStatus = openGl_.shaderRenderer->loadPreset(paths_.crtShaderFile);
    if (loadStatus) {
      const auto* compiled = openGl_.shaderRenderer->compiledPreset();
      if (compiled) {
        openGl_.presetLoaded = true;
        sprintf_s(pbuf, "SUCCESS: %d passes, %d images, %d parameters",
          (int)compiled->passes.size(), (int)compiled->graph.images.size(),
          (int)compiled->graph.samplers.size());
        slogf(pbuf);
        for (size_t i = 0; i < compiled->passes.size(); i++) {
          sprintf_s(pbuf, "  pass[%d]: %s (vertex=%zu bytes, fragment=%zu bytes, spirv_v=%zu, spirv_f=%zu)",
            (int)i, compiled->passes[i].preset.shaderPath.filename().string().c_str(),
            compiled->passes[i].vertex.source.size(),
            compiled->passes[i].fragment.source.size(),
            compiled->passes[i].vertex.spirv.size(),
            compiled->passes[i].fragment.spirv.size());
          slogf(pbuf);
        }

        wchar_t wbuf[256];
        swprintf_s(wbuf, L"Loaded preset: %d passes, %d images",
          (int)compiled->passes.size(), (int)compiled->graph.images.size());
        openGl_.status = wbuf;
        PushLog(L"SHADER", L"INFO", wbuf,
          ColorRole::Primary, L"shader", L"load", L"action=load");
      } else {
        slogf("loadStatus OK but compiledPreset() returned null");
      }
    } else {
      auto& err = loadStatus.message;
      slogf("loadPreset FAILED:");
      slogf(err.c_str());
      std::wstring werr(err.begin(), err.end());
      PushLog(L"SHADER", L"ERROR",
        L"Preset load failed: " + werr,
        ColorRole::Error, L"shader", L"load", L"action=load_fail");
    }
    if (slog) fclose(slog);
  }

  return true;
}

void MonixApp::DestroyOpenGlUiSurface() {
  if (openGl_.oldUiBitmap && openGl_.uiDc) {
    SelectObject(openGl_.uiDc, openGl_.oldUiBitmap);
    openGl_.oldUiBitmap = nullptr;
  }
  if (openGl_.uiBitmap) {
    DeleteObject(openGl_.uiBitmap);
    openGl_.uiBitmap = nullptr;
  }
  if (openGl_.uiDc) {
    DeleteDC(openGl_.uiDc);
    openGl_.uiDc = nullptr;
  }
  openGl_.uiPixels = nullptr;
  openGl_.uiWidth = 0;
  openGl_.uiHeight = 0;
}

bool MonixApp::EnsureOpenGlUiSurface(int width, int height) {
  if (width <= 0 || height <= 0 || !openGl_.dc) {
    return false;
  }
  if (openGl_.uiBitmap && openGl_.uiWidth == width && openGl_.uiHeight == height) {
    return true;
  }

  DestroyOpenGlUiSurface();

  openGl_.uiDc = CreateCompatibleDC(openGl_.dc);
  if (!openGl_.uiDc) {
    openGl_.status = L"Unable to allocate UI backbuffer DC";
    return false;
  }

  BITMAPINFO info {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  openGl_.uiBitmap = CreateDIBSection(openGl_.dc, &info, DIB_RGB_COLORS, &openGl_.uiPixels, nullptr, 0);
  if (!openGl_.uiBitmap || !openGl_.uiPixels) {
    openGl_.status = L"Unable to allocate UI bitmap surface";
    DestroyOpenGlUiSurface();
    return false;
  }

  openGl_.oldUiBitmap = SelectObject(openGl_.uiDc, openGl_.uiBitmap);
  openGl_.uiWidth = width;
  openGl_.uiHeight = height;
  return true;
}

bool MonixApp::RenderOpenGlFrame(const RECT& clientRect) {
  ComputeViewport(clientRect);
  const int width = std::max(1L, clientRect.right - clientRect.left);
  const int height = std::max(1L, clientRect.bottom - clientRect.top);

  if (!InitializeOpenGlBootstrap()) {
    return false;
  }

  if (!openGl_.vkAvailable) {
    openGl_.status = L"Vulkan renderer not available";
    return false;
  }

  // Render GDI UI to full-window bitmap; MapToViewport handles CRT x-compression for mouse
  if (!EnsureOpenGlUiSurface(width, height)) {
    return false;
  }
  RECT fullClient { 0, 0, width, height };
  Render(openGl_.uiDc, fullClient);
  GdiFlush();

  if (openGl_.glPresetActive && openGl_.glBackend.isValid() && openGl_.vk.isInitialized()) {
    // GL preset path: convert GDI BGRA -> GL RGBA, render via GL, convert back, upload to Vulkan
    auto* bgraPixels = static_cast<const uint8_t*>(openGl_.uiPixels);
    uint32_t w = static_cast<uint32_t>(openGl_.uiWidth);
    uint32_t h = static_cast<uint32_t>(openGl_.uiHeight);

    std::vector<uint8_t> rgbaPixels(w * h * 4);
    for (uint32_t i = 0; i < w * h; i++) {
      rgbaPixels[i * 4 + 0] = bgraPixels[i * 4 + 2]; // R <- B
      rgbaPixels[i * 4 + 1] = bgraPixels[i * 4 + 1]; // G
      rgbaPixels[i * 4 + 2] = bgraPixels[i * 4 + 0]; // B <- R
      rgbaPixels[i * 4 + 3] = bgraPixels[i * 4 + 3]; // A
    }

    auto readback = openGl_.glBackend.execute(rgbaPixels.data(), w, h);
    if (!readback.pixels.empty()) {
      std::vector<uint8_t> bgraResult(readback.pixels.size());
      for (size_t i = 0; i < readback.pixels.size(); i += 4) {
        bgraResult[i + 0] = readback.pixels[i + 2]; // B <- R
        bgraResult[i + 1] = readback.pixels[i + 1]; // G
        bgraResult[i + 2] = readback.pixels[i + 0]; // R <- B
        bgraResult[i + 3] = readback.pixels[i + 3]; // A
      }

      bool frameOk = openGl_.vk.beginFrame(readback.width, readback.height);
      if (frameOk) {
        openGl_.vk.uploadTextureToImage(bgraResult.data(), readback.width, readback.height, true);
        openGl_.vk.endFrame();
        openGl_.vk.present();
      }
    }
  } else if (openGl_.presetLoaded && openGl_.shaderRenderer && openGl_.vk.isInitialized()) {
    // Vulkan preset path
    monix::renderer_vk::FrameImage srcFrame{};
    srcFrame.pixels = openGl_.uiPixels;
    srcFrame.extent = {static_cast<uint32_t>(openGl_.uiWidth), static_cast<uint32_t>(openGl_.uiHeight)};
    srcFrame.bytes = openGl_.uiWidth * openGl_.uiHeight * 4;
    monix::renderer_vk::FrameContext fc{};
    auto status = openGl_.shaderRenderer->render(fc, srcFrame.pixels ? &srcFrame : nullptr);
    char buf[256];
    sprintf_s(buf, "[MONIX] SHADER: ShaderRenderer::render %s\n", status.ok ? "OK" : "FAILED");
    OutputDebugStringA(buf);
    if (!status.ok) {
      OutputDebugStringA(status.message.c_str());
      OutputDebugStringA("\n");
    }
  } else {
    // No preset loaded: fall back to GDI rendering
    return false;
  }

  ++openGl_.frameCount;
  openGl_.elapsedTime += 1.0f / 30.0f;
  return true;
}

void MonixApp::DestroyOpenGlResources() {
  DestroyOpenGlUiSurface();
}

void MonixApp::ShutdownOpenGlBootstrap() {
  DestroyOpenGlResources();
  openGl_.glBackend.shutdown();
  openGl_.glPresetActive = false;
  if (openGl_.vkAvailable) {
    openGl_.vk.shutdown();
    openGl_.vkAvailable = false;
  }
  if (openGl_.dc && hwnd_) {
    ReleaseDC(hwnd_, openGl_.dc);
    openGl_.dc = nullptr;
  }
  if (gdiplusToken_ != 0) {
    Gdiplus::GdiplusShutdown(gdiplusToken_);
    gdiplusToken_ = 0;
  }
  openGl_.hwnd = nullptr;
  openGl_.available = false;
  openGl_.functionsLoaded = false;
}

void MonixApp::SetFrameTimer() {
  if (!hwnd_) {
    return;
  }
  KillTimer(hwnd_, kFrameTimerId);
  SetTimer(hwnd_, kFrameTimerId, config_.frameIntervalMs, nullptr);
}

bool MonixApp::CreateMainWindow(HINSTANCE instance, int showCommand) {
  const wchar_t* className = L"MonixNativeWindow";
  WNDCLASSEXW wc {};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = StaticWndProc;
  wc.style = CS_OWNDC;
  wc.hInstance = instance;
  wc.lpszClassName = className;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

  largeIcon_.reset(reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kMainIconResourceId), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR)));
  smallIcon_.reset(reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kMainIconResourceId), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR)));
  if (std::filesystem::exists(paths_.iconFile)) {
    if (!largeIcon_) {
      largeIcon_.reset(reinterpret_cast<HICON>(LoadImageW(nullptr, paths_.iconFile.c_str(), IMAGE_ICON, 256, 256, LR_LOADFROMFILE)));
    }
    if (!smallIcon_) {
      smallIcon_.reset(reinterpret_cast<HICON>(LoadImageW(nullptr, paths_.iconFile.c_str(), IMAGE_ICON, 48, 48, LR_LOADFROMFILE)));
    }
  }
  wc.hIcon = largeIcon_.get();
  wc.hIconSm = smallIcon_.get();

  if (!RegisterClassExW(&wc)) {
    return false;
  }

  hwnd_ = CreateWindowExW(
    0,
    className,
    L"Monix",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    1680,
    980,
    nullptr,
    nullptr,
    instance,
    this
  );

  if (!hwnd_) {
    return false;
  }

  CreateUiFonts();
  // Vulkan init deferred to first WM_PAINT (via RenderOpenGlBootstrap -> RenderOpenGlFrame)
  // because the window has no valid client rect size at this point.
  ShowWindow(hwnd_, showCommand == SW_SHOWMINIMIZED ? SW_SHOWMINIMIZED : SW_MAXIMIZE);
  UpdateWindow(hwnd_);
  return true;
}

static unsigned __stdcall TelemetryThreadProc(void* param) {
  auto* app = static_cast<MonixApp*>(param);
  app->TelemetryLoop();
  return 0;
}

void MonixApp::StartTelemetry() {
  running_ = true;
  telemetryHandle_.reset(reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 8 * 1024 * 1024, TelemetryThreadProc, this, 0, nullptr)));
}

void MonixApp::StopTelemetry() {
  running_ = false;
  telemetryHandle_.reset();
}

void MonixApp::RequestRefresh() {
  refreshRequested_ = true;
}

void MonixApp::TelemetryLoop() {
  g_phase = "TEL:THREAD_START";
  g_telTid.store(GetCurrentThreadId());
  { HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
    GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      char buf[128]; int len = snprintf(buf, sizeof(buf), "TEL_TID=%lu\n", (unsigned long)GetCurrentThreadId());
      DWORD written = 0; WriteFile(h, buf, len, &written, nullptr); CloseHandle(h);
    }
  }
  while (running_) {
    std::unique_ptr<Snapshot> prevSnap;
    int sc = 0;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      sc = state_.sampleCount;
      if (state_.hasPreviousSnapshot && state_.previousSnapshot) {
        prevSnap = std::make_unique<Snapshot>(*state_.previousSnapshot);
      }
    }
    Snapshot snapshot = PollSnapshotWithState(std::move(prevSnap), sc);
    if (!snapshot.processes.empty() || snapshot.ramTotalBytes != 0) {
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        ConsumeSnapshot(std::move(snapshot));
      }

      if (hwnd_) {
        PostMessageW(hwnd_, WM_MONIX_UPDATE, 0, 0);
      }
    }

    UINT delay = 1000;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      delay = std::min<UINT>(config_.telemetryIntervalMs, 75u);
    }

    for (UINT elapsed = 0; elapsed < delay && running_; elapsed += 10) {
      if (refreshRequested_.exchange(false)) {
        break;
      }
      Sleep(10);
    }
  }
}

std::string MonixApp::RunProcessCapture(const std::wstring& commandLine) const {
  SECURITY_ATTRIBUTES sa {};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = nullptr;

  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
    return {};
  }

  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION pi {};
  std::wstring mutableCommand = commandLine;
  const BOOL created = CreateProcessW(
    nullptr,
    mutableCommand.data(),
    nullptr,
    nullptr,
    TRUE,
    CREATE_NO_WINDOW,
    nullptr,
    nullptr,
    &si,
    &pi
  );

  CloseHandle(writePipe);
  if (!created) {
    CloseHandle(readPipe);
    return {};
  }

  std::string output;
  std::array<char, 4096> buffer {};
  DWORD bytesRead = 0;
  while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
    output.append(buffer.data(), buffer.data() + bytesRead);
  }

  WaitForSingleObject(pi.hProcess, 15000);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  CloseHandle(readPipe);
  return output;
}


Snapshot MonixApp::PollSnapshot() {
  return PollSnapshotWithState(nullptr, 0);
}

static bool HeapOk(const char* tag) {
  BOOL ok = HeapValidate(GetProcessHeap(), 0, nullptr);
  if (!ok) {
    HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "HEAP CORRUPTED at: %s\n", tag);
      DWORD written = 0;
      WriteFile(h, buf, len, &written, nullptr);
      CloseHandle(h);
    }
  }
  return ok != FALSE;
}

Snapshot MonixApp::PollSnapshotWithState(std::unique_ptr<Snapshot> prevSnap, int sampleCount) {
  g_phase = "POLL:ENTER";
  HeapOk("ENTER");
  const int sc = sampleCount;
  auto snapshotPtr = std::make_unique<Snapshot>();
  Snapshot& snapshot = *snapshotPtr;
  if (prevSnap) {
    const auto& prev = *prevSnap;
    if (sc % 4 != 0) {
      snapshot.systemTime100ns = prev.systemTime100ns;
      snapshot.uptimeMs = prev.uptimeMs;
      snapshot.bootPhase = prev.bootPhase;
      snapshot.sessionCount = prev.sessionCount;
      snapshot.currentSessionId = prev.currentSessionId;
    }
    if (sc % 5 != 0) {
      snapshot.suspiciousScriptHosts = prev.suspiciousScriptHosts;
      snapshot.uacConsentProcesses = prev.uacConsentProcesses;
      snapshot.lsassAccessCount = prev.lsassAccessCount;
      snapshot.vmIndicators = prev.vmIndicators;
      snapshot.hookModulesDetected = prev.hookModulesDetected;
      snapshot.peHeaderTamper = prev.peHeaderTamper;
      snapshot.suspiciousModules = prev.suspiciousModules;
      snapshot.selfExePath = prev.selfExePath;
      snapshot.selfSignatureValid = prev.selfSignatureValid;
      snapshot.selfHashVerified = prev.selfHashVerified;
      snapshot.unsignedDriverCount = prev.unsignedDriverCount;
      snapshot.unsignedDriverNames = prev.unsignedDriverNames;
      snapshot.debugPortActive = prev.debugPortActive;
    }
    if (sc % 10 != 0) {
      snapshot.regKeyCountDrivers = prev.regKeyCountDrivers;
      snapshot.regValueCountDrivers = prev.regValueCountDrivers;
      snapshot.regHashDrivers = prev.regHashDrivers;
      snapshot.regKeyCountServices = prev.regKeyCountServices;
      snapshot.regHashServices = prev.regHashServices;
      snapshot.regHashFirewall = prev.regHashFirewall;
      snapshot.regHashUac = prev.regHashUac;
      snapshot.regHashEnvPath = prev.regHashEnvPath;
      snapshot.startupFolderCount = prev.startupFolderCount;
      snapshot.diskTotalBytes = prev.diskTotalBytes;
      snapshot.diskFreeBytes = prev.diskFreeBytes;
      snapshot.diskPctUsed = prev.diskPctUsed;
      snapshot.sehExceptionCount = prev.sehExceptionCount;
      snapshot.accessViolationCount = prev.accessViolationCount;
      snapshot.stackOverflowCount = prev.stackOverflowCount;
      snapshot.heapCorruptionDetected = prev.heapCorruptionDetected;
      snapshot.crashEventsToday = prev.crashEventsToday;
      snapshot.processHash = prev.processHash;
    }
  }
  g_phase = "POLL:SNAP_CREATED";
  HeapOk("SNAP_CREATED");

  wchar_t hostBuffer[MAX_COMPUTERNAME_LENGTH + 1] {};
  DWORD hostSize = static_cast<DWORD>(std::size(hostBuffer));
  if (GetComputerNameW(hostBuffer, &hostSize)) {
    snapshot.host = hostBuffer;
  }
  g_phase = "POLL:HOST_DONE";
  HeapOk("HOST_DONE");

  LARGE_INTEGER nowQpc {};
  QueryPerformanceCounter(&nowQpc);
  double elapsedSeconds = 0.0;
  if (lastNativeSampleQpc_.QuadPart > 0 && qpcFrequency_.QuadPart > 0) {
    elapsedSeconds = static_cast<double>(nowQpc.QuadPart - lastNativeSampleQpc_.QuadPart) / static_cast<double>(qpcFrequency_.QuadPart);
  }
  if (elapsedSeconds <= 0.0) {
    elapsedSeconds = 0.10;
  }

  RamInfo ramInfo;
  hw_read_ram(&ramInfo);
  snapshot.ramTotalBytes = ramInfo.totalBytes;
  snapshot.ramUsedBytes = ramInfo.usedBytes;
  g_phase = "POLL:RAM_DONE";
  HeapOk("RAM_DONE");

  PERFORMANCE_INFORMATION performance {};
  performance.cb = sizeof(performance);
  GetPerformanceInfo(&performance, sizeof(performance));
  snapshot.handleCount = static_cast<int>(performance.HandleCount);
  snapshot.processCount = static_cast<int>(performance.ProcessCount);
  snapshot.threadCount = static_cast<int>(performance.ThreadCount);
  g_phase = "POLL:PERF_DONE";
  HeapOk("PERF_DONE");

  MemoryInfo memInfo;
  hw_read_memory_info(&memInfo);
  snapshot.ramAvailBytes = memInfo.availablePhysicalBytes;
  g_phase = "POLL:MEM_DONE";
  HeapOk("MEM_DONE");

  CpuTimes cpuTimes;
  hw_read_cpu_times(&cpuTimes);
  g_phase = "POLL:CPUTIMES_DONE";
  HeapOk("CPUTIMES_DONE");

  if (!cpuBaselineCaptured_) {
    g_phase = "POLL:CPUID_START";
    cpu_identify(&cpuBaseline_);
    g_phase = "POLL:CPUID_DONE";
    cpuBaselineCaptured_ = true;
    CpuMeta meta;
    g_phase = "POLL:CPU_META_START";
    hw_read_cpu_meta(&meta);
    g_phase = "POLL:CPU_META_DONE2";
    hw_estimate_base_clock(&meta);
    g_phase = "POLL:BASE_CLOCK_DONE";
    cpuBaseTscPerSec_ = meta.tscPerSec;
  }
  g_phase = "POLL:BASELINE_DONE";

  memcpy(snapshot.cpuVendor, cpuBaseline_.vendor, sizeof(snapshot.cpuVendor));
  memcpy(snapshot.cpuBrand, cpuBaseline_.brand, sizeof(snapshot.cpuBrand));
  g_phase = "POLL:CPU_META_DONE";
  HeapOk("CPU_META_DONE");
  snapshot.cpuFamily = cpuBaseline_.family;
  snapshot.cpuModel = cpuBaseline_.model;
  snapshot.cpuStepping = cpuBaseline_.stepping;
  snapshot.cpuCores = cpuBaseline_.physicalCores;
  snapshot.cpuFeaturesEdx = cpuBaseline_.featuresEdx;
  snapshot.cpuFeaturesEcx = cpuBaseline_.featuresEcx;
  snapshot.cpuExtFeatures = cpuBaseline_.extFeatures;

  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  snapshot.cpuLogicalCpus = static_cast<uint32_t>(sysInfo.dwNumberOfProcessors);
  snapshot.cpuHtEnabled = (cpuBaseline_.featuresEdx & (1 << 28)) ? 1 : 0;

  {
    uint64_t tsc = cpu_read_tsc();
    snapshot.cpuTscDelta = tsc - prevCpuTsc_;
    prevCpuTsc_ = tsc;

    if (cpuTimesInitialized_ && elapsedSeconds > 0.0 && snapshot.cpuTscDelta > 0) {
      snapshot.estimatedFrequencyMhz = (double)(snapshot.cpuTscDelta / (uint64_t)(elapsedSeconds * 1000000.0));
    }
    cpuTimesInitialized_ = true;
  }

  if (cpuTimes.userTime > 0) {
    const uint64_t kernelOnly = cpuTimes.kernelTime - cpuTimes.idleTime;
    snapshot.ipcEstimate = (double)cpuTimes.userTime / (double)(kernelOnly + cpuTimes.userTime);
  }

  HeapOk("BEFORE_MAPS");
  volatile uint64_t stackCanary1 = 0xDEADBEEFCAFE1234ULL;
  volatile uint64_t stackCanary2 = 0x1234567890ABCDEFULL;
  std::map<int, std::wstring> processNames;
  std::map<int, NativeProcessSample> currentSamples;
  std::uint64_t systemTotalDeltaForProcesses = 0;
  std::uint64_t totalReadBytes = 0;
  std::uint64_t totalWriteBytes = 0;

  HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (processSnapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W entry {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(processSnapshot, &entry)) {
      do {
        if (entry.th32ProcessID == 0 || entry.th32ProcessID == GetCurrentProcessId()) {
          continue;
        }

        ProcessInfo process;
        process.name = entry.szExeFile;
        process.pid = static_cast<int>(entry.th32ProcessID);
        process.parentPid = static_cast<int>(entry.th32ParentProcessID);
        process.status = L"ACTIVE";
        process.priority = L"NORMAL";
        processNames[process.pid] = process.name;

        DWORD sessionId = 0;
        if (ProcessIdToSessionId(entry.th32ProcessID, &sessionId)) {
          process.sessionId = static_cast<int>(sessionId);
        }

        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        NativeProcessSample sample {};
        if (handle) {
          PROCESS_MEMORY_COUNTERS_EX memoryCounters {};
          if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters), sizeof(memoryCounters))) {
            process.ramBytes = memoryCounters.WorkingSetSize;
          }

          FILETIME createTime {};
          FILETIME exitTime {};
          FILETIME kernel {};
          FILETIME user {};
          if (GetProcessTimes(handle, &createTime, &exitTime, &kernel, &user)) {
            sample.createTime = FileTimeToUInt64(createTime);
            sample.cpuTime = FileTimeToUInt64(kernel) + FileTimeToUInt64(user);
            process.processGuid = FormatProcessGuid(process.pid, sample.createTime);
          }

          IO_COUNTERS io {};
          if (GetProcessIoCounters(handle, &io)) {
            sample.readBytes = io.ReadTransferCount;
            sample.writeBytes = io.WriteTransferCount;
            totalReadBytes += sample.readBytes;
            totalWriteBytes += sample.writeBytes;
          }

          DWORD handleCount = 0;
          if (GetProcessHandleCount(handle, &handleCount)) {
            snapshot.handleCount += static_cast<int>(handleCount);
          }

          const DWORD priority = GetPriorityClass(handle);
          switch (priority) {
            case HIGH_PRIORITY_CLASS: process.priority = L"HIGH"; break;
            case IDLE_PRIORITY_CLASS: process.priority = L"LOW"; break;
            case REALTIME_PRIORITY_CLASS: process.priority = L"REALTIME"; break;
            case BELOW_NORMAL_PRIORITY_CLASS: process.priority = L"BELOW"; break;
            case ABOVE_NORMAL_PRIORITY_CLASS: process.priority = L"ABOVE"; break;
            default: process.priority = L"NORMAL"; break;
          }
          CloseHandle(handle);
        }

        const auto previousSample = previousProcessSamples_.find(process.pid);
        if (previousSample != previousProcessSamples_.end() && systemTotalDeltaForProcesses > 0 && sample.cpuTime >= previousSample->second.cpuTime) {
          const std::uint64_t processDelta = sample.cpuTime - previousSample->second.cpuTime;
          process.cpuPct = std::clamp((static_cast<double>(processDelta) / static_cast<double>(systemTotalDeltaForProcesses)) * 100.0, 0.0, 100.0);
        }

        currentSamples[process.pid] = sample;
        snapshot.threadCount += static_cast<int>(entry.cntThreads);

        const std::wstring upper = ToUpper(process.name);
        if (upper.find(L"LSASS") != std::wstring::npos || upper.find(L"CSRSS") != std::wstring::npos || upper.find(L"SYSTEM") != std::wstring::npos || upper.find(L"SVCHOST") != std::wstring::npos) {
          process.status = L"SYSTEM";
        } else if (upper.find(L"CHATGPT") != std::wstring::npos || upper.find(L"MONIX") != std::wstring::npos) {
          process.status = L"RUNNING";
        }

        snapshot.processes.push_back(std::move(process));
      } while (Process32NextW(processSnapshot, &entry));
    }
    CloseHandle(processSnapshot);
  }
  g_phase = "POLL:PROC_ENUM_DONE";

  static std::uint64_t previousSystemTotalForRate = 0;
  previousSystemTotalForRate = lastSystemKernelTime_ + lastSystemUserTime_;

  if (nativeBaselineReady_ && elapsedSeconds > 0.0) {
    snapshot.diskReadBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalReadBytes - std::min(totalReadBytes, lastDiskReadBytes_)) / elapsedSeconds);
    snapshot.diskWriteBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalWriteBytes - std::min(totalWriteBytes, lastDiskWriteBytes_)) / elapsedSeconds);
  }
  lastDiskReadBytes_ = totalReadBytes;
  lastDiskWriteBytes_ = totalWriteBytes;

  StorageInfo storInfo;
  hw_read_storage_info(&storInfo);
  snapshot.diskTotalBytes = storInfo.usage.totalBytes;
  snapshot.diskFreeBytes = storInfo.usage.freeBytes;
  snapshot.diskPctUsed = storInfo.usage.usedPercent;
  snapshot.diskQueueLength = storInfo.performance.diskQueueLength;
  snapshot.diskReadLatencyMs = storInfo.performance.readLatencyMs;
  snapshot.diskWriteLatencyMs = storInfo.performance.writeLatencyMs;
  snapshot.diskReadIops = storInfo.performance.readOpsPerSec;
  snapshot.diskWriteIops = storInfo.performance.writeOpsPerSec;
  snapshot.smartHealthOk = storInfo.health.smartHealthOk;
  snapshot.nvmeTempC = storInfo.health.temperatureC;
  snapshot.nvmeTempValid = storInfo.health.temperatureValid;
  snapshot.previousDiskReadBytesPerSec = lastDiskReadBytesPerSec_;
  snapshot.previousDiskWriteBytesPerSec = lastDiskWriteBytesPerSec_;
  snapshot.previousDiskReadIops = lastDiskReadIops_;
  snapshot.previousDiskWriteIops = lastDiskWriteIops_;
  lastDiskReadBytesPerSec_ = snapshot.diskReadBytesPerSec;
  lastDiskWriteBytesPerSec_ = snapshot.diskWriteBytesPerSec;
  lastDiskReadIops_ = snapshot.diskReadIops;
  lastDiskWriteIops_ = snapshot.diskWriteIops;

  std::uint64_t totalNetIn = 0;
  std::uint64_t totalNetOut = 0;
  ULONG ifTableSize = 0;
  GetIfTable(nullptr, &ifTableSize, FALSE);
  if (ifTableSize > 0) {
    std::vector<BYTE> ifBuffer(ifTableSize);
    auto* ifTable = reinterpret_cast<MIB_IFTABLE*>(ifBuffer.data());
    if (GetIfTable(ifTable, &ifTableSize, FALSE) == NO_ERROR) {
      for (DWORD i = 0; i < ifTable->dwNumEntries; ++i) {
        const auto& row = ifTable->table[i];
        if (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL && row.dwType != IF_TYPE_SOFTWARE_LOOPBACK) {
          totalNetIn += row.dwInOctets;
          totalNetOut += row.dwOutOctets;
        }
      }
    }
  }
  if (nativeBaselineReady_ && elapsedSeconds > 0.0) {
    snapshot.netDownBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalNetIn - std::min(totalNetIn, lastNetworkInBytes_)) / elapsedSeconds);
    snapshot.netUpBytesPerSec = static_cast<std::uint64_t>(static_cast<double>(totalNetOut - std::min(totalNetOut, lastNetworkOutBytes_)) / elapsedSeconds);
  }
  lastNetworkInBytes_ = totalNetIn;
  lastNetworkOutBytes_ = totalNetOut;

  std::map<int, NetworkFlow> flowByPid;
  auto flowName = [&](DWORD pid) {
    const auto name = processNames.find(static_cast<int>(pid));
    if (name != processNames.end()) {
      return name->second + L" PID " + std::to_wstring(pid);
    }
    return L"PID " + std::to_wstring(pid);
  };

  DWORD tableSize = 0;
  GetExtendedTcpTable(nullptr, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
  if (tableSize > 0) {
    std::vector<BYTE> buffer(tableSize);
    auto* tcpTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    if (GetExtendedTcpTable(tcpTable, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
      for (DWORD i = 0; i < tcpTable->dwNumEntries; ++i) {
        const auto& row = tcpTable->table[i];
        if (row.dwOwningPid == GetCurrentProcessId()) {
          continue;
        }
        NetworkFlow& flow = flowByPid[static_cast<int>(row.dwOwningPid)];
        flow.name = flowName(row.dwOwningPid);
        if (row.dwState == MIB_TCP_STATE_ESTAB) {
          ++flow.activeConnections;
          ++snapshot.outboundConnections;
          flow.state = L"ACTIVE";
          const std::wstring remote = FormatIpv4(row.dwRemoteAddr) + L":" + std::to_wstring(ntohs(static_cast<u_short>(row.dwRemotePort)));
          if (flow.remote.empty()) {
            flow.remote = remote;
          } else if (flow.remote.size() < 120 && flow.remote.find(remote) == std::wstring::npos) {
            flow.remote += L", " + remote;
          }
        } else if (row.dwState == MIB_TCP_STATE_LISTEN) {
          ++snapshot.inboundConnections;
          if (flow.remote.empty()) {
            flow.remote = L"listening :" + std::to_wstring(ntohs(static_cast<u_short>(row.dwLocalPort)));
          }
          if (flow.state.empty()) {
            flow.state = L"LISTEN";
          }
        }
      }
    }
  }

  tableSize = 0;
  GetExtendedUdpTable(nullptr, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
  if (tableSize > 0) {
    std::vector<BYTE> buffer(tableSize);
    auto* udpTable = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());
    if (GetExtendedUdpTable(udpTable, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
      snapshot.udpConnectionCount = static_cast<int>(udpTable->dwNumEntries);
      for (DWORD i = 0; i < udpTable->dwNumEntries; ++i) {
        const auto& row = udpTable->table[i];
        if (row.dwOwningPid == GetCurrentProcessId()) {
          continue;
        }
        NetworkFlow& flow = flowByPid[static_cast<int>(row.dwOwningPid)];
        flow.name = flowName(row.dwOwningPid);
        flow.state = flow.state.empty() ? L"UDP" : flow.state;
        if (flow.remote.empty()) {
          flow.remote = L"udp :" + std::to_wstring(ntohs(static_cast<u_short>(row.dwLocalPort)));
        }
      }
    }
  }

  for (auto it = flowByPid.begin(); it != flowByPid.end(); ++it) {
    auto& flow = it->second;
    if (flow.remote.empty()) {
      flow.remote = L"local";
    }
    if (flow.state.empty()) {
      flow.state = L"IDLE";
    }
    snapshot.flows.push_back(std::move(flow));
  }

  std::sort(snapshot.flows.begin(), snapshot.flows.end(), [](const NetworkFlow& left, const NetworkFlow& right) {
    return left.activeConnections > right.activeConnections;
  });
  if (snapshot.flows.size() > 24) {
    snapshot.flows.resize(24);
  }

  snapshot.dnsPseudo = std::clamp(snapshot.outboundConnections / 4, 0, 96);
  snapshot.latencyMs = snapshot.outboundConnections > 0 ? std::clamp(8 + snapshot.outboundConnections / 3, 8, 85) : 0;

  snapshot.tcpResets = CountTcpResets();

  MIB_TCPSTATS tcpStats {};
  if (GetTcpStatistics(&tcpStats) == NO_ERROR) {
    snapshot.tcpRetransmits = tcpStats.dwRetransSegs;
  }

  CollectSchedulerData(snapshot);
  CollectNetworkDiagnostics(snapshot);
  g_phase = "POLL:SCHED_NET_DONE";
  if (snapshot.pingRttMs >= 0) {
    snapshot.latencyMs = snapshot.pingRttMs;
  }
  if (snapshot.dnsResolutionMs >= 0) {
    snapshot.dnsPseudo = std::clamp(snapshot.dnsResolutionMs, 0, 96);
  }

  if (nativeBaselineReady_ && elapsedSeconds > 0.0) {
    const uint64_t csDelta = snapshot.totalContextSwitches >= prevTotalContextSwitches_ ?
      snapshot.totalContextSwitches - prevTotalContextSwitches_ : 0;
    snapshot.contextSwitchesPerSec = static_cast<int>(static_cast<double>(csDelta) / elapsedSeconds);
    const uint64_t irqDelta = snapshot.totalInterruptCount >= prevTotalInterruptCount_ ?
      snapshot.totalInterruptCount - prevTotalInterruptCount_ : 0;
    snapshot.interruptsPerSec = static_cast<int>(static_cast<double>(irqDelta) / elapsedSeconds);
    snapshot.processorQueueLength = snapshot.cpuPct >= 80.0 ? std::max(1, static_cast<int>(snapshot.cpuPct / 20.0)) : 0;
    snapshot.systemCallsPerSec = static_cast<int>(snapshot.processCount * 120);
    snapshot.threadCreationDelta = snapshot.threadCount - (state_.hasPreviousSnapshot ? state_.previousSnapshot->threadCount : snapshot.threadCount);
    snapshot.threadTerminationDelta = 0;
  } else {
    snapshot.processorQueueLength = snapshot.cpuPct >= 80.0 ? std::max(1, static_cast<int>(snapshot.cpuPct / 20.0)) : 0;
    snapshot.contextSwitchesPerSec = static_cast<int>(snapshot.threadCount * 1.8);
    snapshot.systemCallsPerSec = static_cast<int>(snapshot.processCount * 120);
    snapshot.interruptsPerSec = static_cast<int>(20 + snapshot.cpuPct * 6.0);
  }
  prevTotalContextSwitches_ = snapshot.totalContextSwitches;
  prevTotalInterruptCount_ = snapshot.totalInterruptCount;
  prevTotalDpcCount_ = snapshot.totalDpcCount;
  prevTotalIsrCount_ = snapshot.totalIsrCount;
  snapshot.uptimeSeconds = hw_get_uptime_ms() / 1000ull;
  snapshot.uptimeMs = GetTickCount64();

  {
    PDH_HQUERY query {};
    PDH_FMT_COUNTERVALUE gpuVal {};
    if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) {
      PDH_HCOUNTER counter {};
      PdhAddEnglishCounterW(query, L"\\GPU Engine(*Total)\\Utilization Percentage", 0, &counter);
      PdhCollectQueryData(query);
      if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &gpuVal) == ERROR_SUCCESS) {
        snapshot.gpuPct = std::clamp(gpuVal.doubleValue, 0.0, 100.0);
      }
      PdhCloseQuery(query);
    }
    snapshot.gpuTempC = 35.0 + (snapshot.gpuPct * 0.58);
    snapshot.gpuTempEstimated = 1;
  }

  CollectGpuDisplayInfo(snapshot);
  g_phase = "POLL:GPU_DONE";
  CollectAudioData(snapshot);
  g_phase = "POLL:AUDIO_DONE";

  if (sc % 4 == 0) {
    CollectOsKernelData(snapshot);
    g_phase = "POLL:OS_DONE";
    CollectPowerData(snapshot);
    g_phase = "POLL:POWER_DONE";
    CollectThermalData(snapshot);
    g_phase = "POLL:THERMAL_DONE";
  }
  if (sc % 5 == 0) {
    CollectSecurityData(snapshot);
    g_phase = "POLL:SECURITY_DONE";
    CollectHardwareBoardData(snapshot);
    g_phase = "POLL:HW_DONE";
  }
  if (sc % 10 == 0) {
    CollectFilesystemData(snapshot);
    g_phase = "POLL:FS_DONE";
    CollectRegistryData(snapshot);
    g_phase = "POLL:REG_DONE";
    CollectReliabilityData(snapshot);
    g_phase = "POLL:RELIABILITY_DONE";
  }

  prevDriverNames_ = snapshot.driverNames;
  prevTotalHandles_ = snapshot.totalHandles;
  prevTotalObjects_ = snapshot.totalObjects;
  prevPageFaultsDelta_ = snapshot.pageFaultsDelta;
  prevIoReadBytesDelta_ = snapshot.ioReadBytesDelta;
  prevIoWriteBytesDelta_ = snapshot.ioWriteBytesDelta;
  prevSystemTime100ns_ = snapshot.systemTime100ns;
  prevSessionCount_ = snapshot.sessionCount;
  prevSelfSignatureValid_ = snapshot.selfSignatureValid;
  prevSelfHashVerified_ = snapshot.selfHashVerified;
  prevUnsignedDriverCount_ = snapshot.unsignedDriverCount;
  prevUnsignedDriverNames_ = snapshot.unsignedDriverNames;
  prevSuspiciousScriptHosts_ = snapshot.suspiciousScriptHosts;
  prevUacConsentProcesses_ = snapshot.uacConsentProcesses;
  prevLsassAccessCount_ = snapshot.lsassAccessCount;
  prevDebugPortActive_ = snapshot.debugPortActive;
  prevHookModulesDetected_ = snapshot.hookModulesDetected;
  prevPeHeaderTamper_ = snapshot.peHeaderTamper;
  prevScheduledTaskCount_ = snapshot.scheduledTaskCount;
  prevSuspiciousModules_ = snapshot.suspiciousModules;
  prevAcLineStatus_ = snapshot.acLineStatus;
  prevBatteryFlag_ = snapshot.batteryFlag;
  prevBatteryLifePercent_ = snapshot.batteryLifePercent;
  prevBatteryLifeTimeSec_ = snapshot.batteryLifeTimeSec;
  prevBatteryChargeRate_ = snapshot.batteryChargeRate;
  prevBatteryChargeState_ = snapshot.batteryChargeState;
  prevBatteryWearLevel_ = snapshot.batteryWearLevel;
  prevBatteryCycleCount_ = snapshot.batteryCycleCount;
  prevBatteryTemperature_ = snapshot.batteryTemperature;
  prevPowerPlanIndex_ = snapshot.powerPlanIndex;
  prevPowerSaverActive_ = snapshot.powerSaverActive;
  prevHighPerfActive_ = snapshot.highPerfActive;
  prevIdlePowerDrawHigh_ = snapshot.idlePowerDrawHigh;
  prevCpuCoreTempC_ = snapshot.cpuCoreTempC;
  prevCpuCoreTempMax_ = snapshot.cpuCoreTempMax;
  prevMotherboardTempC_ = snapshot.motherboardTempC;
  prevVrmTempC_ = snapshot.vrmTempC;
  prevAmbientTempC_ = snapshot.ambientTempC;
  prevCpuThrottling_ = snapshot.cpuThrottling;
  prevFanCount_ = snapshot.fanCount;
  prevFanSpeeds_ = snapshot.fanSpeeds;
  prevPumpSpeed_ = snapshot.pumpSpeed;
  prevThermalSensorCount_ = snapshot.thermalSensorCount;
  prevThermalSensorFailures_ = snapshot.thermalSensorFailures;
  prevVoltage12V_ = snapshot.voltage12V;
  prevVoltage5V_ = snapshot.voltage5V;
  prevVoltage33V_ = snapshot.voltage33V;
  prevVoltageVcore_ = snapshot.voltageVcore;
  prevTpmPresent_ = snapshot.tpmPresent;
  prevTpmReady_ = snapshot.tpmReady;
  prevCmosBatteryOk_ = snapshot.cmosBatteryOk;
  prevSensorPollFailures_ = snapshot.sensorPollFailures;

  prevAudioOutputDeviceCount_ = snapshot.audioOutputDeviceCount;
  prevAudioInputDeviceCount_ = snapshot.audioInputDeviceCount;
  prevAudioMixerCount_ = snapshot.audioMixerCount;
  prevAudioMasterVolume_ = snapshot.audioMasterVolume;
  prevAudioMasterMuted_ = snapshot.audioMasterMuted;

  prevSehExceptionCount_ = snapshot.sehExceptionCount;
  prevUnhandledExceptionCount_ = snapshot.unhandledExceptionCount;
  prevAccessViolationCount_ = snapshot.accessViolationCount;
  prevHeapCorruptionDetected_ = snapshot.heapCorruptionDetected;

  std::sort(snapshot.processes.begin(), snapshot.processes.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
    if (left.cpuPct == right.cpuPct) {
      return left.ramBytes > right.ramBytes;
    }
    return left.cpuPct > right.cpuPct;
  });

  if (snapshot.processes.size() > 512) {
    snapshot.processes.resize(512);
  }

  previousProcessSamples_ = std::move(currentSamples);
  lastNativeSampleQpc_ = nowQpc;
  nativeBaselineReady_ = true;
  return *snapshotPtr;
}

std::vector<ProcessInfo> MonixApp::BuildFallbackProcesses(const Snapshot& snapshot) {
  HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (processSnapshot == INVALID_HANDLE_VALUE) {
    return {};
  }

  std::vector<ProcessInfo> results;
  PROCESSENTRY32W entry {};
  entry.dwSize = sizeof(entry);

  if (Process32FirstW(processSnapshot, &entry)) {
    do {
      const std::wstring name = entry.szExeFile;
      if (name.empty() || name == L"Idle" || name == L"System Idle Process") {
        continue;
      }

      ProcessInfo process;
      process.name = name;
      process.pid = static_cast<int>(entry.th32ProcessID);
      process.status = L"ACTIVE";
      process.priority = L"NORMAL";

      HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
      if (handle) {
        PROCESS_MEMORY_COUNTERS_EX counters {};
        if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
          process.ramBytes = counters.WorkingSetSize;
        }

        const DWORD priority = GetPriorityClass(handle);
        switch (priority) {
          case HIGH_PRIORITY_CLASS: process.priority = L"HIGH"; break;
          case IDLE_PRIORITY_CLASS: process.priority = L"LOW"; break;
          case REALTIME_PRIORITY_CLASS: process.priority = L"REALTIME"; break;
          case BELOW_NORMAL_PRIORITY_CLASS: process.priority = L"BELOW"; break;
          case ABOVE_NORMAL_PRIORITY_CLASS: process.priority = L"ABOVE"; break;
          default: process.priority = L"NORMAL"; break;
        }
        CloseHandle(handle);
      }

      const std::wstring lowered = ToUpper(name);
      if (lowered.find(L"GAME") != std::wstring::npos || lowered.find(L"MONIX") != std::wstring::npos || lowered.find(L"CHATGPT") != std::wstring::npos) {
        process.status = L"RUNNING";
      } else if (lowered.find(L"EXPLORER") != std::wstring::npos) {
        process.status = L"SYSTEM";
      } else if (lowered.find(L"SYSTEM") != std::wstring::npos || lowered.find(L"SVCHOST") != std::wstring::npos) {
        process.status = L"KERNEL";
      } else if (lowered.find(L"OBS") != std::wstring::npos) {
        process.status = L"RECORDING";
      }

      results.push_back(process);
    } while (Process32NextW(processSnapshot, &entry));
  }

  CloseHandle(processSnapshot);
  std::sort(results.begin(), results.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
    return left.ramBytes > right.ramBytes;
  });

  if (results.size() > 18) {
    results.resize(18);
  }

  const std::array<double, 18> cpuWeights { 0.32, 0.18, 0.12, 0.10, 0.08, 0.06, 0.05, 0.04, 0.03, 0.02, 0.015, 0.012, 0.010, 0.008, 0.006, 0.005, 0.004, 0.003 };
  const std::array<double, 18> gpuWeights { 0.48, 0.18, 0.12, 0.08, 0.05, 0.04, 0.025, 0.018, 0.012, 0.010, 0.008, 0.006, 0.005, 0.004, 0.003, 0.003, 0.002, 0.002 };
  for (std::size_t i = 0; i < results.size(); ++i) {
    results[i].cpuPct = std::clamp(snapshot.cpuPct * cpuWeights[i], 0.0, 100.0);
    results[i].gpuPct = std::clamp(snapshot.gpuPct * gpuWeights[i], 0.0, 100.0);
  }

  return results;
}

void MonixApp::IncrementCounter(ColorRole color) {
  switch (color) {
    case ColorRole::CriticalScram:      ++state_.counters.ai;
      break;
    case ColorRole::Network:
    case ColorRole::UserInput:
      if (color == ColorRole::Network) {
        ++state_.counters.network;
      } else {
        ++state_.counters.userInput;
      }
      break;
    case ColorRole::Scram:++state_.counters.ai;
      break;
    case ColorRole::Kernel:
      ++state_.counters.kernel;
      break;
    case ColorRole::Storage:
      ++state_.counters.storage;
      break;
    default:
      break;
  }
}

std::filesystem::path MonixApp::ResolveLogFilePath(const std::wstring& extension) const {
  SYSTEMTIME time {};
  GetLocalTime(&time);
  wchar_t baseName[64];
  swprintf(baseName, 64, L"monix-%04d-%02d-%02d", time.wYear, time.wMonth, time.wDay);

  std::filesystem::path candidate = paths_.logsDir / (std::wstring(baseName) + L"." + extension);
  if (!std::filesystem::exists(candidate) || std::filesystem::file_size(candidate) < config_.logMaxFileBytes) {
    return candidate;
  }

  for (int index = 2; index < 1000; ++index) {
    std::filesystem::path rotated = paths_.logsDir / (std::wstring(baseName) + L"-" + std::to_wstring(index) + L"." + extension);
    if (!std::filesystem::exists(rotated) || std::filesystem::file_size(rotated) < config_.logMaxFileBytes) {
      return rotated;
    }
  }

  return candidate;
}

void MonixApp::CleanupLogFiles() {
  if (!std::filesystem::exists(paths_.logsDir)) {
    return;
  }

  const auto cutoff = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * std::max<UINT>(1, config_.logRetentionDays));
  for (const auto& entry : std::filesystem::directory_iterator(paths_.logsDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto extension = ToUpper(entry.path().extension().wstring());
    if (extension != L".LOG" && extension != L".JSONL" && extension != L".CSV" && extension != L".JSON") {
      continue;
    }
    std::error_code ec;
    const auto lastWrite = std::filesystem::last_write_time(entry.path(), ec);
    if (!ec && lastWrite < cutoff) {
      std::filesystem::remove(entry.path(), ec);
    }
  }
}

void MonixApp::FlushLogQueues(bool force) {
  const ULONGLONG now = GetTickCount64();
  if (!force && now - lastLogFlushAtMs_ < config_.logFlushIntervalMs) {
    return;
  }

  // Acquire lock to safely swap out pending log vectors
  std::vector<std::wstring> plainBatch;
  std::vector<std::wstring> jsonBatch;
  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (pendingPlainLogs_.empty() && pendingJsonLogs_.empty() && !force) {
      if (now - lastRetentionSweepAtMs_ > 60000) {
        CleanupLogFiles();
        lastRetentionSweepAtMs_ = now;
      }
      return;
    }
    plainBatch.swap(pendingPlainLogs_);
    jsonBatch.swap(pendingJsonLogs_);
  }

  std::error_code directoryError;
  std::filesystem::create_directories(paths_.logsDir, directoryError);
  if (directoryError) {
    lastLogFlushAtMs_ = now;
    return;
  }

  if (config_.logPlainEnabled && !plainBatch.empty()) {
    std::wofstream output(ResolveLogFilePath(L"log"), std::ios::app);
    std::size_t written = 0;
    if (output.is_open()) {
      for (const auto& line : plainBatch) {
        output << line << L"\n";
        if (!output.good()) break;
        ++written;
      }
      output.flush();
      if (!output.good()) {
        written = std::min(written, plainBatch.size());
      }
    }
    if (written < plainBatch.size()) {
      // Put unflushed entries back
      std::lock_guard<std::mutex> lock(stateMutex_);
      pendingPlainLogs_.insert(pendingPlainLogs_.begin(),
        std::make_move_iterator(plainBatch.begin() + written),
        std::make_move_iterator(plainBatch.end()));
    }
  }

  if (config_.logJsonEnabled && !jsonBatch.empty()) {
    std::wofstream output(ResolveLogFilePath(L"jsonl"), std::ios::app);
    std::size_t written = 0;
    if (output.is_open()) {
      for (const auto& line : jsonBatch) {
        output << line << L"\n";
        if (!output.good()) break;
        ++written;
      }
      output.flush();
      if (!output.good()) {
        written = std::min(written, jsonBatch.size());
      }
    }
    if (written < jsonBatch.size()) {
      std::lock_guard<std::mutex> lock(stateMutex_);
      pendingJsonLogs_.insert(pendingJsonLogs_.begin(),
        std::make_move_iterator(jsonBatch.begin() + written),
        std::make_move_iterator(jsonBatch.end()));
    }
  }

  lastLogFlushAtMs_ = now;
  if (now - lastRetentionSweepAtMs_ > 60000 || force) {
    CleanupLogFiles();
    lastRetentionSweepAtMs_ = now;
  }
}

void MonixApp::QueueNotification(const LogEntry& entry) {
  if (!config_.notificationsEnabled) {
    return;
  }

  if (entry.level != LogLevel::Error && entry.level != LogLevel::Critical) {
    return;
  }

  NotificationItem item;
  item.title = entry.domain + L" :: " + entry.severity;
  item.message = entry.message;
  item.color = entry.color;
  item.level = entry.level;
  item.eventId = entry.eventId;
  item.expiresAtMs = GetTickCount64() + config_.notificationDurationMs;
  state_.notifications.insert(state_.notifications.begin(), std::move(item));
  if (static_cast<int>(state_.notifications.size()) > config_.notificationMaxStack) {
    state_.notifications.resize(config_.notificationMaxStack);
  }
  PlayAlertSound(entry.level);
}

void MonixApp::PlayAlertSound(LogLevel level) {
  if (!config_.soundEnabled || !state_.loggedIn) {
    return;
  }

  std::filesystem::path soundFile;
  switch (level) {
    case LogLevel::Critical:
    case LogLevel::Error:
      soundFile = paths_.soundDir / L"Log" / L"Error.mp3";
      break;
    case LogLevel::Warn:
      soundFile = paths_.soundDir / L"Log" / L"Warning.mp3";
      break;
    default:
      return;
  }

  if (!std::filesystem::exists(soundFile)) {
    return;
  }

  mciSendStringW(L"close monix_alert", nullptr, 0, nullptr);
  const std::wstring cmd = L"open \"" + soundFile.wstring() + L"\" type mpegvideo alias monix_alert";
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW(L"play monix_alert notify", nullptr, 0, nullptr);
  }
}

void MonixApp::PlayLogSound() {
  if (!config_.soundEnabled || !state_.loggedIn) {
    return;
  }
  static int logAliasIndex = 0;
  static const wchar_t* logAliases[] = {
    L"monix_log0", L"monix_log1", L"monix_log2",
    L"monix_log3", L"monix_log4", L"monix_log5",
    L"monix_log6", L"monix_log7", L"monix_log8",
    L"monix_log9", L"monix_logA", L"monix_logB"
  };
  constexpr int kMaxLogAliases = 12;
  const std::wstring alias = logAliases[logAliasIndex % kMaxLogAliases];
  logAliasIndex++;
  mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
  const std::wstring soundFile = (paths_.soundDir / L"Log" / L"log.wav").wstring();
  const std::wstring cmd = L"open \"" + soundFile + L"\" type waveaudio alias " + alias;
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW((L"play " + alias + L" from 0").c_str(), nullptr, 0, nullptr);
  }
}

void MonixApp::PlayClickSound() {
  if (!config_.soundEnabled || !state_.loggedIn) {
    return;
  }
  const std::filesystem::path soundFile = paths_.soundDir / L"App" / L"click.mp3";
  if (!std::filesystem::exists(soundFile)) {
    return;
  }
  mciSendStringW(L"close monix_click", nullptr, 0, nullptr);
  const std::wstring cmd = L"open \"" + soundFile.wstring() + L"\" type mpegvideo alias monix_click";
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW(L"play monix_click notify", nullptr, 0, nullptr);
  }
}


void MonixApp::SpawnDemoLogs() {
  if (!state_.loggedIn) {
    SetToast(L"Access denied — authentication required");
    return;
  }

  struct DemoEntry {
    std::wstring domain;
    std::wstring severity;
    std::wstring message;
    ColorRole color;
  };
  const DemoEntry entries[] = {
    { L"PROCESS",    L"INFO",     L"chrome.exe entered the active task set (PID 16376)", ColorRole::White },
    { L"PROCESS",    L"WARNING",  L"chrome.exe memory allocation increased rapidly", ColorRole::Warning },
    { L"PROCESS",    L"ERROR",    L"svchost.exe terminated unexpectedly (PID 10608)", ColorRole::Error },
    { L"CPU",        L"WARNING",  L"Sustained CPU pressure detected around discord.exe", ColorRole::Warning },
    { L"CPU",        L"ERROR",    L"Core throttling triggered — temperature above Tjunction", ColorRole::Error },
    { L"CPU",        L"INFO",     L"Context switches stabilized at 14518/s", ColorRole::Kernel },
    { L"RAM",        L"WARNING",  L"Memory residency crossed threshold: chrome.exe 2.9GB", ColorRole::Warning },
    { L"RAM",        L"ERROR",    L"Memory allocation failure imminent on java.exe", ColorRole::Error },
    { L"RAM",        L"CRITICAL", L"Pagefile exhaustion — system may become unstable", ColorRole::Fatal },
    { L"NETWORK",    L"INFO",     L"Latency 15ms, 23 established / 17 listening sockets", ColorRole::Network },
    { L"NETWORK",    L"WARNING",  L"ProtonVPN.Client.exe upload spike detected (3.2MB/s)", ColorRole::Warning },
    { L"NETWORK",    L"ERROR",    L"Outbound socket fan-out crossed threshold (42 active)", ColorRole::Error },
    { L"NETWORK",    L"ERROR",    L"Latency crossed degraded threshold: 180ms average", ColorRole::Error },
    { L"GPU",        L"WARNING",  L"GPU utilization sustained above 90% for 30 seconds", ColorRole::Warning },
    { L"GPU",        L"ERROR",    L"DXGI swap chain reported device removed", ColorRole::Error },
    { L"GPU",        L"CRITICAL", L"VRAM allocation failure — driver reset required", ColorRole::Fatal },
    { L"DISK",       L"WARNING",  L"Storage temperature climbed above normal range", ColorRole::Warning },
    { L"DISK",       L"INFO",     L"Write throughput: 142 MB/s on volume C:", ColorRole::White },
    { L"DISK",       L"ERROR",    L"S.M.A.R.T. threshold exceeded: reallocated sector count", ColorRole::Error },
    { L"KERNEL",     L"INFO",     L"Queue 0 | switches 13429/s | IRQ 401/s", ColorRole::Kernel },
    { L"KERNEL",     L"INFO",     L"Thread scheduler latency: 1.2ms", ColorRole::Kernel },
    { L"KERNEL",     L"WARNING",  L"DPC latency spike: 4.8ms on core 3", ColorRole::Kernel },
    { L"SCRAM",      L"INFO",     L"Memory pressure is increasing", ColorRole::Scram },
    { L"SCRAM",      L"WARNING",  L"Heuristic cluster divergence in network module", ColorRole::Scram },
    { L"SCRAM",      L"ERROR",    L"Behavioral anomaly cluster detected — escalation pending", ColorRole::Fatal },
    { L"SCRAM",      L"CRITICAL", L"Anomalous process behavior detected — initiating triage", ColorRole::Fatal },
    { L"S.C.R.A.M",  L"CRITICAL", L"TEMPERATURE: INCREASING FAN SPEED (3% > 40%)", ColorRole::Fatal },
    { L"S.C.R.A.M",  L"WARNING",  L"Thermal envelope approaching critical margins", ColorRole::Scram },
    { L"ENGINE",     L"INFO",     L"UI render latency stable at 4ms", ColorRole::Success },
    { L"SYSTEM",     L"SUCCESS",  L"Collector synchronized with kernel counters", ColorRole::Success },
    { L"APPLICATION",L"ERROR",    L"render_service.exe crashed — Exception: 0xc0000005", ColorRole::Error },
    { L"APPLICATION",L"WARNING",  L"Plugin host unresponsive for 2400ms", ColorRole::Warning },
    { L"TASKS",      L"INFO",     L"Inspection focus moved to discord.exe (PID 15604)", ColorRole::White },
    { L"CONFIG",     L"INFO",     L"Settings are writable live — hot reload active", ColorRole::Dim },
    { L"SECURITY",   L"WARNING",  L"Failed login attempt from unknown session", ColorRole::Warning },
    { L"SECURITY",   L"ERROR",    L"Integrity check failed on config block", ColorRole::Error },
    { L"SECURITY",   L"CRITICAL", L"Unauthorized access attempt blocked", ColorRole::Fatal },
    { L"THERMAL",    L"WARNING",  L"CPU package temp: 92°C — fan curve adjusting", ColorRole::Warning },
    { L"THERMAL",    L"ERROR",    L"Thermal throttling active on all cores", ColorRole::Error },
    { L"AUTH",       L"INFO",     L"Session authenticated — user: admin", ColorRole::Success },
    { L"AUTH",       L"WARNING",  L"Lockout timer active — 2 attempts remaining", ColorRole::Warning },
    { L"TEST",       L"INFO",     L"Unit test suite passed: 142/142 assertions", ColorRole::Success },
    { L"TEST",       L"WARNING",  L"Integration test latency exceeded 500ms threshold", ColorRole::Warning },
    { L"TEST",       L"ERROR",    L"Regression detected: test_kernel_scheduler failed", ColorRole::Error },
    { L"TEST",       L"CRITICAL", L"Smoke test abort — critical path broken in module core", ColorRole::Fatal },
    { L"TEST",       L"INFO",     L"Benchmark: render loop 16.2ms avg (target <16.6ms)", ColorRole::Success },
    { L"TEST",       L"WARNING",  L"Memory leak detected in test_cleanup_teardown", ColorRole::Warning },
  };
  constexpr int kTotal = 20;
  constexpr int kEntryCount = sizeof(entries) / sizeof(entries[0]);
  for (int i = 0; i < kTotal; ++i) {
    const auto& e = entries[(GetTickCount64() + i * 7 + i * i * 3) % kEntryCount];
    const std::wstring tag = L" #" + std::to_wstring(i) + L" t" + std::to_wstring(GetTickCount64() % 10000);
    PushLog(e.domain, e.severity, e.message + tag, e.color, L"core", L"demo");
  }
  SetToast(L"Spawned " + std::to_wstring(kTotal) + L" demo entries — all types");
}

void MonixApp::PushLog(
  std::wstring domain,
  std::wstring severity,
  std::wstring message,
  ColorRole color,
  std::wstring module,
  std::wstring service,
  std::wstring metadata
) {
  const LogLevel level = ParseLogLevel(severity);
  const std::wstring normalizedSeverity = LogLevelText(level);
  if (LogLevelRank(level) < LogLevelRank(config_.logLevel)) {
    return;
  }

  if (config_.logDeduplicate && domain != L"SCRAM" && !state_.logs.empty()) {
    const int scanLimit = (std::min)(static_cast<int>(state_.logs.size()), 20);
    for (int i = 0; i < scanLimit; ++i) {
      auto& existing = state_.logs[i];
      if (existing.domain == domain &&
          existing.severity == normalizedSeverity &&
          existing.message == message &&
          existing.module == module &&
          existing.service == service) {
        existing.repeatCount += 1;
        existing.time = FormatClockNow(config_.logMilliseconds);
        existing.fullTimestamp = FormatIsoTimestampNow(config_.logMilliseconds);
        return;
      }
    }
  }

  LogEntry entry;
  entry.time = FormatClockNow(config_.logMilliseconds);
  entry.fullTimestamp = FormatIsoTimestampNow(config_.logMilliseconds);
  entry.domain = std::move(domain);
  entry.severity = normalizedSeverity;
  entry.module = std::move(module);
  entry.service = std::move(service);
  entry.message = std::move(message);
  entry.metadata = std::move(metadata);
  entry.sessionId = state_.sessionId;
  entry.userId = config_.userId;
  entry.eventId = state_.nextEventId++;
  entry.processId = GetCurrentProcessId();
  entry.threadId = GetCurrentThreadId();
  entry.level = level;
  entry.color = color;

  state_.logs.insert(state_.logs.begin(), entry);
  if (static_cast<int>(state_.logs.size()) > config_.logBufferSize) {
    state_.logs.resize(config_.logBufferSize);
  }

  switch (level) {
    case LogLevel::Debug: ++state_.counters.debug; break;
    case LogLevel::Info: ++state_.counters.info; break;
    case LogLevel::Warn: ++state_.counters.warnings; break;
    case LogLevel::Error: ++state_.counters.errors; break;
    case LogLevel::Critical: ++state_.counters.critical; break;
  }
  IncrementCounter(color);

  const bool isWarn   = (level == LogLevel::Warn);
  const bool isErr    = (level == LogLevel::Error);
  const bool isCrit   = (level == LogLevel::Critical);
  const bool isKernel = (color == ColorRole::Kernel);
  const bool isNet    = (color == ColorRole::Network);
  const double alpha = 0.15;
  const auto ema = [&](std::vector<double>& hist, bool hit) {
    double prev = hist.empty() ? 0.0 : hist.back();
    double next = prev * (1.0 - alpha) + (hit ? alpha : 0.0);
    AppendBounded(hist, next, config_.historyCapacity);
  };
  ema(state_.logWarnHistory,   isWarn);
  ema(state_.logErrHistory,    isErr);
  ema(state_.logCritHistory,   isCrit);
  ema(state_.logKernelHistory, isKernel);
  ema(state_.logNetHistory,    isNet);
  if (level == LogLevel::Warn || level == LogLevel::Error || level == LogLevel::Critical) {
    QueueNotification(state_.logs.front());
  } else {
    PlayLogSound();
  }

  if (config_.logPlainEnabled) {
    std::wstring plain = entry.fullTimestamp +
      L" [" + entry.severity + L"]" +
      L" [" + entry.domain + L"]" +
      L" [module=" + entry.module + L"]" +
      L" [service=" + entry.service + L"]" +
      L" [session=" + entry.sessionId + L"]" +
      L" [event=" + std::to_wstring(entry.eventId) + L"]" +
      L" [pid=" + std::to_wstring(entry.processId) + L"]" +
      L" [tid=" + std::to_wstring(entry.threadId) + L"] " +
      entry.message;
    if (!entry.metadata.empty()) {
      plain += L" { " + entry.metadata + L" }";
    }
    pendingPlainLogs_.push_back(std::move(plain));
  }

  if (config_.logJsonEnabled) {
    std::wstring json =
      L"{\"timestamp\":\"" + EscapeJson(entry.fullTimestamp) +
      L"\",\"level\":\"" + EscapeJson(entry.severity) +
      L"\",\"domain\":\"" + EscapeJson(entry.domain) +
      L"\",\"module\":\"" + EscapeJson(entry.module) +
      L"\",\"service\":\"" + EscapeJson(entry.service) +
      L"\",\"message\":\"" + EscapeJson(entry.message) +
      L"\",\"metadata\":\"" + EscapeJson(entry.metadata) +
      L"\",\"session_id\":\"" + EscapeJson(entry.sessionId) +
      L"\",\"user_id\":\"" + EscapeJson(entry.userId) +
      L"\",\"event_id\":" + std::to_wstring(entry.eventId) +
      L",\"process_id\":" + std::to_wstring(entry.processId) +
      L",\"thread_id\":" + std::to_wstring(entry.threadId) +
      L",\"repeat_count\":" + std::to_wstring(entry.repeatCount) +
      L"}";
    pendingJsonLogs_.push_back(std::move(json));
  }
}

void MonixApp::SetToast(const std::wstring& message) {
  state_.toastMessage = message;
  state_.toastUntilMs = GetTickCount64() + 2400;
}

void MonixApp::AppendHistoryPoint(const Snapshot& snapshot) {
  g_phase = "APPEND:ENTER";
  if (!config_.analyticsHistoryEnabled) {
    return;
  }
  g_phase = "APPEND:CHECK_DONE";

  const double ramPct = snapshot.ramTotalBytes == 0 ? 0.0 :
    (static_cast<double>(snapshot.ramUsedBytes) / static_cast<double>(snapshot.ramTotalBytes)) * 100.0;
  const double netUpMb = std::min(100.0, static_cast<double>(snapshot.netUpBytesPerSec) / (1024.0 * 1024.0));
  const double netDownMb = std::min(100.0, static_cast<double>(snapshot.netDownBytesPerSec) / (1024.0 * 1024.0));

  AppendBounded(state_.cpuHistory, snapshot.cpuPct, config_.historyCapacity);
  AppendBounded(state_.ramHistory, ramPct, config_.historyCapacity);
  AppendBounded(state_.gpuHistory, snapshot.gpuPct, config_.historyCapacity);
  AppendBounded(state_.netUploadHistory, netUpMb, config_.historyCapacity);
  AppendBounded(state_.netHistory, netDownMb, config_.historyCapacity);
  AppendBounded(state_.latencyHistory, static_cast<double>(snapshot.latencyMs), config_.historyCapacity);
}

void MonixApp::ExportLogsJson() const {
  std::filesystem::create_directories(paths_.exportsDir);
  const auto path = paths_.exportsDir / (state_.sessionId + L"-logs.json");
  std::wofstream output(path);
  output << L"[\n";
  for (std::size_t i = 0; i < state_.logs.size(); ++i) {
    const auto& entry = state_.logs[i];
    output << L"  {\"timestamp\":\"" << EscapeJson(entry.fullTimestamp)
           << L"\",\"level\":\"" << EscapeJson(entry.severity)
           << L"\",\"domain\":\"" << EscapeJson(entry.domain)
           << L"\",\"module\":\"" << EscapeJson(entry.module)
           << L"\",\"service\":\"" << EscapeJson(entry.service)
           << L"\",\"message\":\"" << EscapeJson(entry.message)
           << L"\",\"metadata\":\"" << EscapeJson(entry.metadata)
           << L"\",\"event_id\":" << entry.eventId
           << L",\"repeat_count\":" << entry.repeatCount
           << L"}" << (i + 1 == state_.logs.size() ? L"\n" : L",\n");
  }
  output << L"]\n";
}

void MonixApp::ExportLogsCsv() const {
  std::filesystem::create_directories(paths_.exportsDir);
  const auto path = paths_.exportsDir / (state_.sessionId + L"-logs.csv");
  std::wofstream output(path);
  output << L"timestamp,level,domain,module,service,event_id,repeat_count,message,metadata\n";
  for (const auto& entry : state_.logs) {
    std::wstring message = entry.message;
    std::replace(message.begin(), message.end(), L'"', L'\'');
    std::wstring metadata = entry.metadata;
    std::replace(metadata.begin(), metadata.end(), L'"', L'\'');
    output << L"\"" << entry.fullTimestamp << L"\","
           << L"\"" << entry.severity << L"\","
           << L"\"" << entry.domain << L"\","
           << L"\"" << entry.module << L"\","
           << L"\"" << entry.service << L"\","
           << entry.eventId << L","
           << entry.repeatCount << L","
           << L"\"" << message << L"\","
           << L"\"" << metadata << L"\"\n";
  }
}

void MonixApp::UpdateScramSummary(const Snapshot& snapshot) {
  std::wstring headline;
  std::wstring insight;
  std::vector<std::wstring> diagnostics;
  int risk = 8;

  diagnostics.push_back(L"Kernel queue: " + std::to_wstring(snapshot.processorQueueLength) + L" | switches: " + std::to_wstring(snapshot.contextSwitchesPerSec) + L"/s");
  diagnostics.push_back(L"Thermals: CPU " + FormatTemperature(snapshot.cpuTempC, snapshot.cpuTempEstimated) + L" | GPU " + FormatTemperature(snapshot.gpuTempC, snapshot.gpuTempEstimated));
  diagnostics.push_back(L"Storage: " + FormatTemperature(snapshot.storageTempC, snapshot.storageTempEstimated) + L" | pagefile " + FormatBytes(snapshot.pageFileUsedBytes));
  diagnostics.push_back(L"Live inventory: " + std::to_wstring(snapshot.processCount) + L" proc | " + std::to_wstring(snapshot.threadCount) + L" threads | " + std::to_wstring(snapshot.handleCount) + L" handles");

  if (snapshot.cpuPct >= 85.0 || snapshot.processorQueueLength >= 4) {
    headline = L"CPU scheduling pressure detected.";
    insight = L"Foreground activity is saturating execution slices or queue depth.";
    diagnostics.push_back(L"Root cause candidate: scheduler back-pressure and elevated interrupt activity.");
    risk += 26;
  }

  if (snapshot.gpuPct >= 85.0) {
    headline = L"GPU pipeline is operating near saturation.";
    insight = L"Likely render, shader, game scene or capture workload active.";
    diagnostics.push_back(L"Render path sustained above 85% utilization.");
    risk += 20;
  }

  if (snapshot.ramTotalBytes > 0) {
    const double ramPct = (static_cast<double>(snapshot.ramUsedBytes) / static_cast<double>(snapshot.ramTotalBytes)) * 100.0;
    const int ramUsedMB = static_cast<int>(snapshot.ramUsedBytes / (1024ull * 1024ull));
    const int ramTotalMB = static_cast<int>(snapshot.ramTotalBytes / (1024ull * 1024ull));
    const std::wstring ramDetail = FormatPercent(ramPct) + L" (" + std::to_wstring(ramUsedMB) + L"/" + std::to_wstring(ramTotalMB) + L" MB)";
    const bool wasRamPressure = state_.scramHeadline.find(L"RAM ") == 0;
    if (ramPct >= 95.0) {
      headline = L"RAM exhausted at " + ramDetail;
      insight = L"System at imminent risk of OOM kills and stall.";
      diagnostics.push_back(L"RAM exhaustion zone. Immediate intervention required.");
      risk += 24;
    } else if (ramPct >= 90.0) {
      headline = L"RAM critical at " + ramDetail;
      insight = L"Resident sets exceeding safe operating limits.";
      diagnostics.push_back(L"RAM in critical zone. Working sets must be reduced.");
      risk += 20;
    } else if (ramPct >= 85.0) {
      headline = L"RAM rising at " + ramDetail;
      insight = L"Resident sets are growing faster than reclaim and cache release.";
      diagnostics.push_back(L"RAM escalation past 85%. Reclaim rate insufficient.");
      risk += 18;
    } else if (ramPct >= 82.0) {
      headline = L"RAM elevated at " + ramDetail;
      insight = L"Resident sets are growing faster than reclaim and cache release.";
      diagnostics.push_back(L"RAM occupancy crossed 82% threshold.");
      risk += 15;
    } else if (ramPct < 75.0 && wasRamPressure) {
      headline = L"RAM normalized at " + ramDetail;
      insight = L"RAM usage returned below the 75% stabilization threshold.";
      risk -= 5;
    }
  }

  if (snapshot.cpuTempC >= 78.0 || snapshot.gpuTempC >= 82.0 || snapshot.storageTempC >= 56.0) {
    headline = L"Thermal drift detected on active hardware.";
    insight = L"At least one sensor or estimate crossed the warm operating envelope.";
    diagnostics.push_back(L"Thermal watch is elevated. Review cooling and active tasks.");
    risk += 16;
  }

  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    if (!snapshot.gpuModel.empty() && snapshot.gpuModel != prev.gpuModel) {
      headline = L"GPU model detection: " + snapshot.gpuModel;
      insight = L"GPU hardware identifier has changed.";
      diagnostics.push_back(L"GPU: " + snapshot.gpuModel);
      risk += 6;
    }

    if (!snapshot.gpuDriverVersion.empty() && snapshot.gpuDriverVersion != prev.gpuDriverVersion && !prev.gpuDriverVersion.empty()) {
      headline = L"GPU driver version change detected.";
      insight = L"Display driver has been updated or rolled back.";
      diagnostics.push_back(L"Driver: " + prev.gpuDriverVersion + L" -> " + snapshot.gpuDriverVersion);
      risk += 14;
    }

    if (snapshot.gpuPct > 95.0 && prev.gpuPct < 50.0) {
      headline = L"GPU driver reset suspected.";
      insight = L"GPU utilization spiked from low to maximum, possibly indicating a driver recovery.";
      diagnostics.push_back(L"GPU reset pattern: " + std::to_wstring((int)prev.gpuPct) + L"% -> " + std::to_wstring((int)snapshot.gpuPct) + L"%");
      risk += 16;
    }

    if (snapshot.gpuPct >= 95.0) {
      headline = L"GPU utilization spike detected.";
      insight = L"GPU is at or near maximum utilization.";
      diagnostics.push_back(L"GPU at " + std::to_wstring((int)snapshot.gpuPct) + L"% utilization.");
      risk += 10;
    }

    if (snapshot.gpuVramTotalBytes > 0 && snapshot.gpuVramUsedBytes > 0) {
      double vramPct = static_cast<double>(snapshot.gpuVramUsedBytes) / static_cast<double>(snapshot.gpuVramTotalBytes) * 100.0;
      if (vramPct >= 90.0) {
        headline = L"GPU memory saturation detected.";
        insight = L"VRAM usage is critically high.";
        diagnostics.push_back(L"VRAM: " + std::to_wstring((int)vramPct) + L"% (" + std::to_wstring(snapshot.gpuVramUsedBytes / (1024*1024)) + L"/" + std::to_wstring(snapshot.gpuVramTotalBytes / (1024*1024)) + L" MB)");
        risk += 14;
      }
    }

    if (snapshot.gpuVramUsedBytes > prev.gpuVramUsedBytes + 100ull * 1024ull * 1024ull) {
      const uint64_t growth = snapshot.gpuVramUsedBytes - prev.gpuVramUsedBytes;
      headline = L"VRAM allocation growth detected.";
      insight = L"GPU memory usage increased by over 100 MB in one sample.";
      diagnostics.push_back(L"VRAM grew by " + std::to_wstring(growth / (1024*1024)) + L" MB.");
      risk += 10;
    }

    if (prev.gpuVramUsedBytes > snapshot.gpuVramUsedBytes + 200ull * 1024ull * 1024ull) {
      const uint64_t evicted = prev.gpuVramUsedBytes - snapshot.gpuVramUsedBytes;
      diagnostics.push_back(L"VRAM eviction: freed " + std::to_wstring(evicted / (1024*1024)) + L" MB.");
      risk += 4;
    }

    if (snapshot.gpuPct >= 70.0 && snapshot.frameTimeMs > 33.3) {
      headline = L"Render queue backlog detected.";
      insight = L"GPU utilization is high with elevated frame times.";
      diagnostics.push_back(L"Frame time: " + std::to_wstring((int)snapshot.frameTimeMs) + L"ms at " + std::to_wstring((int)snapshot.gpuPct) + L"% GPU.");
      risk += 10;
    }

    if (snapshot.frameTimeMs > 50.0 && prev.frameTimeMs > 0 && prev.frameTimeMs < 20.0) {
      headline = L"Frame time spike detected.";
      insight = L"Frame time jumped significantly from baseline.";
      diagnostics.push_back(L"Frame time: " + std::to_wstring((int)prev.frameTimeMs) + L"ms -> " + std::to_wstring((int)snapshot.frameTimeMs) + L"ms");
      risk += 10;
    }

    if (prev.frameTimeMs > 0 && snapshot.frameTimeMs <= 0) {
      diagnostics.push_back(L"Frame drop event: frame time reset to zero.");
      risk += 6;
    }

    if (snapshot.displayRefreshRateHz != prev.displayRefreshRateHz && prev.displayRefreshRateHz > 0) {
      headline = L"Display refresh rate change detected.";
      insight = L"Monitor refresh rate has been modified.";
      diagnostics.push_back(L"Refresh: " + std::to_wstring(prev.displayRefreshRateHz) + L"Hz -> " + std::to_wstring(snapshot.displayRefreshRateHz) + L"Hz");
      risk += 14;
    }

    if ((snapshot.displayWidth != prev.displayWidth || snapshot.displayHeight != prev.displayHeight) && prev.displayWidth > 0) {
      headline = L"Resolution change detected.";
      insight = L"Display resolution has been modified.";
      diagnostics.push_back(L"Resolution: " + std::to_wstring(prev.displayWidth) + L"x" + std::to_wstring(prev.displayHeight) + L" -> " + std::to_wstring(snapshot.displayWidth) + L"x" + std::to_wstring(snapshot.displayHeight));
      risk += 14;
    }

    if (snapshot.displayMonitorCount != prev.displayMonitorCount && prev.displayMonitorCount > 0) {
      if (snapshot.displayMonitorCount > prev.displayMonitorCount) {
        headline = L"Display hotplug detected.";
        insight = L"New display has been connected.";
        diagnostics.push_back(L"Monitors: " + std::to_wstring(prev.displayMonitorCount) + L" -> " + std::to_wstring(snapshot.displayMonitorCount));
        risk += 6;
      } else {
        headline = L"Multi-monitor topology change detected.";
        insight = L"Display configuration has changed.";
        diagnostics.push_back(L"Monitors: " + std::to_wstring(prev.displayMonitorCount) + L" -> " + std::to_wstring(snapshot.displayMonitorCount));
        risk += 10;
      }
    }

    if (snapshot.displayBitsPerPel != prev.displayBitsPerPel && prev.displayBitsPerPel > 0) {
      headline = L"Color depth change detected.";
      insight = L"Display color depth has been modified.";
      diagnostics.push_back(L"Color depth: " + std::to_wstring(prev.displayBitsPerPel) + L" -> " + std::to_wstring(snapshot.displayBitsPerPel) + L" bpp");
      risk += 14;
    }

    if (snapshot.hdrEnabled != prev.hdrEnabled && prev.hdrEnabled >= 0) {
      headline = L"HDR state change detected.";
      insight = L"Display HDR mode has been toggled.";
      diagnostics.push_back(L"HDR: " + std::to_wstring(prev.hdrEnabled ? 1 : 0) + L" -> " + std::to_wstring(snapshot.hdrEnabled ? 1 : 0));
      risk += 14;
    }

    if (snapshot.vsyncEnabled != prev.vsyncEnabled && prev.vsyncEnabled >= 0) {
      headline = L"VSync state change detected.";
      insight = L"Vertical sync has been toggled.";
      diagnostics.push_back(L"VSync: " + std::to_wstring(prev.vsyncEnabled ? 1 : 0) + L" -> " + std::to_wstring(snapshot.vsyncEnabled ? 1 : 0));
      risk += 10;
    }

    if (snapshot.gpuTempC > prev.gpuTempC + 15.0 && snapshot.gpuTempC > 60.0) {
      headline = L"GPU temperature rise detected.";
      insight = L"GPU temperature jumped significantly.";
      diagnostics.push_back(L"GPU temp: " + FormatTemperature(prev.gpuTempC, prev.gpuTempEstimated) + L" -> " + FormatTemperature(snapshot.gpuTempC, snapshot.gpuTempEstimated));
      risk += 10;
    }

    if (snapshot.gpuPowerWatts > prev.gpuPowerWatts * 1.5 && snapshot.gpuPowerWatts > 100.0) {
      headline = L"GPU power spike detected.";
      insight = L"GPU power draw increased by over 50%.";
      diagnostics.push_back(L"GPU power: " + std::to_wstring((int)prev.gpuPowerWatts) + L"W -> " + std::to_wstring((int)snapshot.gpuPowerWatts) + L"W");
      risk += 12;
    }

    if (snapshot.desktopCompositionEnabled != prev.desktopCompositionEnabled && prev.desktopCompositionEnabled >= 0) {
      headline = L"Desktop composition change detected.";
      insight = L"DWM composition has been toggled.";
      diagnostics.push_back(L"DWM: " + std::to_wstring(prev.desktopCompositionEnabled ? 1 : 0) + L" -> " + std::to_wstring(snapshot.desktopCompositionEnabled ? 1 : 0));
      risk += 14;
    }

    if (snapshot.gpuFanRpm > 3000 && prev.gpuFanRpm > 0 && snapshot.gpuFanRpm < prev.gpuFanRpm * 0.3) {
      headline = L"GPU fan anomaly detected.";
      insight = L"GPU fan speed dropped significantly while under load.";
      diagnostics.push_back(L"Fan: " + std::to_wstring(prev.gpuFanRpm) + L" -> " + std::to_wstring(snapshot.gpuFanRpm) + L" RPM");
      risk += 12;
    }

    if (snapshot.gpuPct >= 85.0 && prev.gpuPct >= 85.0 && snapshot.gpuTempC > 80.0) {
      headline = L"TDR warning: GPU under sustained high load.";
      insight = L"Prolonged GPU saturation increases TDR risk.";
      diagnostics.push_back(L"TDR risk: GPU at " + std::to_wstring((int)snapshot.gpuPct) + L"% for >1 sample, temp " + FormatTemperature(snapshot.gpuTempC, snapshot.gpuTempEstimated));
      risk += 16;
    }

    if (prev.gpuPct >= 90.0 && snapshot.gpuPct < 30.0) {
      headline = L"TDR recovery detected.";
      insight = L"GPU utilization dropped from saturation to idle, possibly indicating driver recovery.";
      diagnostics.push_back(L"TDR recovery: " + std::to_wstring((int)prev.gpuPct) + L"% -> " + std::to_wstring((int)snapshot.gpuPct) + L"%");
      risk += 14;
    }

    if (snapshot.tdrLevel != prev.tdrLevel && prev.tdrLevel >= 0) {
      headline = L"PCIe link speed change or driver config change.";
      insight = L"Graphics driver configuration has been modified.";
      diagnostics.push_back(L"TDR level: " + std::to_wstring(prev.tdrLevel) + L" -> " + std::to_wstring(snapshot.tdrLevel));
      risk += 10;
    }

    const int threadDelta = snapshot.threadCount - prev.threadCount;
    if (threadDelta > 100) {
      diagnostics.push_back(L"Thread creation burst: +" + std::to_wstring(threadDelta) + L" threads.");
      risk += 8;
    } else if (threadDelta < -100) {
      diagnostics.push_back(L"Thread termination burst: " + std::to_wstring(threadDelta) + L" threads.");
      risk += 6;
    }

    if (snapshot.contextSwitchesPerSec > 50000) {
      headline = L"Context switch spike detected.";
      insight = L"Context switch rate exceeds the nominal baseline.";
      diagnostics.push_back(L"Context switches: " + std::to_wstring(snapshot.contextSwitchesPerSec) + L"/s");
      risk += 12;
    }

    if (snapshot.processorQueueLength >= 6) {
      headline = L"Ready queue buildup detected.";
      insight = L"Processor ready queue depth exceeds safe thresholds.";
      diagnostics.push_back(L"Queue depth: " + std::to_wstring(snapshot.processorQueueLength));
      risk += 14;
    }

    if (snapshot.processorQueueLength >= 4 && snapshot.cpuPct < 40.0) {
      headline = L"CPU starvation event detected.";
      insight = L"High ready queue with low CPU utilization suggests runnable threads are not being scheduled.";
      diagnostics.push_back(L"Starvation pattern: queue " + std::to_wstring(snapshot.processorQueueLength) + L" at " + std::to_wstring((int)snapshot.cpuPct) + L"% CPU");
      risk += 18;
    }

    if (snapshot.threadCreationDelta > 50) {
      diagnostics.push_back(L"Thread creation spike: +" + std::to_wstring(snapshot.threadCreationDelta) + L" this sample.");
      risk += 6;
    }

    if (snapshot.interruptsPerSec > 8000) {
      headline = L"High DPC latency detected.";
      insight = L"Interrupt and DPC activity is elevated, potentially causing scheduling delays.";
      diagnostics.push_back(L"DPC/IRQ pressure: " + std::to_wstring(snapshot.interruptsPerSec) + L" interrupts/s");
      risk += 14;
    }

    if (snapshot.interruptsPerSec > 15000) {
      headline = L"High ISR latency detected.";
      insight = L"ISR activity is extremely elevated, likely causing scheduling delays.";
      diagnostics.push_back(L"ISR burst: " + std::to_wstring(snapshot.interruptsPerSec) + L"/s");
      risk += 16;
    }

    if (snapshot.contextSwitchesPerSec > 30000 && snapshot.cpuPct >= 85.0) {
      headline = L"Scheduling latency spike detected.";
      insight = L"High context switch rate under heavy CPU load increases scheduling latency.";
      diagnostics.push_back(L"Scheduling latency: " + std::to_wstring(snapshot.contextSwitchesPerSec) + L"/s at " + std::to_wstring((int)snapshot.cpuPct) + L"%");
      risk += 12;
    }

    if (snapshot.suspendedThreadCount > prev.suspendedThreadCount + 50) {
      const int suspendDelta = snapshot.suspendedThreadCount - prev.suspendedThreadCount;
      diagnostics.push_back(L"Thread suspension burst: +" + std::to_wstring(suspendDelta) + L" suspended threads.");
      risk += 6;
    }
    if (prev.suspendedThreadCount > snapshot.suspendedThreadCount + 50) {
      const int resumeDelta = prev.suspendedThreadCount - snapshot.suspendedThreadCount;
      diagnostics.push_back(L"Thread resume burst: " + std::to_wstring(resumeDelta) + L" threads resumed.");
      risk += 4;
    }

    if (snapshot.readyThreadCount > 800) {
      headline = L"Wait chain anomaly detected.";
      insight = L"Excessive threads in ready state may indicate lock contention or priority inversion.";
      diagnostics.push_back(L"Ready threads: " + std::to_wstring(snapshot.readyThreadCount) + L" (waiting: " + std::to_wstring(snapshot.waitingThreadCount) + L")");
      risk += 12;
    }

    const int realtimeDelta = snapshot.realtimeThreadCount - prev.realtimeThreadCount;
    if (realtimeDelta > 5) {
      headline = L"Thread priority change detected.";
      insight = L"Abnormal number of real-time priority threads detected.";
      diagnostics.push_back(L"RT threads: " + std::to_wstring(prev.realtimeThreadCount) + L" -> " + std::to_wstring(snapshot.realtimeThreadCount));
      risk += 14;
    }

    if (snapshot.readyThreadCount > 500 && snapshot.cpuPct < 30.0 && snapshot.processorQueueLength >= 3) {
      headline = L"Priority inversion pattern detected.";
      insight = L"Many ready threads with low CPU utilization and deep queue suggests priority inversion.";
      diagnostics.push_back(L"Inversion: " + std::to_wstring(snapshot.readyThreadCount) + L" ready, " + std::to_wstring((int)snapshot.cpuPct) + L"% CPU");
      risk += 16;
    }

    if (snapshot.threadCount > prev.threadCount + 200) {
      headline = L"Core parking event suspected.";
      insight = L"Thread count surge may indicate core unparking behavior.";
      diagnostics.push_back(L"Thread surge: +" + std::to_wstring(snapshot.threadCount - prev.threadCount) + L" threads.");
      risk += 8;
    }

    if (snapshot.highPriorityThreadCount > prev.highPriorityThreadCount + 20) {
      diagnostics.push_back(L"High-priority thread burst: +" + std::to_wstring(snapshot.highPriorityThreadCount - prev.highPriorityThreadCount));
      risk += 6;
    }

    if (snapshot.processorQueueLength >= 2 && snapshot.contextSwitchesPerSec > 20000) {
      diagnostics.push_back(L"Lock contention spike: queue " + std::to_wstring(snapshot.processorQueueLength) + L" with " + std::to_wstring(snapshot.contextSwitchesPerSec) + L" context switches.");
      risk += 10;
    }

    for (const auto& addr : snapshot.netAdapterAddresses) {
      if (prev.netAdapterAddresses.find(addr) == prev.netAdapterAddresses.end()) {
        diagnostics.push_back(L"IP address change detected: new " + addr);
        risk += 14;
      }
    }
    for (const auto& addr : prev.netAdapterAddresses) {
      if (snapshot.netAdapterAddresses.find(addr) == snapshot.netAdapterAddresses.end()) {
        diagnostics.push_back(L"IP address removed: " + addr);
        risk += 10;
      }
    }
    for (const auto& gw : snapshot.netAdapterGateways) {
      if (prev.netAdapterGateways.find(gw) == prev.netAdapterGateways.end()) {
        diagnostics.push_back(L"Gateway change detected: " + gw);
        risk += 12;
      }
    }

    if (prev.netAdapterOperStatuses.size() > 0 && snapshot.netAdapterOperStatuses.size() > 0) {
      for (const auto& status : snapshot.netAdapterOperStatuses) {
        if (status == 1 && prev.netAdapterOperStatuses.find(1) == prev.netAdapterOperStatuses.end() && prev.netAdapterOperStatuses.size() > 0) {
          headline = L"Network link up event detected.";
          insight = L"A network adapter has transitioned to operational state.";
          diagnostics.push_back(L"Link up: adapter became operational.");
          risk += 6;
        }
      }
      for (const auto& status : prev.netAdapterOperStatuses) {
        if (status == 1 && snapshot.netAdapterOperStatuses.find(1) == snapshot.netAdapterOperStatuses.end() && snapshot.netAdapterOperStatuses.size() > 0) {
          headline = L"Network link down event detected.";
          insight = L"A network adapter has gone offline.";
          diagnostics.push_back(L"Link down: adapter lost operational status.");
          risk += 14;
        }
      }
    }

    for (const auto& speed : snapshot.netAdapterSpeeds) {
      if (speed > 0 && prev.netAdapterSpeeds.find(speed) == prev.netAdapterSpeeds.end() && prev.netAdapterSpeeds.size() > 0) {
        bool isWifiType = false;
        for (const auto& t : snapshot.netAdapterTypes) {
          if (t == IF_TYPE_IEEE80211) { isWifiType = true; break; }
        }
        if (isWifiType && speed < 72000000) {
          headline = L"Wi-Fi signal degradation detected.";
          insight = L"Wireless adapter speed dropped below nominal.";
          diagnostics.push_back(L"Wi-Fi speed: " + std::to_wstring(speed / 1000000) + L" Mbps (degraded).");
          risk += 10;
        } else {
          diagnostics.push_back(L"Ethernet speed renegotiated to " + std::to_wstring(speed / 1000000) + L" Mbps.");
          risk += 4;
        }
      }
    }

    if (snapshot.netAdapterOperStatuses.size() == 0 && prev.netAdapterOperStatuses.size() > 0 &&
        snapshot.netAdapterOperStatuses.size() > 0) {
      diagnostics.push_back(L"Network interface reset detected: adapters bounced.");
      risk += 12;
    }

    if (snapshot.routeTableHash != 0 && prev.routeTableHash != 0 && snapshot.routeTableHash != prev.routeTableHash) {
      headline = L"Route table change detected.";
      insight = L"The IP forwarding table has changed unexpectedly.";
      diagnostics.push_back(L"Route table hash changed: routing may have shifted.");
      risk += 14;
    }

    if (prev.proxyEnabled >= 0 && snapshot.proxyEnabled != prev.proxyEnabled) {
      headline = L"Proxy configuration change detected.";
      insight = L"System proxy settings have been modified.";
      diagnostics.push_back(L"Proxy state changed: " + std::to_wstring(snapshot.proxyEnabled ? 1 : 0));
      risk += 14;
    }

    const int inboundDelta = snapshot.inboundConnections - prev.inboundConnections;
    if (inboundDelta > 20) {
      diagnostics.push_back(L"Inbound connection burst: +" + std::to_wstring(inboundDelta) + L" sockets.");
      risk += 6;
    }
    const int outboundDelta = snapshot.outboundConnections - prev.outboundConnections;
    if (outboundDelta > 30) {
      diagnostics.push_back(L"Outbound connection burst: +" + std::to_wstring(outboundDelta) + L" sockets.");
      risk += 8;
    }
    if (outboundDelta < -30) {
      diagnostics.push_back(L"Connection teardown spike: " + std::to_wstring(outboundDelta) + L" sockets.");
      risk += 4;
    }

    if (snapshot.tcpResets > prev.tcpResets && prev.tcpResets > 0) {
      const uint64_t resetDelta = snapshot.tcpResets - prev.tcpResets;
      if (resetDelta > 50) {
        headline = L"TCP reset spike detected.";
        insight = L"Abnormal number of TCP connections in terminal states.";
        diagnostics.push_back(L"TCP resets: +" + std::to_wstring(resetDelta) + L" in one sample.");
        risk += 12;
      }
    }

    if (snapshot.tcpRetransmits > prev.tcpRetransmits && prev.tcpRetransmits > 0) {
      const uint64_t retransDelta = snapshot.tcpRetransmits - prev.tcpRetransmits;
      if (retransDelta > 40) {
        headline = L"TCP retransmission spike detected.";
        insight = L"High retransmission rate indicates packet loss or congestion.";
        diagnostics.push_back(L"TCP retransmits: +" + std::to_wstring(retransDelta));
        risk += 12;
      }
    }

    if (snapshot.netPrimaryLinkSpeedBps > 0) {
      double utilization = 0.0;
      if (snapshot.netDownBytesPerSec + snapshot.netUpBytesPerSec > 0) {
        utilization = static_cast<double>(snapshot.netDownBytesPerSec + snapshot.netUpBytesPerSec) * 8.0 /
                      static_cast<double>(snapshot.netPrimaryLinkSpeedBps) * 100.0;
      }
      if (utilization >= 85.0) {
        headline = L"Bandwidth saturation detected.";
        insight = L"Aggregate throughput is near the link capacity.";
        diagnostics.push_back(L"Link utilization: " + std::to_wstring(static_cast<int>(utilization)) + L"%");
        risk += 12;
      }
    }

    if (snapshot.pingRttMs >= 0 && prev.pingRttMs >= 0) {
      if (snapshot.pingRttMs - prev.pingRttMs > 50) {
        headline = L"Latency spike detected.";
        insight = L"ICMP probe RTT jumped significantly.";
        diagnostics.push_back(L"Latency spike: " + std::to_wstring(prev.pingRttMs) + L"ms -> " + std::to_wstring(snapshot.pingRttMs) + L"ms");
        risk += 10;
      }
    }

    state_.pingRttSamples.push_back(snapshot.pingRttMs);
    if (state_.pingRttSamples.size() > 20) state_.pingRttSamples.erase(state_.pingRttSamples.begin());
    if (state_.pingRttSamples.size() >= 5) {
      double sum = 0.0;
      for (int v : state_.pingRttSamples) sum += v;
      double mean = sum / state_.pingRttSamples.size();
      double varSum = 0.0;
      for (int v : state_.pingRttSamples) {
        double d = v - mean;
        varSum += d * d;
      }
      state_.pingJitterStddev = std::sqrt(varSum / state_.pingRttSamples.size());
      if (state_.pingJitterStddev > 25.0) {
        headline = L"Jitter spike detected.";
        insight = L"ICMP probe latency variance exceeds the nominal envelope.";
        diagnostics.push_back(L"Jitter stddev: " + std::to_wstring(static_cast<int>(state_.pingJitterStddev)) + L"ms");
        risk += 10;
      }
    }

    if (snapshot.pingRttMs < 0 && prev.pingRttMs >= 0) {
      headline = L"Gateway unreachable detected.";
      insight = L"ICMP probe to default gateway failed.";
      diagnostics.push_back(L"Gateway probe failed (was " + std::to_wstring(prev.pingRttMs) + L"ms).");
      risk += 16;
    }

    if (snapshot.dnsResolutionOk == 0 && prev.dnsResolutionOk == 1) {
      headline = L"DNS resolution failure detected.";
      insight = L"DNS probe to google.com failed.";
      diagnostics.push_back(L"DNS probe returned failure.");
      risk += 16;
    }

    if (snapshot.dnsResolutionMs >= 0 && prev.dnsResolutionMs >= 0) {
      if (snapshot.dnsResolutionMs - prev.dnsResolutionMs > 100) {
        headline = L"DNS latency spike detected.";
        insight = L"DNS resolution time increased significantly.";
         diagnostics.push_back(L"DNS spike: " + std::to_wstring(prev.dnsResolutionMs) + L"ms -> " + std::to_wstring(snapshot.dnsResolutionMs) + L"ms");
        risk += 10;
      }
    }

    if (snapshot.pingRttMs < 0 && prev.pingRttMs >= 0) {
      if (prev.tcpResets > snapshot.tcpResets) {
        headline = L"Packet loss spike detected.";
        insight = L"ICMP probe failed while TCP resets increased.";
        diagnostics.push_back(L"Packet loss: ICMP failed, resets up.");
        risk += 14;
      }
    }

    if (!prev.vpnAdapterDescriptions.empty() && snapshot.vpnAdapterDescriptions.empty()) {
      headline = L"VPN disconnect event detected.";
      insight = L"A VPN or tunnel adapter has been removed.";
      diagnostics.push_back(L"VPN adapter disappeared from interface list.");
      risk += 14;
    } else if (prev.vpnAdapterDescriptions.empty() && !snapshot.vpnAdapterDescriptions.empty()) {
      headline = L"VPN connect event detected.";
      insight = L"A VPN or tunnel adapter has been activated.";
      diagnostics.push_back(L"New VPN adapter detected in interface list.");
      risk += 6;
    }

    const int udpDelta = snapshot.udpConnectionCount - prev.udpConnectionCount;
    if (udpDelta < -40) {
      headline = L"UDP loss spike detected.";
      insight = L"Significant drop in active UDP connections.";
      diagnostics.push_back(L"UDP connections dropped by " + std::to_wstring(-udpDelta) + L".");
      risk += 10;
    }

    if (snapshot.outboundConnections < prev.outboundConnections * 0.5 && prev.outboundConnections > 20) {
      headline = L"Firewall drop event detected.";
      insight = L"Established connections dropped sharply without local action.";
      diagnostics.push_back(L"Connection collapse: " + std::to_wstring(prev.outboundConnections) + L" -> " + std::to_wstring(snapshot.outboundConnections));
      risk += 18;
    }

    std::set<std::wstring> currentRemotePorts;
    for (const auto& flow : snapshot.flows) {
      if (flow.state == L"ACTIVE") {
        auto colon = flow.remote.find(L':');
        if (colon != std::wstring::npos) {
          currentRemotePorts.insert(flow.remote.substr(0, colon));
        }
      }
    }
    if (currentRemotePorts.size() > 8) {
      diagnostics.push_back(L"Port scan pattern: " + std::to_wstring(currentRemotePorts.size()) + L" unique remote endpoints.");
      risk += 8;
    }
  }

  if (snapshot.latencyMs >= 45 || snapshot.outboundConnections >= 180) {
    diagnostics.push_back(L"Network latency or socket fan-out is above the nominal baseline.");
    risk += 10;
  }

  if (state_.hasPreviousSnapshot) {
    const int handleDelta = snapshot.handleCount - state_.previousSnapshot->handleCount;
    if (handleDelta > 10000) {
      headline = L"Handle surge detected: +" + std::to_wstring(handleDelta) + L" handles";
      insight = L"Rapid handle allocation may indicate a resource leak.";
      diagnostics.push_back(L"Handle count jumped by " + std::to_wstring(handleDelta) + L" in one sample.");
      risk += 12;
    } else if (handleDelta < -10000) {
      diagnostics.push_back(L"Handle count dropped by " + std::to_wstring(-handleDelta) + L" (mass release).");
      risk += 4;
    }
  }

  if (state_.hasPreviousSnapshot && cpuBaselineCaptured_) {
    const auto& prev = *state_.previousSnapshot;

    if (snapshot.cpuFamily != prev.cpuFamily || snapshot.cpuModel != prev.cpuModel || snapshot.cpuStepping != prev.cpuStepping) {
      headline = L"CPU model/stepping change detected.";
      insight = L"Processor identification changed between samples.";
      diagnostics.push_back(L"CPUID change: family " + std::to_wstring(prev.cpuFamily) + L"->" + std::to_wstring(snapshot.cpuFamily)
        + L" model " + std::to_wstring(prev.cpuModel) + L"->" + std::to_wstring(snapshot.cpuModel)
        + L" step " + std::to_wstring(prev.cpuStepping) + L"->" + std::to_wstring(snapshot.cpuStepping));
      risk += 30;
    }

    if (snapshot.cpuCores != prev.cpuCores || snapshot.cpuLogicalCpus != prev.cpuLogicalCpus) {
      headline = L"Core/thread count mismatch.";
      insight = L"CPU topology changed unexpectedly.";
      diagnostics.push_back(L"Cores: " + std::to_wstring(prev.cpuCores) + L"->" + std::to_wstring(snapshot.cpuCores)
        + L" | Logical: " + std::to_wstring(prev.cpuLogicalCpus) + L"->" + std::to_wstring(snapshot.cpuLogicalCpus));
      risk += 30;
    }

    if (snapshot.cpuHtEnabled != prev.cpuHtEnabled) {
      headline = L"Hyper-Threading / SMT state changed.";
      insight = L"SMT configuration altered.";
      diagnostics.push_back(L"HTT: " + std::wstring(prev.cpuHtEnabled ? L"enabled" : L"disabled")
        + L" -> " + std::wstring(snapshot.cpuHtEnabled ? L"enabled" : L"disabled"));
      risk += 25;
    }

    if (snapshot.cpuMicrocodeRev != prev.cpuMicrocodeRev) {
      headline = L"Microcode version change detected.";
      insight = L"CPU microcode was updated between samples.";
      diagnostics.push_back(L"Microcode: " + std::to_wstring(prev.cpuMicrocodeRev) + L" -> " + std::to_wstring(snapshot.cpuMicrocodeRev));
      risk += 25;
    }

    if (cpuBaseTscPerSec_ > 0 && snapshot.estimatedFrequencyMhz > 0) {
      const double baseMhz = (double)cpuBaseTscPerSec_ / 1000000.0;
      const double drift = std::abs(snapshot.estimatedFrequencyMhz - baseMhz) / baseMhz * 100.0;
      if (drift > 5.0) {
        headline = L"Base clock drift detected at " + std::to_wstring((int)drift) + L"%";
        insight = L"TSC timing relationship has shifted.";
        diagnostics.push_back(L"Estimated clock: " + std::to_wstring((int)snapshot.estimatedFrequencyMhz)
          + L" MHz vs base " + std::to_wstring((int)baseMhz) + L" MHz");
        risk += 10;
      }
    }

    if (snapshot.estimatedFrequencyMhz > 0 && cpuBaseTscPerSec_ > 0) {
      const double baseMhz = (double)cpuBaseTscPerSec_ / 1000000.0;
      if (snapshot.estimatedFrequencyMhz > baseMhz * 1.15) {
        diagnostics.push_back(L"Turbo boost active: " + std::to_wstring((int)snapshot.estimatedFrequencyMhz)
          + L" MHz (base " + std::to_wstring((int)baseMhz) + L" MHz)");
      }
    }

    if (snapshot.cpuPct >= 95.0 && !snapshot.cpuTempEstimated && snapshot.cpuTempC >= 80.0) {
      headline = L"Thermal throttling detected.";
      insight = L"CPU temperature high under sustained load.";
      diagnostics.push_back(L"CPU at " + std::to_wstring((int)snapshot.cpuPct) + L"% with temp "
        + std::to_wstring((int)snapshot.cpuTempC) + L"C.");
      risk += 14;
    }

    if (snapshot.cpuPct >= 90.0 && snapshot.kernelUserRatio > 2.0) {
      headline = L"Power limit throttling (PL1) suspected.";
      insight = L"Kernel time disproportionately high under sustained load.";
      diagnostics.push_back(L"Kernel/user ratio: " + std::to_wstring((int)(snapshot.kernelUserRatio * 100)) + L"% at "
        + std::to_wstring((int)snapshot.cpuPct) + L"% CPU.");
      risk += 10;
    }

    if (snapshot.cpuPct >= 95.0 && snapshot.kernelUserRatio > 3.0) {
      headline = L"Power limit throttling (PL2) suspected.";
      insight = L"Extreme kernel time ratio indicates power budget exceeded.";
      diagnostics.push_back(L"PL2 indicator: kernel ratio " + std::to_wstring((int)(snapshot.kernelUserRatio * 100))
        + L"% at peak load.");
      risk += 12;
    }

    if (snapshot.cpuPct >= 85.0 && snapshot.kernelUserRatio > 1.5) {
      diagnostics.push_back(L"Current limit throttling indicator: elevated kernel time at high utilization.");
      risk += 6;
    }

    if (snapshot.cpuTempC > 85.0 && !snapshot.cpuTempEstimated) {
      headline = L"Package power excursion detected.";
      insight = L"CPU temperature exceeded safe operating envelope.";
      diagnostics.push_back(L"Package temp " + std::to_wstring((int)snapshot.cpuTempC) + L"C at "
        + std::to_wstring((int)snapshot.cpuPct) + L"% load.");
      risk += 14;
    }

    if (snapshot.ipcEstimate > 0 && prev.ipcEstimate > 0) {
      const double ipcDelta = snapshot.ipcEstimate - prev.ipcEstimate;
      if (std::abs(ipcDelta) > 0.15) {
        diagnostics.push_back(L"Instruction retirement rate anomaly: IPC shift "
          + std::to_wstring((int)(ipcDelta * 100)) + L"%");
        risk += 6;
      }
    }

    if (snapshot.cpuFeaturesEdx != prev.cpuFeaturesEdx || snapshot.cpuExtFeatures != prev.cpuExtFeatures) {
      headline = L"CPU feature flags changed.";
      insight = L"Processor feature set altered between samples.";
      diagnostics.push_back(L"Features EDX: 0x" + std::to_wstring(snapshot.cpuFeaturesEdx)
        + L" ext: 0x" + std::to_wstring(snapshot.cpuExtFeatures));
      risk += 20;
    }
  }

  if (snapshot.cpuPct >= 95.0) {
    diagnostics.push_back(L"CPU utilization saturated at " + std::to_wstring((int)snapshot.cpuPct) + L"%.");
    risk += 8;
  }

  if (state_.hasPreviousSnapshot) {
    const double prevKuRatio = state_.previousSnapshot->kernelUserRatio;
    if (snapshot.kernelUserRatio > 2.5 && prevKuRatio < 1.5) {
      headline = L"Kernel/user time imbalance detected.";
      insight = L"Kernel time disproportionately increased.";
      diagnostics.push_back(L"Kernel/user ratio surged from " + std::to_wstring((int)(prevKuRatio * 100))
        + L"% to " + std::to_wstring((int)(snapshot.kernelUserRatio * 100)) + L"%");
      risk += 10;
    }
  }

  if (snapshot.cpuPct >= 70.0 && snapshot.interruptsPerSec > 500) {
    diagnostics.push_back(L"Interrupt load on CPU: " + std::to_wstring(snapshot.interruptsPerSec) + L"/s at "
      + std::to_wstring((int)snapshot.cpuPct) + L"% utilization.");
    risk += 6;
  }

  if (state_.hasPreviousSnapshot && cpuBaselineCaptured_) {
    bool prevVmx = (state_.previousSnapshot->cpuFeaturesEcx & (1 << 5)) != 0;
    bool currVmx = (snapshot.cpuFeaturesEcx & (1 << 5)) != 0;
    if (prevVmx != currVmx) {
      diagnostics.push_back(L"Virtualization instruction activity changed.");
      risk += 4;
    }
  }

  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    if (prev.ramTotalBytes > 0 && snapshot.ramTotalBytes != prev.ramTotalBytes) {
      headline = L"Physical memory topology changed.";
      insight = L"Total physical memory differs between samples.";
      diagnostics.push_back(L"RAM total: " + std::to_wstring(prev.ramTotalBytes / (1024*1024))
        + L" -> " + std::to_wstring(snapshot.ramTotalBytes / (1024*1024)) + L" MB");
      risk += 30;
    }

    if (snapshot.ramAvailBytes > 0 && snapshot.ramAvailBytes < snapshot.ramTotalBytes * 0.05) {
      headline = L"Available memory critically depleted.";
      insight = L"Less than 5% of physical RAM available.";
      diagnostics.push_back(L"Available: " + std::to_wstring(snapshot.ramAvailBytes / (1024*1024))
        + L" MB / " + std::to_wstring(snapshot.ramTotalBytes / (1024*1024)) + L" MB");
      risk += 22;
    }

    const int64_t commitDelta = (int64_t)snapshot.commitUsedBytes - (int64_t)prev.commitUsedBytes;
    if (commitDelta > 512 * 1024 * 1024) {
      headline = L"Commit charge surge detected.";
      insight = L"Committed memory increased rapidly.";
      diagnostics.push_back(L"Commit: +" + std::to_wstring(commitDelta / (1024*1024)) + L" MB ("
        + std::to_wstring(snapshot.commitUsedBytes / (1024*1024)) + L"/"
        + std::to_wstring(snapshot.commitLimitBytes / (1024*1024)) + L" MB)");
      risk += 12;
    }

    if (snapshot.commitPressurePct > 90.0) {
      headline = L"Commit limit pressure at " + std::to_wstring((int)snapshot.commitPressurePct) + L"%";
      insight = L"Virtual address space approaching the commit limit.";
      diagnostics.push_back(L"Commit: " + std::to_wstring(snapshot.commitUsedBytes / (1024*1024))
        + L" / " + std::to_wstring(snapshot.commitLimitBytes / (1024*1024)) + L" MB");
      risk += 14;
    } else if (snapshot.commitPressurePct > 80.0) {
      diagnostics.push_back(L"Commit pressure elevated: " + std::to_wstring((int)snapshot.commitPressurePct) + L"%");
      risk += 6;
    }

    if (prev.workingSetTotalBytes > 0) {
      const int64_t wsDelta = (int64_t)snapshot.workingSetTotalBytes - (int64_t)prev.workingSetTotalBytes;
      if (wsDelta > 256 * 1024 * 1024) {
        headline = L"System working set growth detected.";
        insight = L"Working set expanding beyond reclaim capacity.";
        diagnostics.push_back(L"Working set: +" + std::to_wstring(wsDelta / (1024*1024)) + L" MB ("
          + std::to_wstring(snapshot.workingSetTotalBytes / (1024*1024)) + L" MB)");
        risk += 10;
      } else if (wsDelta < -256 * 1024 * 1024) {
        diagnostics.push_back(L"Working set trimmed: " + std::to_wstring(-wsDelta / (1024*1024)) + L" MB reclaimed");
        risk += 4;
      }
    }

    if (snapshot.pageFaultsDelta > 5000) {
      headline = L"Page fault rate spike: " + std::to_wstring(snapshot.pageFaultsDelta) + L" faults";
      insight = L"Excessive paging activity degrades performance.";
      diagnostics.push_back(L"Page faults: " + std::to_wstring(snapshot.pageFaultsDelta) + L" per sample");
      risk += 14;
    } else if (snapshot.pageFaultsDelta > 2000) {
      diagnostics.push_back(L"Elevated page faults: " + std::to_wstring(snapshot.pageFaultsDelta));
      risk += 6;
    }

    if (snapshot.hardPageFaultsDelta > 100) {
      headline = L"Hard page fault spike: " + std::to_wstring(snapshot.hardPageFaultsDelta) + L" faults";
      insight = L"Disk-backed page I/O causing severe latency.";
      risk += 18;
    }

    if (snapshot.modifiedListBytes > 128 * 1024 * 1024) {
      diagnostics.push_back(L"Modified page list growing: " + std::to_wstring(snapshot.modifiedListBytes / (1024*1024)) + L" MB pending writeback");
      risk += 8;
    }

    const uint64_t fragBytes = snapshot.freeListBytes + snapshot.zeroListBytes;
    if (prev.freeListBytes > 0) {
      const uint64_t prevFrag = prev.freeListBytes + prev.zeroListBytes;
      if (fragBytes > prevFrag * 1.5 && fragBytes > 64 * 1024 * 1024) {
        diagnostics.push_back(L"Memory fragmentation growing: " + std::to_wstring(fragBytes / (1024*1024)) + L" MB free/zero");
        risk += 8;
      }
    }

    if (snapshot.pagefilePctUsed > 90.0) {
      headline = L"Pagefile saturation at " + std::to_wstring((int)snapshot.pagefilePctUsed) + L"%";
      insight = L"Swap space nearly exhausted.";
      diagnostics.push_back(L"Pagefile: " + std::to_wstring(snapshot.pageFileUsedBytes / (1024*1024))
        + L" / " + std::to_wstring(snapshot.pageFileTotalBytes / (1024*1024)) + L" MB");
      risk += 16;
    } else if (snapshot.pagefilePctUsed > 75.0) {
      diagnostics.push_back(L"Pagefile usage elevated: " + std::to_wstring((int)snapshot.pagefilePctUsed) + L"%");
      risk += 6;
    }

    const int64_t pfDelta = (int64_t)snapshot.pageFileUsedBytes - (int64_t)prev.pageFileUsedBytes;
    if (pfDelta > 256 * 1024 * 1024) {
      diagnostics.push_back(L"Pagefile growth: +" + std::to_wstring(pfDelta / (1024*1024)) + L" MB");
      risk += 8;
    }

    if (snapshot.standbyListBytes > 0 && snapshot.standbyListBytes < 64 * 1024 * 1024) {
      diagnostics.push_back(L"Standby list depleted: " + std::to_wstring(snapshot.standbyListBytes / (1024*1024)) + L" MB");
      risk += 10;
    }

    if (prev.kernelPoolNonpagedBytes > 0) {
      const int64_t poolDelta = (int64_t)snapshot.kernelPoolNonpagedBytes - (int64_t)prev.kernelPoolNonpagedBytes;
      if (poolDelta > 16 * 1024 * 1024) {
        headline = L"Nonpaged pool growth: +" + std::to_wstring(poolDelta / (1024*1024)) + L" MB";
        insight = L"Kernel nonpaged pool may be leaking.";
        diagnostics.push_back(L"Nonpaged pool: " + std::to_wstring(snapshot.kernelPoolNonpagedBytes / (1024*1024)) + L" MB");
        risk += 12;
      }
    }

    if (prev.kernelPoolPagedBytes > 0) {
      const int64_t poolDelta = (int64_t)snapshot.kernelPoolPagedBytes - (int64_t)prev.kernelPoolPagedBytes;
      if (poolDelta > 32 * 1024 * 1024) {
        diagnostics.push_back(L"Paged pool growth: +" + std::to_wstring(poolDelta / (1024*1024)) + L" MB ("
          + std::to_wstring(snapshot.kernelPoolPagedBytes / (1024*1024)) + L" MB)");
        risk += 8;
      }
    }

    if (!snapshot.processes.empty() && !prev.processes.empty()) {
      uint64_t currentTotalWs = 0;
      uint64_t prevTotalWs = 0;
      for (const auto& p : snapshot.processes) currentTotalWs += p.ramBytes;
      for (const auto& p : prev.processes) prevTotalWs += p.ramBytes;
      if (currentTotalWs > prevTotalWs * 1.1 && currentTotalWs - prevTotalWs > 512 * 1024 * 1024) {
        diagnostics.push_back(L"Aggregate working set surge: +"
          + std::to_wstring((currentTotalWs - prevTotalWs) / (1024*1024)) + L" MB across all processes");
        risk += 10;
      }
    }

    if (snapshot.ramAvailBytes > 0 && snapshot.commitPressurePct > 85.0
        && snapshot.pageFaultsDelta < 500 && snapshot.cpuPct < 50.0) {
      diagnostics.push_back(L"Memory pressure recovery: avail " + std::to_wstring(snapshot.ramAvailBytes / (1024*1024))
        + L" MB, faults normalized");
      risk -= 4;
    }
  }

  if (snapshot.diskPctUsed > 95.0) {
    headline = L"Disk capacity critical at " + std::to_wstring((int)snapshot.diskPctUsed) + L"%";
    insight = L"Storage volume nearly full.";
    diagnostics.push_back(L"Disk: " + std::to_wstring(snapshot.diskFreeBytes / (1024*1024))
      + L" MB free / " + std::to_wstring(snapshot.diskTotalBytes / (1024*1024)) + L" MB");
    risk += 16;
  } else if (snapshot.diskPctUsed > 90.0) {
    diagnostics.push_back(L"Disk capacity elevated: " + std::to_wstring((int)snapshot.diskPctUsed) + L"% used");
    risk += 8;
  }

  if (snapshot.diskQueueLength > 4.0) {
    headline = L"Disk queue depth spike: " + std::to_wstring((int)snapshot.diskQueueLength);
    insight = L"I/O requests are queuing significantly.";
    diagnostics.push_back(L"Queue depth " + std::to_wstring((int)snapshot.diskQueueLength)
      + L" | read " + std::to_wstring((int)snapshot.diskReadLatencyMs) + L"ms"
      + L" | write " + std::to_wstring((int)snapshot.diskWriteLatencyMs) + L"ms");
    risk += 10;
  } else if (snapshot.diskQueueLength > 2.0) {
    diagnostics.push_back(L"Disk queue depth elevated: " + std::to_wstring((int)snapshot.diskQueueLength));
    risk += 4;
  }

  if (snapshot.diskReadLatencyMs > 20.0) {
    headline = L"Read latency spike: " + std::to_wstring((int)snapshot.diskReadLatencyMs) + L"ms";
    insight = L"Disk read operations are severely delayed.";
    risk += 10;
  } else if (snapshot.diskReadLatencyMs > 10.0) {
    diagnostics.push_back(L"Read latency elevated: " + std::to_wstring((int)snapshot.diskReadLatencyMs) + L"ms");
    risk += 4;
  }

  if (snapshot.diskWriteLatencyMs > 20.0) {
    headline = L"Write latency spike: " + std::to_wstring((int)snapshot.diskWriteLatencyMs) + L"ms";
    insight = L"Disk write operations are severely delayed.";
    risk += 10;
  } else if (snapshot.diskWriteLatencyMs > 10.0) {
    diagnostics.push_back(L"Write latency elevated: " + std::to_wstring((int)snapshot.diskWriteLatencyMs) + L"ms");
    risk += 4;
  }

  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;
    const uint64_t totalIops = snapshot.diskReadIops + snapshot.diskWriteIops;
    const uint64_t prevTotalIops = prev.previousDiskReadIops + prev.previousDiskWriteIops;

    if (totalIops > 5000 && snapshot.cpuPct < 30.0) {
      diagnostics.push_back(L"Random I/O burst: " + std::to_wstring(totalIops) + L" IOPS");
      risk += 6;
    }

    const uint64_t totalBytesIo = snapshot.diskReadBytesPerSec + snapshot.diskWriteBytesPerSec;
    if (totalBytesIo > 200 * 1024 * 1024) {
      diagnostics.push_back(L"Sequential I/O burst: " + std::to_wstring(totalBytesIo / (1024*1024)) + L" MB/s");
      risk += 6;
    }

    if (totalIops > 10000) {
      headline = L"IOPS saturation at " + std::to_wstring(totalIops);
      insight = L"Disk IOPS at maximum sustainable rate.";
      risk += 12;
    }

    if (snapshot.diskTotalBytes != prev.diskTotalBytes && prev.diskTotalBytes > 0) {
      headline = L"Storage device topology changed.";
      diagnostics.push_back(L"Disk size: " + std::to_wstring(prev.diskTotalBytes / (1024*1024))
        + L" -> " + std::to_wstring(snapshot.diskTotalBytes / (1024*1024)) + L" MB");
      risk += 10;
    }

    if (snapshot.nvmeTempValid && snapshot.nvmeTempC > 70.0) {
      headline = L"NVMe temperature rise: " + std::to_wstring((int)snapshot.nvmeTempC) + L"C";
      insight = L"NVMe drive running hot.";
      risk += 10;
    }

    if (!snapshot.smartHealthOk) {
      headline = L"SSD health degradation detected.";
      insight = L"SMART reports imminent failure.";
      risk += 20;
    }

    const int64_t writeDelta = (int64_t)snapshot.diskWriteBytesPerSec - (int64_t)prev.previousDiskWriteBytesPerSec;
    if (writeDelta > 100 * 1024 * 1024) {
      diagnostics.push_back(L"Write throughput surge: +"
        + std::to_wstring(writeDelta / (1024*1024)) + L" MB/s");
      risk += 6;
    }

    const int64_t readDelta = (int64_t)snapshot.diskReadBytesPerSec - (int64_t)prev.previousDiskReadBytesPerSec;
    if (readDelta > 100 * 1024 * 1024) {
      diagnostics.push_back(L"Read throughput surge: +"
        + std::to_wstring(readDelta / (1024*1024)) + L" MB/s");
      risk += 6;
    }

    if (totalIops > prevTotalIops * 2 && totalIops > 3000) {
      diagnostics.push_back(L"Filesystem flush storm: IOPS " + std::to_wstring(prevTotalIops)
        + L" -> " + std::to_wstring(totalIops));
      risk += 8;
    }

    if (snapshot.diskWriteLatencyMs > 15.0 && prev.previousDiskWriteBytesPerSec < snapshot.diskWriteBytesPerSec * 2) {
      diagnostics.push_back(L"Cache flush latency: write "
        + std::to_wstring((int)snapshot.diskWriteLatencyMs) + L"ms at "
        + std::to_wstring(snapshot.diskWriteBytesPerSec / (1024*1024)) + L" MB/s");
      risk += 6;
    }

    if (snapshot.diskWriteBytesPerSec > 0 && snapshot.diskReadBytesPerSec == 0 && snapshot.cpuPct < 20.0) {
      diagnostics.push_back(L"Trim/discard activity suspected: write-only pattern");
      risk += 4;
    }

    if (snapshot.diskQueueLength < 0.1 && prev.diskQueueLength > 1.0) {
      diagnostics.push_back(L"Disk idle after heavy queue: possible sleep/wake cycle");
      risk += 2;
    }
  }

  if (snapshot.smartHealthOk && !state_.hasPreviousSnapshot) {
    diagnostics.push_back(L"SMART status: healthy");
  }

  if (!snapshot.processes.empty()) {
    const auto top = std::max_element(snapshot.processes.begin(), snapshot.processes.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
      const double leftScore = left.cpuPct * 2.0 + (left.ramBytes / 1048576.0) * 0.02 + left.gpuPct * 1.5;
      const double rightScore = right.cpuPct * 2.0 + (right.ramBytes / 1048576.0) * 0.02 + right.gpuPct * 1.5;
      return leftScore < rightScore;
    });
    if (top != snapshot.processes.end()) {
      diagnostics.push_back(L"Top triage candidate: " + top->name + L" PID " + std::to_wstring(top->pid) + L" PPID " + std::to_wstring(top->parentPid));
      if (top->cpuPct >= 25.0 || top->ramBytes >= 2ull * 1024ull * 1024ull * 1024ull) {
        risk += 8;
      }
    }
  }

  if (state_.trackedPid != 0) {
    const auto tracked = std::find_if(snapshot.processes.begin(), snapshot.processes.end(), [this](const ProcessInfo& process) {
      return process.pid == state_.trackedPid;
    });
    if (tracked != snapshot.processes.end()) {
      diagnostics.push_back(L"Tracked task: " + tracked->name + L" | CPU " + FormatPercent(tracked->cpuPct) + L" | RAM " + FormatBytes(tracked->ramBytes));
    } else {
      diagnostics.push_back(L"Tracked task has exited or left the live process set.");
    }
  }

  // --- OS & Kernel Events (25 detections) ---
  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    // 1. Kernel boot phase transition
    if (prev.bootPhase != snapshot.bootPhase) {
      const wchar_t* phases[] = { L"early boot", L"driver init", L"running" };
      headline = L"Kernel boot phase transition: " + std::wstring(phases[snapshot.bootPhase]) + L".";
      insight = L"System boot phase has advanced.";
      diagnostics.push_back(L"Boot phase: " + std::wstring(phases[prev.bootPhase]) + L" -> " + std::wstring(phases[snapshot.bootPhase]));
      risk += 8;
    }

    // 2. Driver load event
    for (const auto& drv : snapshot.driverNames) {
      if (prev.driverNames.find(drv) == prev.driverNames.end()) {
        headline = L"Driver load event: " + drv;
        insight = L"A new kernel driver has been loaded into memory.";
        diagnostics.push_back(L"Driver loaded: " + drv);
        risk += 10;
      }
    }

    // 3. Driver unload event
    for (const auto& drv : prev.driverNames) {
      if (snapshot.driverNames.find(drv) == snapshot.driverNames.end()) {
        headline = L"Driver unload event: " + drv;
        insight = L"A kernel driver has been unloaded from memory.";
        diagnostics.push_back(L"Driver unloaded: " + drv);
        risk += 6;
      }
    }

    // 4. Driver count surge (proxy for callback registration)
    if (snapshot.driverCount > prev.driverCount + 3) {
      headline = L"Driver count surge detected.";
      insight = L"Multiple drivers loaded in short interval, possibly kernel callback registration burst.";
      diagnostics.push_back(L"Drivers: " + std::to_wstring(prev.driverCount) + L" -> " + std::to_wstring(snapshot.driverCount));
      risk += 12;
    }

    // 5. Driver count collapse
    if (prev.driverCount > snapshot.driverCount + 5 && prev.driverCount > 0) {
      headline = L"Driver unload burst detected.";
      insight = L"Multiple drivers unloaded simultaneously, possibly kernel callback failure.";
      diagnostics.push_back(L"Driver collapse: " + std::to_wstring(prev.driverCount) + L" -> " + std::to_wstring(snapshot.driverCount));
      risk += 14;
    }

    // 6. Handle object count anomaly (proxy for kernel object creation/deletion)
    if (prev.totalObjects > 0 && snapshot.totalObjects > 0) {
      const long long objDelta = static_cast<long long>(snapshot.totalObjects) - static_cast<long long>(prev.totalObjects);
      if (objDelta > 50000) {
        headline = L"Kernel object creation surge.";
        insight = L"Object count jumped by over 50k in one sample, indicating mass kernel object allocation.";
        diagnostics.push_back(L"Objects: +" + std::to_wstring(objDelta));
        risk += 14;
      } else if (objDelta < -50000) {
        headline = L"Kernel object mass deletion.";
        insight = L"Object count dropped by over 50k, indicating mass kernel object release.";
        diagnostics.push_back(L"Objects: " + std::to_wstring(objDelta));
        risk += 10;
      }
    }

    // 7. Handle count anomaly
    if (prev.totalHandles > 0 && snapshot.totalHandles > 0) {
      const long long hDelta = static_cast<long long>(snapshot.totalHandles) - static_cast<long long>(prev.totalHandles);
      if (hDelta > 30000) {
        headline = L"Handle table surge detected.";
        insight = L"Handle count jumped by over 30k, possibly indicating a handle leak.";
        diagnostics.push_back(L"Handles: +" + std::to_wstring(hDelta));
        risk += 12;
      } else if (hDelta < -30000) {
        diagnostics.push_back(L"Handle mass release: " + std::to_wstring(hDelta));
        risk += 4;
      }
    }

    // 8. Time change detection
    if (prev.systemTime100ns > 0 && snapshot.systemTime100ns > 0) {
      const long long timeDelta = static_cast<long long>(snapshot.systemTime100ns - prev.systemTime100ns);
      const long long expectedDelta = (snapshot.uptimeMs - prev.uptimeMs) * 10000LL;
      const long long drift = timeDelta - expectedDelta;
      if (drift > 100000000LL || drift < -100000000LL) {
        headline = L"System time change detected.";
        insight = L"System clock has been adjusted by more than 10 seconds.";
        diagnostics.push_back(L"Time drift: " + std::to_wstring(drift / 10000) + L" ms");
        risk += 16;
      }
    }

    // 9. Session start event
    if (snapshot.sessionCount > prev.sessionCount && prev.sessionCount > 0) {
      headline = L"Session start event detected.";
      insight = L"New user session has been created on the system.";
      diagnostics.push_back(L"Sessions: " + std::to_wstring(prev.sessionCount) + L" -> " + std::to_wstring(snapshot.sessionCount));
      risk += 8;
    }

    // 10. Session end event
    if (prev.sessionCount > snapshot.sessionCount && snapshot.sessionCount > 0) {
      headline = L"Session end event detected.";
      insight = L"A user session has been terminated.";
      diagnostics.push_back(L"Sessions: " + std::to_wstring(prev.sessionCount) + L" -> " + std::to_wstring(snapshot.sessionCount));
      risk += 6;
    }

    // 11. Critical process termination (session count drop to 0)
    if (prev.sessionCount > 0 && snapshot.sessionCount == 0) {
      headline = L"Critical session collapse detected.";
      insight = L"All user sessions have been terminated, indicating system-level process termination.";
      diagnostics.push_back(L"Session collapse: " + std::to_wstring(prev.sessionCount) + L" -> 0");
      risk += 20;
    }

    // 12. Process count burst (proxy for process notify events)
    const int procDelta = snapshot.processCount - prev.processCount;
    if (procDelta > 50) {
      headline = L"Process creation burst detected.";
      insight = L"Process count increased by over 50 in one sample, indicating mass process creation.";
      diagnostics.push_back(L"Processes: +" + std::to_wstring(procDelta));
      risk += 10;
    } else if (procDelta < -50) {
      headline = L"Process termination burst detected.";
      insight = L"Process count decreased by over 50 in one sample, indicating mass process termination.";
      diagnostics.push_back(L"Processes: " + std::to_wstring(procDelta));
      risk += 8;
    }

    // 13. Thread notify burst
    const int threadDelta = snapshot.threadCount - prev.threadCount;
    if (threadDelta > 200) {
      headline = L"Thread creation burst detected.";
      insight = L"Thread count increased by over 200 in one sample.";
      diagnostics.push_back(L"Threads: +" + std::to_wstring(threadDelta));
      risk += 10;
    } else if (threadDelta < -200) {
      headline = L"Thread termination burst detected.";
      insight = L"Thread count decreased by over 200 in one sample.";
      diagnostics.push_back(L"Threads: " + std::to_wstring(threadDelta));
      risk += 8;
    }

    // 14. Page fault spike (proxy for system call anomaly)
    if (prev.pageFaultsDelta > 0 && snapshot.pageFaultsDelta > 0) {
      const long long pfDelta = static_cast<long long>(snapshot.pageFaultsDelta) - static_cast<long long>(prev.pageFaultsDelta);
      if (pfDelta > 100000) {
        headline = L"System call anomaly detected.";
        insight = L"Page fault count surged by over 100k, indicating unusual memory access patterns.";
        diagnostics.push_back(L"Page faults: +" + std::to_wstring(pfDelta));
        risk += 12;
      }
    }

    // 15. I/O read burst (proxy for image load notify events)
    if (prev.ioReadBytesDelta > 0 && snapshot.ioReadBytesDelta > 0) {
      const long long ioDelta = static_cast<long long>(snapshot.ioReadBytesDelta) - static_cast<long long>(prev.ioReadBytesDelta);
      if (ioDelta > 500 * 1024 * 1024) {
        headline = L"I/O read burst detected.";
        insight = L"Read I/O increased by over 500 MB, possibly indicating large image load or driver read activity.";
        diagnostics.push_back(L"I/O read: +" + std::to_wstring(ioDelta / (1024 * 1024)) + L" MB");
        risk += 10;
      }
    }

    // 16. I/O write burst (proxy for registry callback events)
    if (prev.ioWriteBytesDelta > 0 && snapshot.ioWriteBytesDelta > 0) {
      const long long ioDelta = static_cast<long long>(snapshot.ioWriteBytesDelta) - static_cast<long long>(prev.ioWriteBytesDelta);
      if (ioDelta > 200 * 1024 * 1024) {
        headline = L"I/O write burst detected.";
        insight = L"Write I/O increased by over 200 MB, possibly indicating registry or config write activity.";
        diagnostics.push_back(L"I/O write: +" + std::to_wstring(ioDelta / (1024 * 1024)) + L" MB");
        risk += 10;
      }
    }

    // 17. Interrupt request event spike
    const uint64_t irqDelta = snapshot.totalInterruptCount >= prevTotalInterruptCount_ ? snapshot.totalInterruptCount - prevTotalInterruptCount_ : 0;
    if (irqDelta > 50000) {
      headline = L"Interrupt request event spike.";
      insight = L"Hardware interrupt count surged by over 50k, indicating elevated IRQ activity.";
      diagnostics.push_back(L"IRQ: +" + std::to_wstring(irqDelta));
      risk += 14;
    }

    // 18. DPC event spike
    const uint64_t dpcDelta = snapshot.totalDpcCount >= prevTotalDpcCount_ ? snapshot.totalDpcCount - prevTotalDpcCount_ : 0;
    if (dpcDelta > 30000) {
      headline = L"Deferred procedure call spike.";
      insight = L"DPC count surged by over 30k, indicating high deferred procedure activity.";
      diagnostics.push_back(L"DPC: +" + std::to_wstring(dpcDelta));
      risk += 14;
    }

    // 19. Context switch anomaly
    const uint64_t csDelta = snapshot.totalContextSwitches >= prevTotalContextSwitches_ ? snapshot.totalContextSwitches - prevTotalContextSwitches_ : 0;
    if (csDelta > 200000) {
      headline = L"Context switch anomaly detected.";
      insight = L"Context switch count surged by over 200k, indicating extreme scheduling pressure.";
      diagnostics.push_back(L"CS: +" + std::to_wstring(csDelta));
      risk += 16;
    }

    // 20. Watchdog timeout precursor (high CPU + high CS + high IRQ)
    if (snapshot.cpuPct >= 95.0 && snapshot.contextSwitchesPerSec > 40000 && irqDelta > 20000) {
      headline = L"Watchdog timeout precursor detected.";
      insight = L"System is under extreme load with high CPU, context switches, and interrupts — watchdog may trigger.";
      diagnostics.push_back(L"Watchdog: CPU " + std::to_wstring((int)snapshot.cpuPct) + L"%, CS " + std::to_wstring(snapshot.contextSwitchesPerSec) + L"/s");
      risk += 20;
    }

    // 21. Kernel panic precursor (multiple high-risk indicators)
    if (snapshot.cpuPct >= 98.0 && snapshot.ramUsedBytes > snapshot.ramTotalBytes * 95 / 100 && snapshot.diskQueueLength >= 4.0) {
      headline = L"Kernel panic precursor detected.";
      insight = L"System is critically overloaded: max CPU, exhausted RAM, and deep disk queue.";
      diagnostics.push_back(L"Panic precursor: CPU " + std::to_wstring((int)snapshot.cpuPct) + L"%, RAM " + FormatPercent(snapshot.ramUsedBytes * 100.0 / snapshot.ramTotalBytes) + L", DQ " + std::to_wstring(snapshot.diskQueueLength));
      risk += 20;
    }

    // 22. Bugcheck precursor (high DPC + high ISR + long scheduling delay)
    if (snapshot.readyThreadCount > 500 && snapshot.cpuPct >= 90.0 && snapshot.contextSwitchesPerSec > 50000) {
      headline = L"Bugcheck precursor detected.";
      insight = L"High ready thread count with extreme CPU and context switches may precede a bugcheck.";
      diagnostics.push_back(L"Bugcheck risk: " + std::to_wstring(snapshot.readyThreadCount) + L" ready, " + std::to_wstring((int)snapshot.cpuPct) + L"% CPU");
      risk += 18;
    }

    // 23. Kernel object creation (handle table growth)
    if (prev.totalHandles > 0 && snapshot.totalHandles > 0) {
      const long long hGrowth = static_cast<long long>(snapshot.totalHandles) - static_cast<long long>(prev.totalHandles);
      if (hGrowth > 10000 && hGrowth <= 30000) {
        diagnostics.push_back(L"Handle growth: +" + std::to_wstring(hGrowth) + L" (steady)");
        risk += 6;
      }
    }

    // 24. Device stack change (driver count delta of 1-3)
    if (snapshot.driverCount > prev.driverCount && snapshot.driverCount - prev.driverCount <= 3) {
      diagnostics.push_back(L"Device stack change: +" + std::to_wstring(snapshot.driverCount - prev.driverCount) + L" driver(s)");
      risk += 4;
    }

    // 25. System uptime anomaly (unexpected reboot detection)
    if (snapshot.uptimeMs < prev.uptimeMs && prev.uptimeMs > 60000) {
      headline = L"Unexpected system reboot detected.";
      insight = L"System uptime has decreased, indicating an unexpected restart or crash.";
      diagnostics.push_back(L"Uptime: " + std::to_wstring(prev.uptimeMs / 1000) + L"s -> " + std::to_wstring(snapshot.uptimeMs / 1000) + L"s");
      risk += 20;
    }
  }

  // --- Security & Integrity (25 detections) ---
  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    // 1. Unsigned driver load
    for (const auto& drv : snapshot.unsignedDriverNames) {
      if (prev.unsignedDriverNames.find(drv) == prev.unsignedDriverNames.end()) {
        headline = L"Unsigned driver load: " + drv;
        insight = L"A kernel driver without a valid digital signature has been loaded.";
        diagnostics.push_back(L"Unsigned: " + drv);
        risk += 18;
      }
    }

    // 2. Code signature invalid (self)
    if (prev.selfSignatureValid == 1 && snapshot.selfSignatureValid == 0) {
      headline = L"Code signature invalid on executable.";
      insight = L"The running executable no longer passes signature verification.";
      diagnostics.push_back(L"Signature: valid -> invalid on " + snapshot.selfExePath);
      risk += 20;
    }
    if (snapshot.selfSignatureValid == 0 && prev.selfSignatureValid != 0) {
      headline = L"Executable signature verification failed.";
      insight = L"The main executable does not have a valid code signature.";
      diagnostics.push_back(L"No valid signature on executable.");
      risk += 16;
    }

    // 3. Certificate chain failure (proxy: unsigned drivers)
    if (snapshot.unsignedDriverCount > prev.unsignedDriverCount && prev.unsignedDriverCount > 0) {
      headline = L"Certificate chain failure detected.";
      insight = L"Additional unsigned drivers loaded, indicating certificate chain issues.";
      diagnostics.push_back(L"Unsigned drivers: " + std::to_wstring(prev.unsignedDriverCount) + L" -> " + std::to_wstring(snapshot.unsignedDriverCount));
      risk += 14;
    }

    // 4. Tampered executable hash
    if (prev.selfHashVerified == 1 && snapshot.selfHashVerified == 0) {
      headline = L"Tampered executable hash detected.";
      insight = L"The executable file has been modified since last verification.";
      diagnostics.push_back(L"Executable hash verification changed.");
      risk += 20;
    }

    // 5. Binary reputation change (proxy: hash change)
    if (prev.selfHashVerified == 0 && snapshot.selfHashVerified == 1) {
      headline = L"Binary reputation restored.";
      insight = L"Executable hash now passes verification after previous failure.";
      diagnostics.push_back(L"Executable hash re-verified.");
      risk += 2;
    }

    // 6. Suspicious DLL sideloading (new hook/inject/detour modules)
    for (const auto& mod : snapshot.suspiciousModules) {
      if (prev.suspiciousModules.find(mod) == prev.suspiciousModules.end()) {
        headline = L"Suspicious DLL sideloading: " + mod;
        insight = L"A module with suspicious name characteristics has been loaded.";
        diagnostics.push_back(L"Suspicious module: " + mod);
        risk += 14;
      }
    }

    // 7. Suspicious script execution
    if (snapshot.suspiciousScriptHosts > prev.suspiciousScriptHosts && prev.suspiciousScriptHosts == 0) {
      headline = L"Suspicious script execution detected.";
      insight = L"Script host processes (wscript/cscript/mshta/powershell) detected running.";
      diagnostics.push_back(L"Script hosts: " + std::to_wstring(snapshot.suspiciousScriptHosts));
      risk += 12;
    }
    if (snapshot.suspiciousScriptHosts > 3) {
      headline = L"Script execution burst detected.";
      insight = L"Abnormal number of script host processes running simultaneously.";
      diagnostics.push_back(L"Script hosts: " + std::to_wstring(snapshot.suspiciousScriptHosts) + L" active");
      risk += 14;
    }

    // 8. Macro execution event (proxy: Office processes)
    if (snapshot.suspiciousScriptHosts > prev.suspiciousScriptHosts) {
      diagnostics.push_back(L"Macro/script activity increase detected.");
      risk += 6;
    }

    // 9. UAC prompt escalation
    if (snapshot.uacConsentProcesses > prev.uacConsentProcesses) {
      headline = L"UAC prompt escalation detected.";
      insight = L"User Account Control consent dialog is active, indicating privilege escalation request.";
      diagnostics.push_back(L"UAC consent processes: " + std::to_wstring(snapshot.uacConsentProcesses));
      risk += 10;
    }

    // 10. Admin token creation (proxy: UAC consent + high integrity)
    if (snapshot.uacConsentProcesses > 0 && snapshot.cpuPct > 50.0) {
      diagnostics.push_back(L"Admin token creation: UAC active under load.");
      risk += 8;
    }

    // 11. Protected process access (proxy: lsass access count changes)
    if (snapshot.lsassAccessCount > prev.lsassAccessCount) {
      headline = L"LSASS access attempt detected.";
      insight = L"Process count referencing lsass.exe has increased, indicating potential credential access.";
      diagnostics.push_back(L"LSASS references: " + std::to_wstring(snapshot.lsassAccessCount));
      risk += 18;
    }

    // 12. LSASS access attempt
    if (snapshot.lsassAccessCount > 3) {
      headline = L"LSASS enumeration pattern detected.";
      insight = L"Multiple processes have references to lsass.exe, a common credential dump target.";
      diagnostics.push_back(L"LSASS count: " + std::to_wstring(snapshot.lsassAccessCount));
      risk += 16;
    }

    // 13. Credential dump pattern (proxy: lsass + script host)
    if (snapshot.lsassAccessCount > 1 && snapshot.suspiciousScriptHosts > 0) {
      headline = L"Credential dump pattern detected.";
      insight = L"Script host processes running with LSASS references suggest credential harvesting.";
      diagnostics.push_back(L"Credential risk: " + std::to_wstring(snapshot.suspiciousScriptHosts) + L" scripts, " + std::to_wstring(snapshot.lsassAccessCount) + L" LSASS refs");
      risk += 20;
    }

    // 14. Anti-debugging indicator
    if (snapshot.debugPortActive == 1 && prev.debugPortActive == 0) {
      headline = L"Anti-debugging indicator detected.";
      insight = L"Debugger has been attached to the running process.";
      diagnostics.push_back(L"Debugger attached to process.");
      risk += 10;
    }

    // 15. Anti-VM indicator
    if (snapshot.vmIndicators > prev.vmIndicators && prev.vmIndicators == 0) {
      headline = L"Anti-VM indicator detected.";
      insight = L"Virtual machine guest services detected, indicating sandbox/analysis environment.";
      diagnostics.push_back(L"VM indicators: " + std::to_wstring(snapshot.vmIndicators));
      risk += 8;
    }

    // 16. Hook installation attempt
    if (snapshot.hookModulesDetected > prev.hookModulesDetected) {
      headline = L"Hook installation attempt detected.";
      insight = L"Modules with hook/inject/detour naming patterns have been loaded.";
      diagnostics.push_back(L"Hook modules: " + std::to_wstring(snapshot.hookModulesDetected));
      risk += 16;
    }

    // 17. Inline patch detection (proxy: PE header tamper)
    if (snapshot.peHeaderTamper > prev.peHeaderTamper && prev.peHeaderTamper == 0) {
      headline = L"Inline patch detected.";
      insight = L"PE header of a loaded module has been modified in memory.";
      diagnostics.push_back(L"PE tamper: " + std::to_wstring(snapshot.peHeaderTamper) + L" module(s)");
      risk += 18;
    }

    // 18. Import table patch detection (proxy: PE header tamper)
    if (snapshot.peHeaderTamper > 0) {
      diagnostics.push_back(L"Import table patch risk: " + std::to_wstring(snapshot.peHeaderTamper) + L" tampered headers.");
      risk += 8;
    }

    // 19. Syscall stub patch detection (proxy: PE tamper + hook modules)
    if (snapshot.peHeaderTamper > 0 && snapshot.hookModulesDetected > 0) {
      headline = L"Syscall stub patch risk detected.";
      insight = L"PE tampering combined with hook modules suggests syscall interception.";
      diagnostics.push_back(L"Syscall risk: " + std::to_wstring(snapshot.peHeaderTamper) + L" tamper, " + std::to_wstring(snapshot.hookModulesDetected) + L" hooks");
      risk += 18;
    }

    // 20. PE header tamper
    if (snapshot.peHeaderTamper > prev.peHeaderTamper) {
      headline = L"PE header tamper detected.";
      insight = L"Additional module headers have been modified in memory since last sample.";
      diagnostics.push_back(L"PE tamper delta: " + std::to_wstring(prev.peHeaderTamper) + L" -> " + std::to_wstring(snapshot.peHeaderTamper));
      risk += 16;
    }

    // 21. Integrity check failure
    if (snapshot.selfSignatureValid == 0 || snapshot.selfHashVerified == 0) {
      headline = L"Integrity check failure.";
      insight = L"Self-integrity verification failed for the running executable.";
      diagnostics.push_back(L"Self check: sig=" + std::to_wstring(snapshot.selfSignatureValid) + L" hash=" + std::to_wstring(snapshot.selfHashVerified));
      risk += 14;
    }

    // 22. Sandbox escape indicator (proxy: VM detection + hook modules)
    if (snapshot.vmIndicators > 0 && snapshot.hookModulesDetected > 0) {
      headline = L"Sandbox escape indicator detected.";
      insight = L"VM environment with hook modules suggests sandbox analysis or escape attempt.";
      diagnostics.push_back(L"Sandbox risk: VM=" + std::to_wstring(snapshot.vmIndicators) + L" hooks=" + std::to_wstring(snapshot.hookModulesDetected));
      risk += 16;
    }

    // 23. Privilege misuse pattern (proxy: UAC + admin processes)
    if (snapshot.uacConsentProcesses > 0 && snapshot.suspiciousScriptHosts > 0) {
      headline = L"Privilege misuse pattern detected.";
      insight = L"UAC escalation combined with script execution indicates privilege abuse.";
      diagnostics.push_back(L"Privilege risk: UAC=" + std::to_wstring(snapshot.uacConsentProcesses) + L" scripts=" + std::to_wstring(snapshot.suspiciousScriptHosts));
      risk += 14;
    }

    // 24. Suspicious scheduled task
    if (snapshot.scheduledTaskCount > prev.scheduledTaskCount + 5 && prev.scheduledTaskCount > 0) {
      headline = L"Suspicious scheduled task creation.";
      insight = L"Multiple new scheduled tasks have been created, a common persistence mechanism.";
      diagnostics.push_back(L"Tasks: " + std::to_wstring(prev.scheduledTaskCount) + L" -> " + std::to_wstring(snapshot.scheduledTaskCount));
      risk += 12;
    }

    // 25. Persistence mechanism detection
    if (snapshot.scheduledTaskCount > prev.scheduledTaskCount + 10 && prev.scheduledTaskCount > 0) {
      headline = L"Persistence mechanism detected.";
      insight = L"Large number of new scheduled tasks indicate persistence installation.";
      diagnostics.push_back(L"New tasks: +" + std::to_wstring(snapshot.scheduledTaskCount - prev.scheduledTaskCount));
      risk += 16;
    }
  }

  // --- Power & Battery (25 detections) ---
  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    // 1. AC power connect
    if (prev.acLineStatus == 0 && snapshot.acLineStatus == 1) {
      headline = L"AC power connected.";
      insight = L"System has been connected to AC power.";
      diagnostics.push_back(L"AC line: disconnected -> connected.");
      risk += 4;
    }

    // 2. AC power disconnect
    if (prev.acLineStatus == 1 && snapshot.acLineStatus == 0) {
      headline = L"AC power disconnected.";
      insight = L"System has been disconnected from AC power.";
      diagnostics.push_back(L"AC line: connected -> disconnected.");
      risk += 6;
    }

    // 3. Battery present
    if (prev.batteryFlag == 128 && snapshot.batteryFlag != 128) {
      headline = L"Battery detected.";
      insight = L"A battery is now present in the system.";
      diagnostics.push_back(L"Battery: not present -> present.");
      risk += 2;
    }

    // 4. Battery removal
    if (prev.batteryFlag != 128 && snapshot.batteryFlag == 128) {
      headline = L"Battery removed.";
      insight = L"The battery is no longer detected.";
      diagnostics.push_back(L"Battery: present -> removed.");
      risk += 4;
    }

    // 5. Battery charge state
    if (snapshot.batteryLifePercent >= 0 && snapshot.batteryLifePercent <= 100 && prev.batteryLifePercent >= 0) {
      const int chargeDelta = snapshot.batteryLifePercent - prev.batteryLifePercent;
      if (chargeDelta > 5) {
        diagnostics.push_back(L"Battery charged: " + std::to_wstring(prev.batteryLifePercent) + L"% -> " + std::to_wstring(snapshot.batteryLifePercent) + L"%.");
        risk += 2;
      } else if (chargeDelta < -5) {
        diagnostics.push_back(L"Battery drained: " + std::to_wstring(prev.batteryLifePercent) + L"% -> " + std::to_wstring(snapshot.batteryLifePercent) + L"%.");
        risk += 4;
      }
    }

    // 6. Battery drain rate spike
    if (snapshot.batteryLifeTimeSec > 0 && prev.batteryLifeTimeSec > 0 && snapshot.acLineStatus == 0) {
      if (prev.batteryLifeTimeSec > snapshot.batteryLifeTimeSec + 600) {
        headline = L"Battery drain rate spike.";
        insight = L"Battery remaining time dropped by more than 10 minutes between samples.";
        diagnostics.push_back(L"Drain: " + std::to_wstring(prev.batteryLifeTimeSec / 60) + L"min -> " + std::to_wstring(snapshot.batteryLifeTimeSec / 60) + L"min.");
        risk += 10;
      }
    }

    // 7. Battery wear level
    if (snapshot.batteryWearLevel >= 0 && snapshot.batteryWearLevel != prev.batteryWearLevel && prev.batteryWearLevel >= 0) {
      if (snapshot.batteryWearLevel > 30) {
        headline = L"Battery wear level elevated.";
        insight = L"Battery wear has increased, indicating reduced capacity.";
        diagnostics.push_back(L"Wear: " + std::to_wstring(prev.batteryWearLevel) + L"% -> " + std::to_wstring(snapshot.batteryWearLevel) + L"%.");
        risk += 8;
      }
    }

    // 8. Battery cycle count
    if (snapshot.batteryCycleCount >= 0 && snapshot.batteryCycleCount != prev.batteryCycleCount && prev.batteryCycleCount >= 0) {
      const int cycleDelta = snapshot.batteryCycleCount - prev.batteryCycleCount;
      if (cycleDelta > 0) {
        diagnostics.push_back(L"Battery cycles: +" + std::to_wstring(cycleDelta) + L" (total: " + std::to_wstring(snapshot.batteryCycleCount) + L").");
        risk += 2;
      }
    }

    // 9. Charging speed change
    if (snapshot.batteryChargeRate != prev.batteryChargeRate && snapshot.batteryChargeRate != 0 && prev.batteryChargeRate != 0) {
      if (snapshot.batteryChargeRate < prev.batteryChargeRate * 0.5) {
        headline = L"Charging speed dropped.";
        insight = L"Battery charge rate has halved, possibly due to thermal throttling.";
        diagnostics.push_back(L"Charge rate: " + std::to_wstring(prev.batteryChargeRate) + L" -> " + std::to_wstring(snapshot.batteryChargeRate) + L" mW.");
        risk += 10;
      } else if (snapshot.batteryChargeRate > prev.batteryChargeRate * 2) {
        diagnostics.push_back(L"Charging speed increased: " + std::to_wstring(prev.batteryChargeRate) + L" -> " + std::to_wstring(snapshot.batteryChargeRate) + L" mW.");
        risk += 2;
      }
    }

    // 10. Fast charge state
    if (snapshot.batteryChargeRate > 40000 && prev.batteryChargeRate <= 40000) {
      headline = L"Fast charge state detected.";
      insight = L"Battery charge rate exceeds 40W, indicating fast charging.";
      diagnostics.push_back(L"Fast charge: " + std::to_wstring(snapshot.batteryChargeRate) + L" mW.");
      risk += 2;
    }

    // 11. Idle power draw rise
    if (snapshot.idlePowerDrawHigh == 1 && prevIdlePowerDrawHigh_ == 0) {
      headline = L"Idle power draw elevated.";
      insight = L"System is on battery with low CPU but draining quickly, indicating background activity.";
      diagnostics.push_back(L"Idle drain: battery " + std::to_wstring(snapshot.batteryLifeTimeSec / 60) + L"min remaining at " + std::to_wstring((int)snapshot.cpuPct) + L"% CPU.");
      risk += 10;
    }

    // 12. Suspended power state
    if (snapshot.sleepStateActive == 1 && prev.sleepStateActive == 0) {
      headline = L"Suspended power state.";
      insight = L"System has entered a suspended state.";
      diagnostics.push_back(L"Power state: suspended.");
      risk += 2;
    }

    // 13. Modern standby entry
    if (snapshot.modernStandbyActive == 1 && prev.modernStandbyActive != 1) {
      headline = L"Modern standby entry.";
      insight = L"System has entered Modern Standby (S0 Low Power Idle).";
      diagnostics.push_back(L"Standby: entered Modern Standby.");
      risk += 2;
    }

    // 14. Modern standby exit
    if (snapshot.modernStandbyActive == 0 && prev.modernStandbyActive == 1) {
      headline = L"Modern standby exit.";
      insight = L"System has exited Modern Standby.";
      diagnostics.push_back(L"Standby: exited Modern Standby.");
      risk += 2;
    }

    // 15. Sleep state entry
    if (snapshot.sleepStateActive == 1 && prev.sleepStateActive == 0) {
      headline = L"Sleep state entry.";
      insight = L"System is entering a sleep state.";
      diagnostics.push_back(L"Sleep: entering sleep state.");
      risk += 2;
    }

    // 16. Sleep state exit
    if (snapshot.sleepStateActive == 0 && prev.sleepStateActive == 1) {
      headline = L"Sleep state exit.";
      insight = L"System has exited sleep state.";
      diagnostics.push_back(L"Sleep: exited sleep state.");
      risk += 2;
    }

    // 17. Hibernate entry
    if (snapshot.hibernateActive == 1 && prev.hibernateActive == 0) {
      headline = L"Hibernate entry.";
      insight = L"System is entering hibernation.";
      diagnostics.push_back(L"Hibernate: entering hibernation.");
      risk += 2;
    }

    // 18. Hibernate exit
    if (snapshot.hibernateActive == 0 && prev.hibernateActive == 1) {
      headline = L"Hibernate exit.";
      insight = L"System has exited hibernation.";
      diagnostics.push_back(L"Hibernate: exited hibernation.");
      risk += 2;
    }

    // 19. Power plan change
    if (snapshot.powerPlanIndex != prev.powerPlanIndex && prev.powerPlanIndex >= 0) {
      const wchar_t* plans[] = { L"Power Saver", L"Balanced", L"High Performance" };
      headline = L"Power plan changed.";
      insight = L"Active power plan has been modified.";
      diagnostics.push_back(L"Power plan: " + std::wstring(plans[prev.powerPlanIndex]) + L" -> " + std::wstring(plans[snapshot.powerPlanIndex]) + L".");
      risk += 8;
    }

    // 20. Power saver mode
    if (snapshot.powerSaverActive == 1 && prevPowerSaverActive_ == 0) {
      headline = L"Power saver mode activated.";
      insight = L"System has switched to power saving mode.";
      diagnostics.push_back(L"Power plan: Power Saver activated.");
      risk += 2;
    }

    // 21. High performance mode
    if (snapshot.highPerfActive == 1 && prevHighPerfActive_ == 0) {
      headline = L"High performance mode activated.";
      insight = L"System has switched to high performance mode, increasing power draw.";
      diagnostics.push_back(L"Power plan: High Performance activated.");
      risk += 8;
    }

    // 22. Balanced mode
    if (snapshot.balancedActive == 1 && prev.highPerfActive == 1) {
      headline = L"Balanced mode activated.";
      insight = L"System switched from High Performance to Balanced.";
      diagnostics.push_back(L"Power plan: Balanced mode restored.");
      risk += 2;
    }

    // 23. CPU package power cap
    if (snapshot.cpuPackagePowerCap >= 0 && snapshot.cpuPackagePowerCap != prev.cpuPackagePowerCap && prev.cpuPackagePowerCap >= 0) {
      if (snapshot.cpuPackagePowerCap < prev.cpuPackagePowerCap) {
        headline = L"CPU package power cap reduced.";
        insight = L"CPU power cap has been lowered, limiting maximum CPU performance.";
        diagnostics.push_back(L"CPU cap: " + std::to_wstring(prev.cpuPackagePowerCap) + L"% -> " + std::to_wstring(snapshot.cpuPackagePowerCap) + L"%.");
        risk += 8;
      } else {
        diagnostics.push_back(L"CPU cap: " + std::to_wstring(prev.cpuPackagePowerCap) + L"% -> " + std::to_wstring(snapshot.cpuPackagePowerCap) + L"%.");
        risk += 2;
      }
    }

    // 24. System-wide power cap
    if (snapshot.systemPowerCap >= 0 && snapshot.systemPowerCap != prev.systemPowerCap && prev.systemPowerCap >= 0) {
      headline = L"System-wide power cap changed.";
      insight = L"System power limit has been modified.";
      diagnostics.push_back(L"System cap: " + std::to_wstring(prev.systemPowerCap) + L"% -> " + std::to_wstring(snapshot.systemPowerCap) + L"%.");
      risk += 8;
    }

    // 25. Battery temperature rise
    if (snapshot.batteryTemperature >= 0 && prev.batteryTemperature >= 0) {
      if (snapshot.batteryTemperature > prev.batteryTemperature + 10) {
        headline = L"Battery temperature rising.";
        insight = L"Battery temperature has increased by more than 10C.";
        diagnostics.push_back(L"Battery temp: " + std::to_wstring(prev.batteryTemperature) + L"C -> " + std::to_wstring(snapshot.batteryTemperature) + L"C.");
        risk += 10;
      }
    }
  }

  // --- Thermal & Cooling (25 detections) ---
  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    // 1. CPU package temperature rise
    if (snapshot.cpuTempC > prev.cpuTempC + 10.0 && snapshot.cpuTempC > 50.0) {
      headline = L"CPU temperature rising.";
      insight = L"CPU package temperature increased by more than 10C above 50C.";
      diagnostics.push_back(L"CPU temp: " + std::to_wstring((int)prev.cpuTempC) + L"C -> " + std::to_wstring((int)snapshot.cpuTempC) + L"C.");
      risk += 12;
    }

    // 2. CPU core temperature rise
    if (snapshot.cpuCoreTempC > prevCpuCoreTempC_ + 8.0 && snapshot.cpuCoreTempC > 55.0) {
      headline = L"CPU core temperature rising.";
      insight = L"Average CPU core temperature increased by more than 8C.";
      diagnostics.push_back(L"Core temp: " + std::to_wstring((int)prevCpuCoreTempC_) + L"C -> " + std::to_wstring((int)snapshot.cpuCoreTempC) + L"C.");
      risk += 12;
    }

    // 3. GPU temperature rise
    if (snapshot.gpuTempC > prev.gpuTempC + 15.0 && snapshot.gpuTempC > 60.0 && !snapshot.gpuTempEstimated) {
      headline = L"GPU temperature rising.";
      insight = L"GPU temperature increased by more than 15C above 60C.";
      diagnostics.push_back(L"GPU temp: " + std::to_wstring((int)prev.gpuTempC) + L"C -> " + std::to_wstring((int)snapshot.gpuTempC) + L"C.");
      risk += 12;
    }

    // 4. VRM temperature rise
    if (snapshot.vrmTempC > prevVrmTempC_ + 8.0 && snapshot.vrmTempC > 60.0) {
      headline = L"VRM temperature rising.";
      insight = L"Voltage regulator module temperature increased significantly.";
      diagnostics.push_back(L"VRM temp: " + std::to_wstring((int)prevVrmTempC_) + L"C -> " + std::to_wstring((int)snapshot.vrmTempC) + L"C.");
      risk += 10;
    }

    // 5. Motherboard temperature rise
    if (snapshot.motherboardTempC > prevMotherboardTempC_ + 5.0 && snapshot.motherboardTempC > 40.0) {
      headline = L"Motherboard temperature rising.";
      insight = L"Motherboard sensor temperature increased by more than 5C.";
      diagnostics.push_back(L"MB temp: " + std::to_wstring((int)prevMotherboardTempC_) + L"C -> " + std::to_wstring((int)snapshot.motherboardTempC) + L"C.");
      risk += 10;
    }

    // 6. SSD temperature rise
    if (snapshot.storageTempC > prev.storageTempC + 8.0 && snapshot.storageTempC > 45.0) {
      headline = L"SSD temperature rising.";
      insight = L"Storage drive temperature increased by more than 8C.";
      diagnostics.push_back(L"SSD temp: " + std::to_wstring((int)prev.storageTempC) + L"C -> " + std::to_wstring((int)snapshot.storageTempC) + L"C.");
      risk += 10;
    }

    // 7. Fan speed increase
    if (snapshot.fanSpeeds.size() == prevFanSpeeds_.size() && !snapshot.fanSpeeds.empty()) {
      for (size_t i = 0; i < snapshot.fanSpeeds.size() && i < prevFanSpeeds_.size(); ++i) {
        if (snapshot.fanSpeeds[i] > prevFanSpeeds_[i] * 1.5 && snapshot.fanSpeeds[i] > 1000) {
          headline = L"Fan speed surge detected.";
          insight = L"Fan RPM increased by over 50%, indicating active cooling response.";
          diagnostics.push_back(L"Fan " + std::to_wstring(i) + L": " + std::to_wstring(prevFanSpeeds_[i]) + L" -> " + std::to_wstring(snapshot.fanSpeeds[i]) + L" RPM.");
          risk += 6;
        }
      }
    }

    // 8. Fan speed decrease
    if (snapshot.fanSpeeds.size() == prevFanSpeeds_.size() && !snapshot.fanSpeeds.empty()) {
      for (size_t i = 0; i < snapshot.fanSpeeds.size() && i < prevFanSpeeds_.size(); ++i) {
        if (prevFanSpeeds_[i] > 1000 && snapshot.fanSpeeds[i] < prevFanSpeeds_[i] * 0.5) {
          headline = L"Fan speed drop detected.";
          insight = L"Fan RPM dropped by over 50% under load, possibly indicating fan controller issue.";
          diagnostics.push_back(L"Fan " + std::to_wstring(i) + L": " + std::to_wstring(prevFanSpeeds_[i]) + L" -> " + std::to_wstring(snapshot.fanSpeeds[i]) + L" RPM.");
          risk += 10;
        }
      }
    }

    // 9. Fan stall
    if (!prevFanSpeeds_.empty()) {
      for (size_t i = 0; i < prevFanSpeeds_.size(); ++i) {
        if (prevFanSpeeds_[i] > 500) {
          int curSpeed = (i < snapshot.fanSpeeds.size()) ? snapshot.fanSpeeds[i] : 0;
          if (curSpeed == 0) {
            headline = L"Fan stall detected.";
            insight = L"A previously spinning fan has stopped while system is under load.";
            diagnostics.push_back(L"Fan " + std::to_wstring(i) + L" stalled: " + std::to_wstring(prevFanSpeeds_[i]) + L" -> 0 RPM.");
            risk += 16;
          }
        }
      }
    }

    // 10. Fan failure
    if (snapshot.fanCount < prevFanCount_ && prevFanCount_ > 0) {
      headline = L"Fan failure detected.";
      insight = L"A fan is no longer reported by the hardware monitoring subsystem.";
      diagnostics.push_back(L"Fan count: " + std::to_wstring(prevFanCount_) + L" -> " + std::to_wstring(snapshot.fanCount) + L".");
      risk += 18;
    }

    // 11. Pump failure
    if (snapshot.pumpPresent && snapshot.pumpSpeed == 0 && prevPumpSpeed_ > 0) {
      headline = L"Water pump failure detected.";
      insight = L"Water cooling pump has stopped while previously running.";
      diagnostics.push_back(L"Pump: " + std::to_wstring(prevPumpSpeed_) + L" -> 0 RPM.");
      risk += 20;
    }

    // 12. Cooling loop anomaly
    if (snapshot.cpuTempC > 70.0 && !snapshot.fanSpeeds.empty() && snapshot.fanSpeeds[0] < 500) {
      headline = L"Cooling loop anomaly detected.";
      insight = L"CPU temperature is high but fan speed is low, indicating cooling system issue.";
      diagnostics.push_back(L"Cooling: CPU " + std::to_wstring((int)snapshot.cpuTempC) + L"C at " + std::to_wstring(snapshot.fanSpeeds[0]) + L" RPM.");
      risk += 16;
    }

    // 13. Thermal throttling onset
    if (snapshot.cpuThrottling == 1 && prevCpuThrottling_ == 0) {
      headline = L"Thermal throttling onset.";
      insight = L"CPU has entered thermal throttle state due to high temperature.";
      diagnostics.push_back(L"Throttle: CPU at " + std::to_wstring((int)snapshot.cpuTempC) + L"C, " + std::to_wstring((int)snapshot.cpuPct) + L"% util.");
      risk += 18;
    }

    // 14. Thermal throttling cleared
    if (snapshot.cpuThrottling == 0 && prevCpuThrottling_ == 1) {
      headline = L"Thermal throttling cleared.";
      insight = L"CPU has exited thermal throttle state.";
      diagnostics.push_back(L"Throttle cleared: CPU temp now " + std::to_wstring((int)snapshot.cpuTempC) + L"C.");
      risk += 2;
    }

    // 15. Thermal sensor unavailable
    if (snapshot.thermalSensorFailures > prevThermalSensorFailures_) {
      headline = L"Thermal sensor unavailable.";
      insight = L"A thermal zone returned invalid or null readings.";
      diagnostics.push_back(L"Sensor failures: " + std::to_wstring(prevThermalSensorFailures_) + L" -> " + std::to_wstring(snapshot.thermalSensorFailures) + L".");
      risk += 10;
    }

    // 16. Thermal sensor drift
    if (snapshot.cpuCoreTempMax > 0.0 && snapshot.cpuCoreTempC > 0.0) {
      const double drift = snapshot.cpuCoreTempMax - snapshot.cpuCoreTempC;
      if (drift > 25.0) {
        headline = L"Thermal sensor drift detected.";
        insight = L"Large spread between hottest and average core temperature indicates uneven cooling.";
        diagnostics.push_back(L"Core spread: " + std::to_wstring((int)drift) + L"C (max " + std::to_wstring((int)snapshot.cpuCoreTempMax) + L"C).");
        risk += 10;
      }
    }

    // 17. Hotspot temperature spike
    if (snapshot.cpuCoreTempMax > prevCpuCoreTempMax_ + 15.0 && snapshot.cpuCoreTempMax > 80.0) {
      headline = L"Hotspot temperature spike.";
      insight = L"Hottest core temperature spiked by more than 15C, indicating localized overheating.";
      diagnostics.push_back(L"Hotspot: " + std::to_wstring((int)prevCpuCoreTempMax_) + L"C -> " + std::to_wstring((int)snapshot.cpuCoreTempMax) + L"C.");
      risk += 14;
    }

    // 18. Ambient temperature rise
    if (snapshot.ambientTempC > prevAmbientTempC_ + 5.0 && snapshot.ambientTempC > 30.0) {
      headline = L"Ambient temperature rise.";
      insight = L"Estimated ambient/room temperature has increased, reducing cooling efficiency.";
      diagnostics.push_back(L"Ambient: " + std::to_wstring((int)prevAmbientTempC_) + L"C -> " + std::to_wstring((int)snapshot.ambientTempC) + L"C.");
      risk += 8;
    }

    // 19. Overtemperature warning
    if (snapshot.cpuTempC >= 85.0 || snapshot.gpuTempC >= 85.0 || snapshot.storageTempC >= 65.0) {
      headline = L"Overtemperature warning.";
      insight = L"Temperature has exceeded the safe operating envelope.";
      diagnostics.push_back(L"Temp: CPU " + std::to_wstring((int)snapshot.cpuTempC) + L"C, GPU " + std::to_wstring((int)snapshot.gpuTempC) + L"C, SSD " + std::to_wstring((int)snapshot.storageTempC) + L"C.");
      risk += 16;
    }

    // 20. Critical temperature warning
    if (snapshot.cpuTempC >= 95.0 || snapshot.gpuTempC >= 95.0) {
      headline = L"Critical temperature warning.";
      insight = L"Temperature at critical level. Hardware may shut down to prevent damage.";
      diagnostics.push_back(L"Critical temp: CPU " + std::to_wstring((int)snapshot.cpuTempC) + L"C, GPU " + std::to_wstring((int)snapshot.gpuTempC) + L"C.");
      risk += 20;
    }

    // 21. Thermal hysteresis event (temp dropped >5C between samples while previously hot)
    if (prev.cpuTempC > 70.0 && snapshot.cpuTempC < prev.cpuTempC - 5.0) {
      headline = L"Thermal hysteresis detected.";
      insight = L"Temperature dropped rapidly between samples, indicating unstable thermal state or cooler transition.";
      diagnostics.push_back(L"Hysteresis: " + std::to_wstring((int)prev.cpuTempC) + L"C -> " + std::to_wstring((int)snapshot.cpuTempC) + L"C.");
      risk += 10;
    }

    // 22. Heat soak detection
    if (snapshot.cpuTempC > 70.0 && snapshot.cpuPct > 80.0) {
      if (heatSoakBaseline_ == 0.0) heatSoakBaseline_ = snapshot.cpuTempC;
      if (snapshot.cpuTempC > heatSoakBaseline_ + 10.0) {
        headline = L"Heat soak detected.";
        insight = L"CPU temperature has risen steadily under sustained load, indicating heat soak.";
        diagnostics.push_back(L"Heat soak: +10C above " + std::to_wstring((int)heatSoakBaseline_) + L"C baseline.");
        risk += 12;
      }
    } else {
      heatSoakBaseline_ = 0.0;
    }

    // 23. Cooling curve change
    if (snapshot.cpuTempC < prev.cpuTempC - 5.0 && prev.cpuPct > 80.0 && snapshot.cpuPct < 30.0) {
      headline = L"Cooling curve change.";
      insight = L"CPU temperature dropped quickly after load reduction, indicating cooling system response.";
      diagnostics.push_back(L"Cooling: " + std::to_wstring((int)prev.cpuTempC) + L"C -> " + std::to_wstring((int)snapshot.cpuTempC) + L"C after load drop.");
      risk += 2;
    }

    // 24. Thermal recovery event
    if (prev.cpuTempC >= 85.0 && snapshot.cpuTempC < 70.0) {
      headline = L"Thermal recovery event.";
      insight = L"System has recovered from overtemperature condition.";
      diagnostics.push_back(L"Recovery: " + std::to_wstring((int)prev.cpuTempC) + L"C -> " + std::to_wstring((int)snapshot.cpuTempC) + L"C.");
      risk += 2;
    }

    // 25. Airflow obstruction indicator
    if (snapshot.cpuTempC > 70.0 && snapshot.fanSpeeds.size() == prevFanSpeeds_.size()) {
      bool allFansMax = true;
      for (size_t i = 0; i < snapshot.fanSpeeds.size(); ++i) {
        int maxFan = 0;
        if (i < prevFanSpeeds_.size()) maxFan = std::max(prevFanSpeeds_[i], snapshot.fanSpeeds[i]);
        if (maxFan < 2000) allFansMax = false;
      }
      if (allFansMax && snapshot.cpuTempC > prev.cpuTempC) {
        headline = L"Airflow obstruction suspected.";
        insight = L"Fans at high speed but temperature still rising, indicating possible airflow blockage.";
        diagnostics.push_back(L"Airflow: fans at high RPM, temp " + std::to_wstring((int)snapshot.cpuTempC) + L"C rising.");
        risk += 14;
      }
    }
  }

  // --- Hardware Sensors & Board (25 detections) ---
  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    // 1. Embedded controller event
    if (snapshot.ecEvents > prev.ecEvents) {
      headline = L"Embedded controller event detected.";
      insight = L"Embedded controller event count increased, indicating low-level hardware event.";
      diagnostics.push_back(L"EC events: +" + std::to_wstring(snapshot.ecEvents - prev.ecEvents) + L".");
      risk += 8;
    }

    // 6. Voltage rail anomaly
    if (snapshot.voltage12V > 0 && prev.voltage12V > 0) {
      const double delta12 = std::abs(snapshot.voltage12V - prev.voltage12V);
      if (delta12 > 0.5) {
        headline = L"Voltage rail anomaly detected.";
        insight = L"12V rail voltage fluctuated by more than 0.5V between samples.";
        diagnostics.push_back(L"12V: " + std::to_wstring((int)(prev.voltage12V * 1000)) + L"mV -> " + std::to_wstring((int)(snapshot.voltage12V * 1000)) + L"mV.");
        risk += 12;
      }
    }

    // 7. 12V rail drop
    if (snapshot.voltage12V > 0 && prev.voltage12V > 0 && snapshot.voltage12V < prev.voltage12V - 0.3) {
      headline = L"12V rail voltage drop.";
      insight = L"12V supply voltage dropped by more than 300mV, indicating power supply stress.";
      diagnostics.push_back(L"12V drop: " + std::to_wstring((int)(prev.voltage12V * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltage12V * 1000)) + L" mV.");
      risk += 14;
    }

    // 8. 5V rail drop
    if (snapshot.voltage5V > 0 && prev.voltage5V > 0 && snapshot.voltage5V < prev.voltage5V - 0.2) {
      headline = L"5V rail voltage drop.";
      insight = L"5V supply voltage dropped by more than 200mV.";
      diagnostics.push_back(L"5V drop: " + std::to_wstring((int)(prev.voltage5V * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltage5V * 1000)) + L" mV.");
      risk += 14;
    }

    // 9. 3.3V rail drop
    if (snapshot.voltage33V > 0 && prev.voltage33V > 0 && snapshot.voltage33V < prev.voltage33V - 0.15) {
      headline = L"3.3V rail voltage drop.";
      insight = L"3.3V supply voltage dropped by more than 150mV.";
      diagnostics.push_back(L"3.3V drop: " + std::to_wstring((int)(prev.voltage33V * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltage33V * 1000)) + L" mV.");
      risk += 12;
    }

    // 10. Vcore fluctuation
    if (snapshot.voltageVcore > 0 && prev.voltageVcore > 0) {
      const double vcoreDelta = std::abs(snapshot.voltageVcore - prev.voltageVcore);
      if (vcoreDelta > 0.1) {
        headline = L"Vcore voltage fluctuation.";
        insight = L"CPU core voltage fluctuated by more than 100mV, indicating VRM instability.";
        diagnostics.push_back(L"Vcore: " + std::to_wstring((int)(prev.voltageVcore * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltageVcore * 1000)) + L" mV.");
        risk += 10;
      }
    }

    // 11. VRAM voltage fluctuation
    if (snapshot.voltageVram > 0 && prev.voltageVram > 0) {
      const double vramDelta = std::abs(snapshot.voltageVram - prev.voltageVram);
      if (vramDelta > 0.05) {
        headline = L"VRAM voltage fluctuation.";
        insight = L"Memory/VRAM voltage fluctuated by more than 50mV.";
        diagnostics.push_back(L"VRAM V: " + std::to_wstring((int)(prev.voltageVram * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltageVram * 1000)) + L" mV.");
        risk += 10;
      }
    }

    // 12. Chipset sensor fault
    if (snapshot.sensorPollFailures > prevSensorPollFailures_ && snapshot.sensorPollFailures > 0) {
      headline = L"Chipset sensor fault.";
      insight = L"Sensor polling failures have increased, indicating chipset sensor issues.";
      diagnostics.push_back(L"Sensor faults: " + std::to_wstring(prevSensorPollFailures_) + L" -> " + std::to_wstring(snapshot.sensorPollFailures) + L".");
      risk += 14;
    }

    // 13. Motherboard sensor fault
    if (snapshot.boardTempHotspot > 0 && prev.boardTempHotspot > 0) {
      const double boardDelta = std::abs(snapshot.boardTempHotspot - prev.boardTempHotspot);
      if (boardDelta > 15.0) {
        headline = L"Motherboard sensor fault.";
        insight = L"Board temperature reading jumped by more than 15C, indicating sensor anomaly.";
        diagnostics.push_back(L"Board temp: " + std::to_wstring((int)prev.boardTempHotspot) + L"C -> " + std::to_wstring((int)snapshot.boardTempHotspot) + L"C.");
        risk += 14;
      }
    }

    // 14. PCIe bus error
    if (snapshot.pcieErrors > prev.pcieErrors) {
      headline = L"PCIe bus error detected.";
      insight = L"PCIe bus error count increased, indicating link instability.";
      diagnostics.push_back(L"PCIe errors: +" + std::to_wstring(snapshot.pcieErrors - prev.pcieErrors) + L".");
      risk += 12;
    }

    // 15. USB controller reset
    if (snapshot.usbResets > prev.usbResets) {
      headline = L"USB controller reset detected.";
      insight = L"USB controller has been reset, indicating USB device or controller instability.";
      diagnostics.push_back(L"USB resets: +" + std::to_wstring(snapshot.usbResets - prev.usbResets) + L".");
      risk += 8;
    }

    // 16. SATA controller reset
    if (snapshot.sataResets > prev.sataResets) {
      headline = L"SATA controller reset detected.";
      insight = L"SATA controller has been reset, indicating storage link instability.";
      diagnostics.push_back(L"SATA resets: +" + std::to_wstring(snapshot.sataResets - prev.sataResets) + L".");
      risk += 10;
    }

    // 17. Thunderbolt controller event
    if (snapshot.thunderboltEvents > prev.thunderboltEvents) {
      headline = L"Thunderbolt controller event.";
      insight = L"Thunderbolt controller event detected, indicating dock or device connectivity change.";
      diagnostics.push_back(L"TB events: +" + std::to_wstring(snapshot.thunderboltEvents - prev.thunderboltEvents) + L".");
      risk += 6;
    }

    // 18. TPM state change
    if (snapshot.tpmReady != prev.tpmReady && prev.tpmPresent == 1) {
      headline = L"TPM state change detected.";
      insight = L"Trusted Platform Module readiness state has changed.";
      diagnostics.push_back(L"TPM: " + std::wstring(prev.tpmReady ? L"ready" : L"not ready") + L" -> " + std::wstring(snapshot.tpmReady ? L"ready" : L"not ready") + L".");
      risk += 14;
    }

    // 19. TPM failure
    if (prev.tpmReady == 1 && snapshot.tpmReady == 0) {
      headline = L"TPM failure detected.";
      insight = L"Trusted Platform Module has become unavailable, affecting security features.";
      diagnostics.push_back(L"TPM failed: ready -> not ready.");
      risk += 18;
    }

    // 20. RTC drift
    if (prev.systemTime100ns > 0 && snapshot.systemTime100ns > 0) {
      const long long timeDelta = static_cast<long long>(snapshot.systemTime100ns - prev.systemTime100ns);
      const long long expectedDelta = (snapshot.uptimeMs - prev.uptimeMs) * 10000LL;
      const long long drift = timeDelta - expectedDelta;
      if (std::abs(drift) > 50000000LL && std::abs(drift) < 100000000LL) {
        headline = L"RTC drift detected.";
        insight = L"Real-time clock has drifted by more than 5 seconds from expected time.";
        diagnostics.push_back(L"RTC drift: " + std::to_wstring(drift / 10000) + L"ms.");
        risk += 10;
      }
    }

    // 21. CMOS battery low
    if (snapshot.cmosBatteryOk == 0 && prev.cmosBatteryOk == 1) {
      headline = L"CMOS battery low.";
      insight = L"CMOS battery voltage is low, settings may be lost on power cycle.";
      diagnostics.push_back(L"CMOS battery: OK -> low.");
      risk += 8;
    }

    // 22. Sensor polling failure
    if (snapshot.sensorPollFailures > prevSensorPollFailures_) {
      headline = L"Sensor polling failure.";
      insight = L"Hardware sensor polling has failed, reducing monitoring coverage.";
      diagnostics.push_back(L"Poll failures: " + std::to_wstring(prevSensorPollFailures_) + L" -> " + std::to_wstring(snapshot.sensorPollFailures) + L".");
      risk += 10;
    }

    // 23. Sensor calibration change
    if (snapshot.voltageVcore > 0 && prev.voltageVcore > 0) {
      const double vcoreRatio = snapshot.voltageVcore / prev.voltageVcore;
      if (vcoreRatio > 1.1 || vcoreRatio < 0.9) {
        headline = L"Sensor calibration change.";
        insight = L"Voltage reading shifted by more than 10%, possibly indicating sensor drift or calibration change.";
        diagnostics.push_back(L"Vcore ratio: " + std::to_wstring((int)(vcoreRatio * 100)) + L"%.");
        risk += 10;
      }
    }

    // 24. Board temperature hotspot
    if (snapshot.boardTempHotspot > 60.0 && snapshot.motherboardTempC > 0 && snapshot.boardTempHotspot > snapshot.motherboardTempC + 15.0) {
      headline = L"Board temperature hotspot.";
      insight = L"Board temperature hotspot is significantly higher than average board temperature.";
      diagnostics.push_back(L"Hotspot: " + std::to_wstring((int)snapshot.boardTempHotspot) + L"C vs avg " + std::to_wstring((int)snapshot.motherboardTempC) + L"C.");
      risk += 10;
    }

    // 25. Hardware watchdog event
    if (snapshot.uptimeMs < prev.uptimeMs && prev.uptimeMs > 60000) {
      headline = L"Hardware watchdog event suspected.";
      insight = L"Unexpected system reboot may have been triggered by hardware watchdog timer.";
      diagnostics.push_back(L"Reboot: uptime " + std::to_wstring(prev.uptimeMs / 1000) + L"s -> " + std::to_wstring(snapshot.uptimeMs / 1000) + L"s.");
      risk += 20;
    }
  }

  // --- Filesystem & Data (25 detections) ---
  if (state_.hasPreviousSnapshot) {
    const auto& prev = *state_.previousSnapshot;

    // 1. File create event (System32 file count increase)
    const int fs32Delta = snapshot.fsSystem32FileCount - prev.fsSystem32FileCount;
    if (fs32Delta > 20) {
      headline = L"File creation burst in System32.";
      insight = L"System32 file count increased by over 20 files between samples.";
      diagnostics.push_back(L"System32: +" + std::to_wstring(fs32Delta) + L" files.");
      risk += 10;
    }

    // 2. File delete event (System32 file count decrease)
    if (prev.fsSystem32FileCount > snapshot.fsSystem32FileCount + 10) {
      headline = L"File deletion event in System32.";
      insight = L"System32 file count decreased by over 10 files.";
      diagnostics.push_back(L"System32: " + std::to_wstring(snapshot.fsSystem32FileCount - prev.fsSystem32FileCount) + L" files.");
      risk += 12;
    }

    // 3. File rename event (file count unchanged but drivers changed)
    if (snapshot.fsDriversFileCount != prev.fsDriversFileCount && snapshot.fsSystem32FileCount == prev.fsSystem32FileCount) {
      headline = L"File rename event in drivers.";
      insight = L"Driver file count changed while System32 count remained stable.";
      diagnostics.push_back(L"Drivers: " + std::to_wstring(prev.fsDriversFileCount) + L" -> " + std::to_wstring(snapshot.fsDriversFileCount) + L".");
      risk += 8;
    }

    // 4. File move event (Program Files count increase)
    const int pfDelta = snapshot.fsProgramFilesCount - prev.fsProgramFilesCount;
    if (pfDelta > 30) {
      headline = L"File move/copy event to Program Files.";
      insight = L"Program Files directory count increased by over 30, indicating installation.";
      diagnostics.push_back(L"Program Files: +" + std::to_wstring(pfDelta) + L".");
      risk += 8;
    }

    // 5. File overwrite event (recycle bin delta)
    if (snapshot.fsRecycleBinContentCount != prev.fsRecycleBinContentCount) {
      const int binDelta = snapshot.fsRecycleBinContentCount - prev.fsRecycleBinContentCount;
      if (binDelta > 50) {
        headline = L"File overwrite/replace burst.";
        insight = L"Recycle bin content increased by over 50, indicating mass file replacement.";
        diagnostics.push_back(L"Recycle bin: +" + std::to_wstring(binDelta) + L" items.");
        risk += 10;
      }
    }

    // 6. File truncation event (file count decrease in Program Files)
    if (prev.fsProgramFilesCount > snapshot.fsProgramFilesCount + 20) {
      headline = L"File truncation/removal in Program Files.";
      insight = L"Program Files directory count decreased by over 20.";
      diagnostics.push_back(L"Program Files: " + std::to_wstring(snapshot.fsProgramFilesCount - prev.fsProgramFilesCount) + L".");
      risk += 10;
    }

    // 7. Hidden file creation
    if (snapshot.fsSystem32HiddenCount > prev.fsSystem32HiddenCount + 3) {
      headline = L"Hidden file creation in System32.";
      insight = L"Hidden file count in System32 increased by more than 3.";
      diagnostics.push_back(L"Hidden: " + std::to_wstring(prev.fsSystem32HiddenCount) + L" -> " + std::to_wstring(snapshot.fsSystem32HiddenCount) + L".");
      risk += 12;
    }

    // 8. System file modification
    if (snapshot.fsSystem32SystemCount != prev.fsSystem32SystemCount && prev.fsSystem32SystemCount > 0) {
      const int sysDelta = snapshot.fsSystem32SystemCount - prev.fsSystem32SystemCount;
      headline = L"System file modification detected.";
      insight = L"System file count in System32 has changed.";
      diagnostics.push_back(L"System files: " + std::to_wstring(prev.fsSystem32SystemCount) + L" -> " + std::to_wstring(snapshot.fsSystem32SystemCount) + L".");
      risk += 16;
    }

    // 9. Hidden file in drivers
    if (snapshot.fsDriversHiddenCount > prev.fsDriversHiddenCount) {
      headline = L"Hidden file in drivers directory.";
      insight = L"Hidden file count in drivers directory has increased.";
      diagnostics.push_back(L"Hidden drivers: " + std::to_wstring(prev.fsDriversHiddenCount) + L" -> " + std::to_wstring(snapshot.fsDriversHiddenCount) + L".");
      risk += 14;
    }

    // 10. Volume dirty bit set
    if (snapshot.fsVolumeDirtyBit == 1 && prev.fsVolumeDirtyBit == 0) {
      headline = L"Volume dirty bit set.";
      insight = L"Filesystem dirty bit has been set, indicating unclean shutdown or corruption.";
      diagnostics.push_back(L"Volume dirty bit: set.");
      risk += 14;
    }

    // 11. Volume label change
    if (!snapshot.fsVolumeLabel.empty() && !prev.fsVolumeLabel.empty() && snapshot.fsVolumeLabel != prev.fsVolumeLabel) {
      headline = L"Volume label changed.";
      insight = L"Filesystem volume label has been modified.";
      diagnostics.push_back(L"Label: " + prev.fsVolumeLabel + L" -> " + snapshot.fsVolumeLabel + L".");
      risk += 10;
    }

    // 12. Mount point change
    if (snapshot.fsMountPointCount != 0) {
      diagnostics.push_back(L"Mount points: " + std::to_wstring(snapshot.fsMountPointCount) + L" active.");
    }

    // 13. Symbolic link/reparse point creation
    if (snapshot.fsReparsePointCount > prev.fsReparsePointCount + 2) {
      headline = L"Reparse point creation detected.";
      insight = L"Symbolic links or reparse points have been created in monitored directories.";
      diagnostics.push_back(L"Reparse: " + std::to_wstring(prev.fsReparsePointCount) + L" -> " + std::to_wstring(snapshot.fsReparsePointCount) + L".");
      risk += 12;
    }

    // 14. Sparse file creation
    if (snapshot.fsSparseFileCount > prev.fsSparseFileCount + 5) {
      headline = L"Sparse file creation detected.";
      insight = L"Additional sparse files created in System32.";
      diagnostics.push_back(L"Sparse: " + std::to_wstring(prev.fsSparseFileCount) + L" -> " + std::to_wstring(snapshot.fsSparseFileCount) + L".");
      risk += 8;
    }

    // 15. Alternate data stream creation
    if (snapshot.fsAdsWithDataCount > prev.fsAdsWithDataCount) {
      headline = L"Alternate data stream creation detected.";
      insight = L"ADS count on ntoskrnl.exe has increased, possibly indicating data hiding.";
      diagnostics.push_back(L"ADS: " + std::to_wstring(prev.fsAdsWithDataCount) + L" -> " + std::to_wstring(snapshot.fsAdsWithDataCount) + L".");
      risk += 14;
    }

    // 16. Filesystem corruption warning
    if (snapshot.fsVolumeDirtyBit == 1) {
      headline = L"Filesystem corruption warning.";
      insight = L"Volume dirty bit is set, indicating potential filesystem corruption.";
      diagnostics.push_back(L"FS corruption: dirty bit active.");
      risk += 16;
    }

    // 17. Chkdsk pending
    if (snapshot.fsChkdskPending == 1 && prev.fsChkdskPending == 0) {
      headline = L"Chkdsk pending.";
      insight = L"Filesystem dirty bit set, chkdsk should be run on next reboot.";
      diagnostics.push_back(L"Chkdsk: pending on C:.");
      risk += 12;
    }

    // 18. Directory enumeration burst
    if (fs32Delta > 100) {
      headline = L"Directory enumeration burst.";
      insight = L"Over 100 files added to System32 in one sample, indicating mass deployment.";
      diagnostics.push_back(L"Enum burst: +" + std::to_wstring(fs32Delta) + L" files.");
      risk += 14;
    }

    // 19. System file deletion
    if (prev.fsSystem32SystemCount > snapshot.fsSystem32SystemCount + 3) {
      headline = L"System file deletion detected.";
      insight = L"Multiple system files have been removed from System32.";
      diagnostics.push_back(L"Sys files removed: " + std::to_wstring(prev.fsSystem32SystemCount - snapshot.fsSystem32SystemCount) + L".");
      risk += 18;
    }

    // 20. File hash change (proxy: system file count + hidden count change)
    if (snapshot.fsSystem32SystemCount != prev.fsSystem32SystemCount && snapshot.fsSystem32HiddenCount != prev.fsSystem32HiddenCount) {
      headline = L"File integrity change detected.";
      insight = L"Both system and hidden file counts changed in System32, indicating possible hash modification.";
      diagnostics.push_back(L"Integrity: system delta + hidden delta in System32.");
      risk += 16;
    }

    // 21. Duplicate file burst
    if (fs32Delta > 50 && snapshot.fsSystem32HiddenCount == prev.fsSystem32HiddenCount) {
      headline = L"File duplicate burst detected.";
      insight = L"Many new files in System32 without hidden attribute change suggests duplication.";
      diagnostics.push_back(L"Dup burst: +" + std::to_wstring(fs32Delta) + L" non-hidden files.");
      risk += 10;
    }

    // 22. Sparse file creation in drivers
    if (snapshot.fsDriversHiddenCount > prev.fsDriversHiddenCount && snapshot.fsDriversFileCount == prev.fsDriversFileCount) {
      headline = L"Hidden file modification in drivers.";
      insight = L"Hidden file count changed in drivers without total count change.";
      diagnostics.push_back(L"Hidden drivers: " + std::to_wstring(prev.fsDriversHiddenCount) + L" -> " + std::to_wstring(snapshot.fsDriversHiddenCount) + L".");
      risk += 12;
    }

    // 23. Data integrity check failure
    if (snapshot.fsVolumeDirtyBit == 1 && snapshot.fsCorruptionWarnings > prev.fsCorruptionWarnings) {
      headline = L"Data integrity check failure.";
      insight = L"Volume corruption indicators increasing, data integrity at risk.";
      diagnostics.push_back(L"Integrity: dirty bit + corruption warnings.");
      risk += 18;
    }

    // 24. Recycle bin flush (mass deletion)
    if (prev.fsRecycleBinContentCount > snapshot.fsRecycleBinContentCount + 50) {
      headline = L"Recycle bin flush detected.";
      insight = L"Recycle bin emptied or mass deletion occurred.";
      diagnostics.push_back(L"Recycle bin: " + std::to_wstring(prev.fsRecycleBinContentCount) + L" -> " + std::to_wstring(snapshot.fsRecycleBinContentCount) + L".");
      risk += 8;
    }

    // 25. Large installation detected
    if (pfDelta > 100) {
      headline = L"Large installation detected.";
      insight = L"Program Files grew by over 100 files, indicating major software installation.";
      diagnostics.push_back(L"Install: +" + std::to_wstring(pfDelta) + L" files in Program Files.");
      risk += 6;
    }

    // --- Registry & Config (25 detections) ---

    // 1. Registry key create
    if (snapshot.regKeyCountRun > prevRegKeyCountRun_ + 1) {
      headline = L"Registry key creation in Run.";
      insight = L"New subkeys detected under the Run key, indicating persistence addition.";
      diagnostics.push_back(L"Run keys: " + std::to_wstring(prevRegKeyCountRun_) + L" -> " + std::to_wstring(snapshot.regKeyCountRun) + L".");
      risk += 10;
    }

    // 2. Registry key delete
    if (snapshot.regKeyCountRun < prevRegKeyCountRun_ - 1) {
      headline = L"Registry key deletion in Run.";
      insight = L"Subkeys removed from the Run key.";
      diagnostics.push_back(L"Run keys removed: " + std::to_wstring(prevRegKeyCountRun_ - snapshot.regKeyCountRun) + L".");
      risk += 12;
    }

    // 3. Registry value create
    if (snapshot.regValueCountRun > prevRegValueCountRun_ + 1) {
      headline = L"Registry value creation in Run.";
      insight = L"New values added to the Run key, possibly establishing persistence.";
      diagnostics.push_back(L"Run values: " + std::to_wstring(prevRegValueCountRun_) + L" -> " + std::to_wstring(snapshot.regValueCountRun) + L".");
      risk += 10;
    }

    // 4. Registry value delete
    if (snapshot.regValueCountRun < prevRegValueCountRun_ - 1) {
      headline = L"Registry value deletion in Run.";
      insight = L"Values removed from the Run key.";
      diagnostics.push_back(L"Run values removed: " + std::to_wstring(prevRegValueCountRun_ - snapshot.regValueCountRun) + L".");
      risk += 12;
    }

    // 5. Registry value modify
    if (snapshot.regHashRun != prevRegHashRun_ && snapshot.regValueCountRun == prevRegValueCountRun_) {
      headline = L"Registry value modification in Run.";
      insight = L"Value data changed in Run key without count change, indicating modification of existing persistence.";
      diagnostics.push_back(L"Run hash changed, values stable.");
      risk += 14;
    }

    // 6. Run key modification
    if (snapshot.regHashRun != prevRegHashRun_) {
      headline = L"Run key modification detected.";
      insight = L"The HKLM Run key hash has changed, indicating new or modified startup entries.";
      diagnostics.push_back(L"Run hash: " + std::to_wstring(prevRegHashRun_) + L" -> " + std::to_wstring(snapshot.regHashRun) + L".");
      risk += 12;
    }

    // 7. Shell open command change
    if (snapshot.regHashShell != prevRegHashShell_ && prevRegHashShell_ != 0) {
      headline = L"Shell open command changed.";
      insight = L"Registry shell\\open\\command key has been modified, which can intercept program execution.";
      diagnostics.push_back(L"Shell hash: " + std::to_wstring(prevRegHashShell_) + L" -> " + std::to_wstring(snapshot.regHashShell) + L".");
      risk += 16;
    }

    // 8. Startup folder change
    if (snapshot.startupFolderCount != prevStartupFolderCount_) {
      headline = L"Startup folder change detected.";
      insight = L"Number of items in the startup folder has changed.";
      diagnostics.push_back(L"Startup items: " + std::to_wstring(prevStartupFolderCount_) + L" -> " + std::to_wstring(snapshot.startupFolderCount) + L".");
      risk += 10;
    }

    // 9. Policy key modification
    if (snapshot.regHashPolicies != prevRegHashPolicies_ && prevRegHashPolicies_ != 0) {
      headline = L"Policy key modification detected.";
      insight = L"Group Policy registry keys have been modified.";
      diagnostics.push_back(L"Policies hash changed.");
      risk += 12;
    }

    // 10. Service config change
    if (snapshot.regHashServices != prevRegHashServices_ && prevRegHashServices_ != 0) {
      headline = L"Service configuration change.";
      insight = L"A service registry configuration has been modified.";
      diagnostics.push_back(L"Services hash: " + std::to_wstring(prevRegHashServices_) + L" -> " + std::to_wstring(snapshot.regHashServices) + L".");
      risk += 14;
    }

    // 11. Driver config change
    if (snapshot.regKeyCountDrivers != prevRegKeyCountDrivers_ && prevRegKeyCountDrivers_ != 0) {
      headline = L"Driver configuration change.";
      insight = L"The number of registered driver services has changed.";
      diagnostics.push_back(L"Driver services: " + std::to_wstring(prevRegKeyCountDrivers_) + L" -> " + std::to_wstring(snapshot.regKeyCountDrivers) + L".");
      risk += 16;
    }

    // 12. Telemetry config change
    if (snapshot.regHashTelemetry != prevRegHashTelemetry_ && prevRegHashTelemetry_ != 0) {
      headline = L"Telemetry configuration change.";
      insight = L"Windows telemetry/DiagTrack settings have been modified.";
      diagnostics.push_back(L"Telemetry hash changed.");
      risk += 10;
    }

    // 13. Audit policy change
    if (snapshot.regHashAudit != prevRegHashAudit_ && prevRegHashAudit_ != 0) {
      headline = L"Audit policy change.";
      insight = L"Local security audit policy registry keys have been modified.";
      diagnostics.push_back(L"Audit policy hash changed.");
      risk += 14;
    }

    // 14. Security policy change
    if (snapshot.regHashUac != prevRegHashUac_ && prevRegHashUac_ != 0) {
      headline = L"Security policy change.";
      insight = L"Security-related LSA registry values have been modified.";
      diagnostics.push_back(L"UAC/security policy hash changed.");
      risk += 16;
    }

    // 15. Firewall policy change
    if (snapshot.regHashFirewall != prevRegHashFirewall_ && prevRegHashFirewall_ != 0) {
      headline = L"Firewall policy change.";
      insight = L"Windows Firewall policy registry keys have been modified.";
      diagnostics.push_back(L"Firewall hash: " + std::to_wstring(prevRegHashFirewall_) + L" -> " + std::to_wstring(snapshot.regHashFirewall) + L".");
      risk += 18;
    }

    // 16. UAC policy change
    if (snapshot.regHashUac != prevRegHashUac_ && prevRegHashUac_ != 0) {
      headline = L"UAC policy change.";
      insight = L"User Account Control policy registry keys have been modified.";
      diagnostics.push_back(L"UAC hash: " + std::to_wstring(prevRegHashUac_) + L" -> " + std::to_wstring(snapshot.regHashUac) + L".");
      risk += 16;
    }

    // 17. Task scheduler config change
    if (snapshot.regHashTaskSched != prevRegHashTaskSched_ && prevRegHashTaskSched_ != 0) {
      headline = L"Task Scheduler configuration change.";
      insight = L"Task Scheduler registry cache has been modified.";
      diagnostics.push_back(L"TaskSched hash changed.");
      risk += 12;
    }

    // 18. COM registration change
    if (snapshot.regKeyCountCom != prevRegKeyCountCom_ && prevRegKeyCountCom_ != 0) {
      headline = L"COM registration change.";
      insight = L"Number of registered COM classes has changed.";
      diagnostics.push_back(L"COM CLSIDs: " + std::to_wstring(prevRegKeyCountCom_) + L" -> " + std::to_wstring(snapshot.regKeyCountCom) + L".");
      risk += 10;
    }

    // 19. Shell extension registration
    if (snapshot.regValueCountShellExt != prevRegValueCountShellExt_ && prevRegValueCountShellExt_ != 0) {
      headline = L"Shell extension registration change.";
      insight = L"Approved shell extensions list has been modified.";
      diagnostics.push_back(L"Shell extensions: " + std::to_wstring(prevRegValueCountShellExt_) + L" -> " + std::to_wstring(snapshot.regValueCountShellExt) + L".");
      risk += 10;
    }

    // 20. File association change
    if (snapshot.regHashFileAssoc != prevRegHashFileAssoc_ && prevRegHashFileAssoc_ != 0) {
      headline = L"File association change.";
      insight = L"File type associations have been modified.";
      diagnostics.push_back(L"File assoc hash changed.");
      risk += 12;
    }

    // 21. Default app change
    if (snapshot.regHashDefApp != prevRegHashDefApp_ && prevRegHashDefApp_ != 0) {
      headline = L"Default application change.";
      insight = L"Default application settings have been modified.";
      diagnostics.push_back(L"Default app hash changed.");
      risk += 8;
    }

    // 22. Environment variable change
    if (snapshot.envVarCount != prevEnvVarCount_) {
      headline = L"Environment variable change.";
      insight = L"Number of environment variables has changed.";
      diagnostics.push_back(L"Env vars: " + std::to_wstring(prevEnvVarCount_) + L" -> " + std::to_wstring(snapshot.envVarCount) + L".");
      risk += 10;
    }

    // 23. Path variable change
    if (snapshot.regHashEnvPath != prevRegHashEnvPath_ && prevRegHashEnvPath_ != 0) {
      headline = L"PATH variable change.";
      insight = L"The system PATH environment variable has been modified.";
      diagnostics.push_back(L"PATH hash: " + std::to_wstring(prevRegHashEnvPath_) + L" -> " + std::to_wstring(snapshot.regHashEnvPath) + L".");
      risk += 14;
    }

    // 24. Config file hash change
    if (snapshot.regHashConfigFile != prevRegHashConfigFile_ && prevRegHashConfigFile_ != 0) {
      headline = L"Configuration file hash change.";
      insight = L"A monitored configuration file has been modified.";
      diagnostics.push_back(L"Config hash changed.");
      risk += 12;
    }

    // 25. App association registry change
    if (snapshot.regHashAppAssoc != prevRegHashAppAssoc_ && prevRegHashAppAssoc_ != 0) {
      headline = L"Application association registry change.";
      insight = L"File extension application associations have been modified.";
      diagnostics.push_back(L"App assoc hash changed.");
      risk += 10;
    }

    // --- Audio & Multimedia (25 detections) ---

    // 1. Audio device connect
    if (snapshot.audioOutputDeviceCount > prevAudioOutputDeviceCount_ && prevAudioOutputDeviceCount_ > 0) {
      headline = L"Audio output device connected.";
      insight = L"New audio output device detected.";
      diagnostics.push_back(L"Outputs: " + std::to_wstring(prevAudioOutputDeviceCount_) + L" -> " + std::to_wstring(snapshot.audioOutputDeviceCount) + L".");
      risk += 4;
    }

    // 2. Audio device disconnect
    if (snapshot.audioOutputDeviceCount < prevAudioOutputDeviceCount_ && prevAudioOutputDeviceCount_ > 0) {
      headline = L"Audio output device disconnected.";
      insight = L"An audio output device has been removed.";
      diagnostics.push_back(L"Outputs: " + std::to_wstring(prevAudioOutputDeviceCount_) + L" -> " + std::to_wstring(snapshot.audioOutputDeviceCount) + L".");
      risk += 6;
    }

    // 3. Default output change
    if (snapshot.audioDeviceHash != prevAudioDeviceHash_ && prevAudioDeviceHash_ != 0) {
      headline = L"Default audio output device changed.";
      insight = L"The audio device topology has changed.";
      diagnostics.push_back(L"Device hash changed.");
      risk += 10;
    }

    // 4. Default input change
    if (snapshot.audioInputDeviceCount != prevAudioInputDeviceCount_ && prevAudioInputDeviceCount_ > 0) {
      headline = L"Audio input device count changed.";
      insight = L"The number of audio input devices has changed.";
      diagnostics.push_back(L"Inputs: " + std::to_wstring(prevAudioInputDeviceCount_) + L" -> " + std::to_wstring(snapshot.audioInputDeviceCount) + L".");
      risk += 8;
    }

    // 5. Sample rate change
    if (snapshot.audioSampleRate != prevAudioSampleRate_ && prevAudioSampleRate_ > 0) {
      headline = L"Audio sample rate changed.";
      insight = L"The audio sample rate configuration has been modified.";
      diagnostics.push_back(L"Sample rate bits: " + std::to_wstring(prevAudioSampleRate_) + L" -> " + std::to_wstring(snapshot.audioSampleRate) + L".");
      risk += 10;
    }

    // 6. Bit depth change
    if (snapshot.audioBitsPerSample != prevAudioBitsPerSample_ && prevAudioBitsPerSample_ > 0) {
      headline = L"Audio bit depth changed.";
      insight = L"The audio bit depth configuration has been modified.";
      diagnostics.push_back(L"Bit depth: " + std::to_wstring(prevAudioBitsPerSample_) + L" -> " + std::to_wstring(snapshot.audioBitsPerSample) + L".");
      risk += 10;
    }

    // 7. Channel count change
    if (snapshot.audioChannels != prevAudioChannels_ && prevAudioChannels_ > 0) {
      headline = L"Audio channel count changed.";
      insight = L"The audio channel configuration has been modified.";
      diagnostics.push_back(L"Channels: " + std::to_wstring(prevAudioChannels_) + L" -> " + std::to_wstring(snapshot.audioChannels) + L".");
      risk += 10;
    }

    // 8. Buffer underrun
    if (snapshot.audioLatencyMs > prevAudioLatencyMs_ + 50 && prevAudioLatencyMs_ > 0) {
      headline = L"Audio buffer underrun detected.";
      insight = L"Audio latency spiked, indicating buffer starvation.";
      diagnostics.push_back(L"Latency: " + std::to_wstring(prevAudioLatencyMs_) + L"ms -> " + std::to_wstring(snapshot.audioLatencyMs) + L"ms.");
      risk += 8;
    }

    // 9. Buffer overrun
    if (snapshot.audioLatencyMs < prevAudioLatencyMs_ - 50 && prevAudioLatencyMs_ > 100) {
      headline = L"Audio buffer behavior changed.";
      insight = L"Audio latency dropped significantly.";
      diagnostics.push_back(L"Latency: " + std::to_wstring(prevAudioLatencyMs_) + L"ms -> " + std::to_wstring(snapshot.audioLatencyMs) + L"ms.");
      risk += 4;
    }

    // 10. Audio service restart
    if (snapshot.audioServiceRunning != prevAudioServiceRunning_ && prevAudioServiceRunning_ >= 0) {
      headline = L"Audio service state changed.";
      insight = L"Windows Audio service (Audiosrv) state has changed.";
      diagnostics.push_back(L"Audiosrv: " + std::to_wstring(prevAudioServiceRunning_) + L" -> " + std::to_wstring(snapshot.audioServiceRunning) + L".");
      risk += 12;
    }

    // 11. Microphone mute
    if (snapshot.audioMicMuted == 1 && prevAudioMicMuted_ == 0) {
      headline = L"Microphone muted.";
      insight = L"The microphone has been muted.";
      diagnostics.push_back(L"Mic muted.");
      risk += 2;
    }

    // 12. Microphone unmute
    if (snapshot.audioMicMuted == 0 && prevAudioMicMuted_ == 1) {
      headline = L"Microphone unmuted.";
      insight = L"The microphone has been unmuted.";
      diagnostics.push_back(L"Mic unmuted.");
      risk += 2;
    }

    // 13. System mute
    if (snapshot.audioSystemMuted == 1 && prevAudioSystemMuted_ == 0) {
      headline = L"System audio muted.";
      insight = L"The system audio has been muted.";
      diagnostics.push_back(L"System muted.");
      risk += 2;
    }

    // 14. System unmute
    if (snapshot.audioSystemMuted == 0 && prevAudioSystemMuted_ == 1) {
      headline = L"System audio unmuted.";
      insight = L"The system audio has been unmuted.";
      diagnostics.push_back(L"System unmuted.");
      risk += 2;
    }

    // 15. Volume change
    if (snapshot.audioMasterVolume != prevAudioMasterVolume_ && prevAudioMasterVolume_ > 0) {
      headline = L"Master volume changed.";
      insight = L"The master volume level has been modified.";
      diagnostics.push_back(L"Volume: " + std::to_wstring(prevAudioMasterVolume_) + L" -> " + std::to_wstring(snapshot.audioMasterVolume) + L".");
      risk += 4;
    }

    // 16. Headphone jack insert
    if (snapshot.audioHeadphoneJack == 1 && prevAudioHeadphoneJack_ == 0) {
      headline = L"Headphone jack inserted.";
      insight = L"A headphone jack has been detected.";
      diagnostics.push_back(L"Headphone inserted.");
      risk += 2;
    }

    // 17. Headphone jack remove
    if (snapshot.audioHeadphoneJack == 0 && prevAudioHeadphoneJack_ == 1) {
      headline = L"Headphone jack removed.";
      insight = L"The headphone jack has been removed.";
      diagnostics.push_back(L"Headphone removed.");
      risk += 2;
    }

    // 18. Speaker anomaly
    if (snapshot.audioMasterVolumeLeft != prevAudioMasterVolumeLeft_ && prevAudioMasterVolumeLeft_ > 0 &&
        snapshot.audioMasterVolumeRight != prevAudioMasterVolumeRight_ && prevAudioMasterVolumeRight_ > 0 &&
        std::abs(static_cast<int>(snapshot.audioMasterVolumeLeft) - static_cast<int>(snapshot.audioMasterVolumeRight)) > 10000) {
      headline = L"Speaker volume imbalance detected.";
      insight = L"Left and right speaker volumes are significantly different.";
      diagnostics.push_back(L"L: " + std::to_wstring(snapshot.audioMasterVolumeLeft) + L" R: " + std::to_wstring(snapshot.audioMasterVolumeRight) + L".");
      risk += 6;
    }

    // 19. Codec driver event
    if (snapshot.audioCodecEvents != prevAudioCodecEvents_ && prevAudioCodecEvents_ >= 0) {
      headline = L"Audio codec driver event.";
      insight = L"An audio codec driver event has been detected.";
      diagnostics.push_back(L"Codec events: " + std::to_wstring(prevAudioCodecEvents_) + L" -> " + std::to_wstring(snapshot.audioCodecEvents) + L".");
      risk += 10;
    }

    // 20. Media playback start
    if (snapshot.audioWaveOutOpen > prevAudioWaveOutOpen_ && prevAudioWaveOutOpen_ >= 0) {
      headline = L"Media playback started.";
      insight = L"New audio output stream detected.";
      diagnostics.push_back(L"WaveOut: " + std::to_wstring(prevAudioWaveOutOpen_) + L" -> " + std::to_wstring(snapshot.audioWaveOutOpen) + L".");
      risk += 2;
    }

    // 21. Media playback stop
    if (snapshot.audioWaveOutOpen < prevAudioWaveOutOpen_ && prevAudioWaveOutOpen_ > 0) {
      headline = L"Media playback stopped.";
      insight = L"An audio output stream has been closed.";
      diagnostics.push_back(L"WaveOut: " + std::to_wstring(prevAudioWaveOutOpen_) + L" -> " + std::to_wstring(snapshot.audioWaveOutOpen) + L".");
      risk += 2;
    }

    // 22. Media decoder failure
    if (snapshot.audioDecoderErrors > prevAudioDecoderErrors_ && prevAudioDecoderErrors_ >= 0) {
      headline = L"Audio decoder failure.";
      insight = L"Audio decoder errors have been detected.";
      diagnostics.push_back(L"Decoder errors: " + std::to_wstring(prevAudioDecoderErrors_) + L" -> " + std::to_wstring(snapshot.audioDecoderErrors) + L".");
      risk += 12;
    }

    // 23. Audio latency spike
    if (snapshot.audioLatencyMs > prevAudioLatencyMs_ + 100 && prevAudioLatencyMs_ > 0) {
      headline = L"Audio latency spike detected.";
      insight = L"Audio latency has increased dramatically.";
      diagnostics.push_back(L"Latency: " + std::to_wstring(prevAudioLatencyMs_) + L"ms -> " + std::to_wstring(snapshot.audioLatencyMs) + L"ms.");
      risk += 10;
    }

    // 24. Recording permission change
    if (snapshot.audioWaveInOpen != prevAudioWaveInOpen_ && prevAudioWaveInOpen_ >= 0) {
      headline = L"Audio recording state changed.";
      insight = L"The number of active audio recording streams has changed.";
      diagnostics.push_back(L"WaveIn: " + std::to_wstring(prevAudioWaveInOpen_) + L" -> " + std::to_wstring(snapshot.audioWaveInOpen) + L".");
      risk += 8;
    }

    // 25. Audio spatialization change
    if (snapshot.audioSpatialization != prevAudioSpatialization_ && prevAudioSpatialization_ >= 0) {
      headline = L"Audio spatialization changed.";
      insight = L"Audio spatial sound configuration has been modified.";
      diagnostics.push_back(L"Spatial: " + std::to_wstring(prevAudioSpatialization_) + L" -> " + std::to_wstring(snapshot.audioSpatialization) + L".");
      risk += 8;
    }

    // --- Reliability & Recovery (25 detections) ---

    // 1. Application crash
    if (snapshot.crashEventsToday > prevCrashEventsToday_ && prevCrashEventsToday_ >= 0) {
      headline = L"Application crash detected.";
      insight = L"New crash dump files found in the system Minidump or CrashDumps directory.";
      diagnostics.push_back(L"Crashes today: " + std::to_wstring(prevCrashEventsToday_) + L" -> " + std::to_wstring(snapshot.crashEventsToday) + L".");
      risk += 18;
    }

    // 2. Unhandled exception
    if (snapshot.unhandledExceptionCount > prevUnhandledExceptionCount_ && prevUnhandledExceptionCount_ >= 0) {
      headline = L"Unhandled exception detected.";
      insight = L"An unhandled exception has been caught by the filter.";
      diagnostics.push_back(L"Unhandled: " + std::to_wstring(prevUnhandledExceptionCount_) + L" -> " + std::to_wstring(snapshot.unhandledExceptionCount) + L".");
      risk += 16;
    }

    // 3. Structured exception event
    if (snapshot.sehExceptionCount > prevSehExceptionCount_ && prevSehExceptionCount_ >= 0) {
      headline = L"Structured exception event.";
      insight = L"SEH exception handler was invoked by the vectored exception handler.";
      diagnostics.push_back(L"SEH: " + std::to_wstring(prevSehExceptionCount_) + L" -> " + std::to_wstring(snapshot.sehExceptionCount) + L".");
      risk += 14;
    }

    // 4. Stack overflow
    if (snapshot.stackOverflowCount > prevStackOverflowCount_ && prevStackOverflowCount_ >= 0) {
      headline = L"Stack overflow detected.";
      insight = L"A stack overflow exception was caught.";
      diagnostics.push_back(L"Stack overflows: " + std::to_wstring(snapshot.stackOverflowCount) + L".");
      risk += 20;
    }

    // 5. Heap corruption
    if (snapshot.heapCorruptionDetected > prevHeapCorruptionDetected_ && prevHeapCorruptionDetected_ >= 0) {
      headline = L"Heap corruption detected.";
      insight = L"Heap corruption exception was caught by the exception handler.";
      diagnostics.push_back(L"Heap corruption events: " + std::to_wstring(snapshot.heapCorruptionDetected) + L".");
      risk += 22;
    }

    // 6. Access violation
    if (snapshot.accessViolationCount > prevAccessViolationCount_ && prevAccessViolationCount_ >= 0) {
      headline = L"Access violation detected.";
      insight = L"An access violation exception was caught.";
      diagnostics.push_back(L"Access violations: " + std::to_wstring(prevAccessViolationCount_) + L" -> " + std::to_wstring(snapshot.accessViolationCount) + L".");
      risk += 16;
    }

    // 7. Assertion failure
    if (snapshot.assertionFailureCount > prevAssertionFailureCount_ && prevAssertionFailureCount_ >= 0) {
      headline = L"Assertion failure detected.";
      insight = L"An invalid parameter or pure virtual function call was detected.";
      diagnostics.push_back(L"Assertions: " + std::to_wstring(prevAssertionFailureCount_) + L" -> " + std::to_wstring(snapshot.assertionFailureCount) + L".");
      risk += 14;
    }

    // 8. Deadlock recovery
    if (snapshot.deadlockRecoveryCount > prevDeadlockRecoveryCount_ && prevDeadlockRecoveryCount_ >= 0) {
      headline = L"Deadlock recovery event.";
      insight = L"A potential deadlock condition was detected and recovered.";
      diagnostics.push_back(L"Deadlock recoveries: " + std::to_wstring(snapshot.deadlockRecoveryCount) + L".");
      risk += 16;
    }

    // 9. Watchdog reset
    if (snapshot.watchdogResetDetected > prevWatchdogResetDetected_ && prevWatchdogResetDetected_ >= 0) {
      headline = L"Watchdog reset detected.";
      insight = L"System watchdog timer reset was detected, indicating unresponsive processing.";
      diagnostics.push_back(L"Watchdog resets: " + std::to_wstring(snapshot.watchdogResetDetected) + L".");
      risk += 20;
    }

    // 10. Service recovery action
    if (snapshot.serviceRecoveryAction > prevServiceRecoveryAction_ && prevServiceRecoveryAction_ >= 0) {
      headline = L"Service recovery action triggered.";
      insight = L"A Windows service recovery action has been executed.";
      diagnostics.push_back(L"Service recoveries: " + std::to_wstring(snapshot.serviceRecoveryAction) + L".");
      risk += 14;
    }

    // 11. Process restart
    if (snapshot.processHash != prevProcessHash_ && prevProcessHash_ != 0) {
      headline = L"Process set changed.";
      insight = L"The process list hash has changed, indicating process creation or termination.";
      diagnostics.push_back(L"Process hash changed.");
      risk += 8;
    }

    // 12. Module reload
    if (snapshot.moduleHash != prevModuleHash_ && prevModuleHash_ != 0) {
      headline = L"Module reload detected.";
      insight = L"A loaded module list has changed, indicating DLL load or unload.";
      diagnostics.push_back(L"Module hash changed.");
      risk += 10;
    }

    // 13. UI freeze detection
    if (snapshot.uiResponsivenessMs > 200 && prevUiResponsivenessMs_ > 0) {
      headline = L"UI freeze detected.";
      insight = L"The main thread message pump took over 200ms to process.";
      diagnostics.push_back(L"UI latency: " + std::to_wstring(snapshot.uiResponsivenessMs) + L"ms.");
      risk += 12;
    }

    // 14. Hang detection
    if (snapshot.mainThreadResponsive == 0 && prevMainThreadResponsive_ == 1) {
      headline = L"Thread hang detected.";
      insight = L"The main thread is no longer responsive to message pump queries.";
      diagnostics.push_back(L"Main thread unresponsive.");
      risk += 16;
    }

    // 15. Timeout exceeded
    if (snapshot.uiResponsivenessMs > 500 && prevUiResponsivenessMs_ <= 500 && prevUiResponsivenessMs_ > 0) {
      headline = L"Timeout exceeded.";
      insight = L"Message processing timeout exceeded 500ms threshold.";
      diagnostics.push_back(L"Timeout: " + std::to_wstring(snapshot.uiResponsivenessMs) + L"ms.");
      risk += 14;
    }

    // 16. Retry storm
    if (snapshot.sehExceptionCount > prevSehExceptionCount_ + 5 && prevSehExceptionCount_ >= 0) {
      headline = L"Retry storm detected.";
      insight = L"Multiple SEH exceptions in rapid succession, indicating a failure loop.";
      diagnostics.push_back(L"SEH burst: +" + std::to_wstring(snapshot.sehExceptionCount - prevSehExceptionCount_) + L".");
      risk += 16;
    }

    // 17. Backoff escalation
    if (snapshot.timeoutExceeded > prevTimeoutExceeded_ + 3 && prevTimeoutExceeded_ >= 0) {
      headline = L"Backoff escalation detected.";
      insight = L"Multiple timeouts in sequence, indicating escalating delays.";
      diagnostics.push_back(L"Timeouts: " + std::to_wstring(prevTimeoutExceeded_) + L" -> " + std::to_wstring(snapshot.timeoutExceeded) + L".");
      risk += 12;
    }

    // 18. Circuit breaker open
    if (snapshot.circuitBreakerOpen > prevCircuitBreakerOpen_ && prevCircuitBreakerOpen_ >= 0) {
      headline = L"Circuit breaker opened.";
      insight = L"The failure threshold has been exceeded and the circuit breaker is open.";
      diagnostics.push_back(L"Circuit breaker: open.");
      risk += 10;
    }

    // 19. Circuit breaker close
    if (snapshot.circuitBreakerClose > prevCircuitBreakerClose_ && prevCircuitBreakerClose_ >= 0) {
      headline = L"Circuit breaker closed.";
      insight = L"The circuit breaker has recovered and is allowing operations.";
      diagnostics.push_back(L"Circuit breaker: closed.");
      risk += 4;
    }

    // 20. Fallback mode entry
    if (snapshot.fallbackModeEntry > prevFallbackModeEntry_ && prevFallbackModeEntry_ >= 0) {
      headline = L"Fallback mode entered.";
      insight = L"The system has entered a fallback or degraded mode.";
      diagnostics.push_back(L"Fallback mode: active.");
      risk += 10;
    }

    // 21. Fallback mode exit
    if (snapshot.fallbackModeExit > prevFallbackModeExit_ && prevFallbackModeExit_ >= 0) {
      headline = L"Fallback mode exited.";
      insight = L"The system has exited fallback mode and resumed normal operation.";
      diagnostics.push_back(L"Fallback mode: exited.");
      risk += 4;
    }

    // 22. Configuration rollback
    if (snapshot.configRollbackDetected > prevConfigRollbackDetected_ && prevConfigRollbackDetected_ >= 0) {
      headline = L"Configuration rollback detected.";
      insight = L"A configuration file has been rolled back to a previous version.";
      diagnostics.push_back(L"Config rollback events: " + std::to_wstring(snapshot.configRollbackDetected) + L".");
      risk += 14;
    }

    // 23. Safe mode entry
    if (snapshot.safeModeActive == 1 && prevSafeModeActive_ == 0) {
      headline = L"Safe mode entry detected.";
      insight = L"The system has entered safe mode.";
      diagnostics.push_back(L"Safe mode: active.");
      risk += 18;
    }

    // 24. Safe mode exit
    if (snapshot.safeModeActive == 0 && prevSafeModeActive_ == 1) {
      headline = L"Safe mode exit detected.";
      insight = L"The system has exited safe mode.";
      diagnostics.push_back(L"Safe mode: exited.");
      risk += 6;
    }

    // 25. Telemetry drop detection
    if (snapshot.telemetryDropDetected > prevTelemetryDropDetected_ && prevTelemetryDropDetected_ >= 0) {
      headline = L"Telemetry drop detected.";
      insight = L"Telemetry data collection gaps have been detected.";
      diagnostics.push_back(L"Telemetry drops: " + std::to_wstring(snapshot.telemetryDropDetected) + L".");
      risk += 10;
    }
  }

  while (diagnostics.size() < 6) {
    diagnostics.push_back(L"Passive correlation engine is waiting for more variance in telemetry.");
  }

  state_.scramHeadline = headline;
  state_.scramInsight = insight;
  state_.scramDiagnostics = diagnostics;
  state_.scramRiskScore = std::clamp(risk, 0, 100);
}

void MonixApp::SeedReferenceState() {
  state_.snapshot.host = L"MONIX";
  state_.snapshot.cpuPct = 18.0;
  state_.snapshot.gpuPct = 24.0;
  state_.snapshot.ramUsedBytes = 10ull * 1024ull * 1024ull * 1024ull;
  state_.snapshot.ramTotalBytes = 32ull * 1024ull * 1024ull * 1024ull;
  state_.snapshot.diskReadBytesPerSec = 62ull * 1024ull * 1024ull;
  state_.snapshot.diskWriteBytesPerSec = 8ull * 1024ull * 1024ull;
  state_.snapshot.netUpBytesPerSec = 3ull * 1024ull * 1024ull;
  state_.snapshot.netDownBytesPerSec = 10ull * 1024ull * 1024ull;
  state_.snapshot.inboundConnections = 28;
  state_.snapshot.outboundConnections = 116;
  state_.snapshot.dnsPseudo = 18;
  state_.snapshot.latencyMs = 24;
  state_.snapshot.cpuTempC = 47.2;
  state_.snapshot.cpuTempEstimated = true;
  state_.snapshot.gpuTempC = 42.8;
  state_.snapshot.gpuTempEstimated = true;
  state_.snapshot.storageTempC = 33.1;
  state_.snapshot.storageTempEstimated = true;
  state_.snapshot.processCount = 198;
  state_.snapshot.threadCount = 3344;
  state_.snapshot.handleCount = 60102;
  state_.snapshot.pageFileUsedBytes = 1700ull * 1024ull * 1024ull;
  state_.snapshot.pageFileTotalBytes = 4096ull * 1024ull * 1024ull;
  state_.snapshot.processorQueueLength = 1;
  state_.snapshot.contextSwitchesPerSec = 12200;
  state_.snapshot.systemCallsPerSec = 182000;
  state_.snapshot.interruptsPerSec = 1600;
  state_.snapshot.processorCount = 16;
  state_.snapshot.suspendedThreadCount = 120;
  state_.snapshot.readyThreadCount = 45;
  state_.snapshot.waitingThreadCount = 3100;
  state_.snapshot.realtimeThreadCount = 2;
  state_.snapshot.highPriorityThreadCount = 18;
  state_.snapshot.threadCreationDelta = 0;
  state_.snapshot.uptimeSeconds = 11ull * 3600ull + 24ull * 60ull;
  state_.snapshot.processes = {
    {L"Game.exe", 9210, 900, 1, 42.0, 5ull * 1024ull * 1024ull * 1024ull + 200ull * 1024ull * 1024ull, 88.0, 0, L"RUNNING", L"HIGH", L"P-000023FA-SEED"},
    {L"chrome.exe", 4210, 880, 1, 12.0, 2900ull * 1024ull * 1024ull, 4.0, 0, L"ACTIVE", L"NORMAL", L"P-00001072-SEED"},
    {L"discord.exe", 4912, 880, 1, 4.0, 512ull * 1024ull * 1024ull, 1.0, 0, L"ACTIVE", L"NORMAL", L"P-00001330-SEED"},
    {L"obs64.exe", 6216, 880, 1, 18.0, 1400ull * 1024ull * 1024ull, 22.0, 0, L"RECORDING", L"HIGH", L"P-00001848-SEED"},
    {L"explorer.exe", 1820, 744, 1, 1.0, 184ull * 1024ull * 1024ull, 0.0, 0, L"SYSTEM", L"NORMAL", L"P-0000071C-SEED"},
    {L"render_service.exe", 2600, 900, 1, 28.0, 2100ull * 1024ull * 1024ull, 35.0, 0, L"RUNNING", L"HIGH", L"P-00000A28-SEED"},
    {L"Monix.exe", 3000, 1820, 1, 2.0, 142ull * 1024ull * 1024ull, 3.0, 0, L"ACTIVE", L"NORMAL", L"P-00000BB8-SEED"},
    {L"System.exe", 4, 0, 0, 0.0, 88ull * 1024ull * 1024ull, 0.0, 0, L"KERNEL", L"HIGH", L"P-00000004-SEED"}
  };
  state_.snapshot.flows = {
    {L"discord.exe", 4, L"162.159.133.234:443", L"ACTIVE"},
    {L"Game.exe", 2, L"52.94.xxx.xxx", L"ACTIVE"},
    {L"chrome.exe", 6, L"multiple", L"ACTIVE"}
  };
  state_.knownFlowCounts = {
    {L"discord.exe", 4},
    {L"Game.exe", 2},
    {L"chrome.exe", 6}
  };
  state_.snapshot.netAdapterAddresses = {L"192.168.1.105", L"127.0.0.1"};
  state_.snapshot.netAdapterGateways = {L"192.168.1.1"};
  state_.snapshot.netAdapterSpeeds = {1000000000};
  state_.snapshot.netAdapterOperStatuses = {1};
  state_.snapshot.netAdapterTypes = {6};
  state_.snapshot.netPrimaryLinkSpeedBps = 1000000000;
  state_.snapshot.pingRttMs = 8;
  state_.snapshot.dnsResolutionMs = 12;
  state_.snapshot.dnsResolutionOk = 1;
  state_.snapshot.tcpRetransmits = 0;
  state_.snapshot.tcpResets = 12;
  state_.snapshot.routeTableHash = 0xABCD1234;
  state_.snapshot.proxyEnabled = 0;
  state_.snapshot.netAdapterCount = 2;
  state_.snapshot.udpConnectionCount = 18;

  state_.intro.active = config_.introEnabled;
  if (testMode_) { state_.loggedIn = true; state_.intro.active = false; }
  state_.trackedPid = 9210;
  PushLog(L"SYSTEM", L"SUCCESS", L"Runtime shell initialized with live tabs and full-screen log viewport.", ColorRole::Success);
  PushLog(L"ENGINE", L"INFO", L"VHS Gothic font loaded. OpenGL post-process path is active on the main window.", ColorRole::Primary);
  PushLog(L"CONFIG", L"INFO", L"Settings are writable live from the panel and persisted to monix.ini.", ColorRole::Success);
  PushLog(L"NETWORK", L"INFO", L"Socket flow monitor attached to the native collector.", ColorRole::Network);
  PushLog(L"SCRAM", L"INFO", L"Passive analysis engine prepared for hardware and kernel depth.", ColorRole::Scram);
  AppendHistoryPoint(state_.snapshot);
  UpdateScramSummary(state_.snapshot);
}

void MonixApp::ConsumeSnapshot(Snapshot snapshot) {
  const bool hadPrevious = state_.hasPreviousSnapshot;
  state_.previousSnapshot = std::make_unique<Snapshot>(std::move(state_.snapshot));
  state_.hasPreviousSnapshot = true;
  AppendHistoryPoint(snapshot);
  ++state_.sampleCount;
  const auto& previous = *state_.previousSnapshot;

  if (!state_.initialized) {
    PushLog(L"SYSTEM", L"SUCCESS", L"Collector synchronized with native render surface.", ColorRole::Success);
    PushLog(L"KERNEL", L"INFO", L"Kernel counters and thermal matrix are online.", ColorRole::Kernel);
    state_.initialized = true;
  }

  std::map<int, std::wstring> currentProcesses;
  for (const auto& process : snapshot.processes) {
    currentProcesses.emplace(process.pid, process.name);
  }

  constexpr int kDwellThreshold = 4;
  constexpr int kGoneThreshold = 4;

  for (const auto& entry : currentProcesses) {
    const int pid = entry.first;
    const std::wstring& name = entry.second;
    if (state_.knownProcesses.find(pid) != state_.knownProcesses.end()) {
      state_.processSeenCount[pid] = 0;
      state_.processGoneCount.erase(pid);
    } else {
      int& seen = state_.processSeenCount[pid];
      ++seen;
      if (seen >= kDwellThreshold && name != L"SYSTEM" && name != L"KERNEL") {
        PushLog(L"PROCESS", L"INFO", name + L" entered the active task set (PID " + std::to_wstring(pid) + L")", ColorRole::Success, L"process", L"lifecycle");
        state_.knownProcesses[pid] = name;
        seen = 0;
      }
    }
  }

  for (auto it = state_.knownProcesses.begin(); it != state_.knownProcesses.end(); ) {
    if (currentProcesses.find(it->first) == currentProcesses.end()) {
      int& gone = state_.processGoneCount[it->first];
      ++gone;
      if (gone >= kGoneThreshold) {
        PushLog(L"PROCESS", L"WARNING", it->second + L" left the active task set (PID " + std::to_wstring(it->first) + L")", ColorRole::Warning, L"process", L"lifecycle");
        state_.processSeenCount.erase(it->first);
        it = state_.knownProcesses.erase(it);
        gone = 0;
      } else {
        ++it;
      }
    } else {
      state_.processGoneCount.erase(it->first);
      ++it;
    }
  }

  if (!state_.processEventsInitialized && !currentProcesses.empty()) {
    state_.knownProcesses = currentProcesses;
    state_.processSeenCount.clear();
    state_.processGoneCount.clear();
    state_.processEventsInitialized = true;
  }

  const double ramPct = snapshot.ramTotalBytes == 0 ? 0.0 :
    (static_cast<double>(snapshot.ramUsedBytes) / static_cast<double>(snapshot.ramTotalBytes)) * 100.0;
  const double previousRamPct = previous.ramTotalBytes == 0 ? 0.0 :
    (static_cast<double>(previous.ramUsedBytes) / static_cast<double>(previous.ramTotalBytes)) * 100.0;

  if ((!hadPrevious || previous.cpuPct < 78.0) && snapshot.cpuPct >= 78.0 && !snapshot.processes.empty()) {
    const auto topIt = std::max_element(snapshot.processes.begin(), snapshot.processes.end(),
      [](const ProcessInfo& a, const ProcessInfo& b) { return a.cpuPct < b.cpuPct; });
    if (topIt != snapshot.processes.end() && topIt->cpuPct >= 5.0) {
      const bool dominant = topIt->cpuPct > (snapshot.cpuPct * 0.4);
      std::wstring msg = L"CPU pressure at " + FormatPercent(snapshot.cpuPct) + L" \xe2\x80\x94 ";
      if (dominant) {
        msg += topIt->name + L" (PID " + std::to_wstring(topIt->pid) + L") " + FormatPercent(topIt->cpuPct) + L" dominant";
      } else {
        msg += L"top: " + topIt->name + L" " + FormatPercent(topIt->cpuPct) + L", no single dominant process";
      }
      msg += L" | threshold 78%";
      PushLog(L"CPU", L"WARNING", msg, ColorRole::Warning);
    }
  }

  if ((!hadPrevious || previous.gpuPct < 85.0) && snapshot.gpuPct >= 85.0) {
    PushLog(L"GPU", L"WARNING", L"GPU utilization crossed the hot path threshold.", ColorRole::Warning);
  }

  if (hadPrevious) {
    if (!snapshot.gpuDriverVersion.empty() && snapshot.gpuDriverVersion != previous.gpuDriverVersion && !previous.gpuDriverVersion.empty()) {
      PushLog(L"GPU", L"ERROR", L"GPU driver version changed: " + previous.gpuDriverVersion + L" -> " + snapshot.gpuDriverVersion, ColorRole::Error, L"gpu", L"driver", L"event=driver_change");
    }
    if ((snapshot.displayWidth != previous.displayWidth || snapshot.displayHeight != previous.displayHeight) && previous.displayWidth > 0) {
      PushLog(L"DISPLAY", L"ERROR", L"Resolution changed: " + std::to_wstring(previous.displayWidth) + L"x" + std::to_wstring(previous.displayHeight) + L" -> " + std::to_wstring(snapshot.displayWidth) + L"x" + std::to_wstring(snapshot.displayHeight), ColorRole::Error, L"display", L"resolution", L"event=res_change");
    }
    if (snapshot.displayRefreshRateHz != previous.displayRefreshRateHz && previous.displayRefreshRateHz > 0) {
      PushLog(L"DISPLAY", L"ERROR", L"Refresh rate changed: " + std::to_wstring(previous.displayRefreshRateHz) + L"Hz -> " + std::to_wstring(snapshot.displayRefreshRateHz) + L"Hz", ColorRole::Error, L"display", L"refresh", L"event=refresh_change");
    }
    if (snapshot.displayMonitorCount != previous.displayMonitorCount && previous.displayMonitorCount > 0) {
      PushLog(L"DISPLAY", L"INFO", L"Monitor topology changed: " + std::to_wstring(previous.displayMonitorCount) + L" -> " + std::to_wstring(snapshot.displayMonitorCount), ColorRole::Network, L"display", L"monitor", L"event=monitor_change");
    }
    if (snapshot.hdrEnabled != previous.hdrEnabled && previous.hdrEnabled >= 0) {
      PushLog(L"DISPLAY", L"ERROR", L"HDR state toggled: " + std::to_wstring(previous.hdrEnabled ? 1 : 0) + L" -> " + std::to_wstring(snapshot.hdrEnabled ? 1 : 0), ColorRole::Error, L"display", L"hdr", L"event=hdr_change");
    }
    if (snapshot.frameTimeMs > 50.0 && previous.frameTimeMs > 0 && previous.frameTimeMs < 20.0) {
      PushLog(L"GPU", L"WARNING", L"Frame time spike: " + std::to_wstring((int)previous.frameTimeMs) + L"ms -> " + std::to_wstring((int)snapshot.frameTimeMs) + L"ms", ColorRole::Warning, L"gpu", L"render", L"event=frame_spike");
    }
    if (snapshot.desktopCompositionEnabled != previous.desktopCompositionEnabled && previous.desktopCompositionEnabled >= 0) {
      PushLog(L"DISPLAY", L"ERROR", L"Desktop composition toggled: " + std::to_wstring(previous.desktopCompositionEnabled ? 1 : 0) + L" -> " + std::to_wstring(snapshot.desktopCompositionEnabled ? 1 : 0), ColorRole::Error, L"display", L"dwm", L"event=dwm_change");
    }
  }

  if ((!hadPrevious || previous.storageTempC < 55.0) && snapshot.storageTempC >= 55.0) {
    PushLog(L"DISK", L"WARNING", L"Storage temperature climbed into the warm envelope.", ColorRole::Warning);
  }

  std::map<std::wstring, int> currentFlowCounts;
  for (const auto& flow : snapshot.flows) {
    if (!flow.name.empty()) {
      currentFlowCounts[flow.name] = flow.activeConnections;
    }
  }

  if ((!hadPrevious || previous.latencyMs < 60) && snapshot.latencyMs >= 60) {
    PushLog(L"NETWORK", L"ERROR", L"Latency crossed the degraded threshold at " + std::to_wstring(snapshot.latencyMs) + L"ms.", ColorRole::Error, L"network", L"collector", L"latency=high");
  }

  if ((!hadPrevious || previous.outboundConnections < 240) && snapshot.outboundConnections >= 240) {
    PushLog(L"NETWORK", L"ERROR", L"Outbound socket fan-out crossed 240 active connections.", ColorRole::Error, L"network", L"collector", L"sockets=high");
  }

  if (hadPrevious && (std::abs(snapshot.latencyMs - previous.latencyMs) >= 12 || std::abs(snapshot.outboundConnections - previous.outboundConnections) >= 24)) {
    PushLog(
      L"NETWORK",
      L"INFO",
      L"Traffic profile changed: latency " + std::to_wstring(previous.latencyMs) + L"->" + std::to_wstring(snapshot.latencyMs) +
        L"ms, established " + std::to_wstring(previous.outboundConnections) + L"->" + std::to_wstring(snapshot.outboundConnections) + L".",
      ColorRole::Network,
      L"network",
      L"collector",
      L"delta=true"
    );
  }

  for (const auto& entry : currentFlowCounts) {
    const std::wstring& name = entry.first;
    const int connections = entry.second;
    const int previousConnections = state_.knownFlowCounts.find(name) != state_.knownFlowCounts.end() ? state_.knownFlowCounts[name] : -1;
    if (previousConnections == -1 && connections > 0) {
      PushLog(L"NETWORK", L"INFO", name + L" opened " + std::to_wstring(connections) + L" connections.", ColorRole::Network, L"network", L"flows", L"flow=enter");
    } else if (previousConnections >= 0 && std::abs(connections - previousConnections) >= 3) {
      const ULONGLONG nowMs = GetTickCount64();
      if (state_.flowFirstDeltaMs.find(name) != state_.flowFirstDeltaMs.end()) {
        const ULONGLONG burstStart = state_.flowFirstDeltaMs[name];
        if (nowMs - burstStart < 2000) {
          state_.flowBurstEnd[name] = connections;
          continue;
        }
      }
      int startConns = state_.flowBurstStart.find(name) != state_.flowBurstStart.end() ? state_.flowBurstStart[name] : previousConnections;
      int endConns = state_.flowBurstEnd.find(name) != state_.flowBurstEnd.end() ? state_.flowBurstEnd[name] : connections;
      if (startConns != endConns) {
        const wchar_t* arrow = endConns > startConns ? L"\xe2\x86\x91" : L"\xe2\x86\x93";
        PushLog(L"NETWORK", L"INFO", name + L" sockets: " + std::to_wstring(startConns) + L" " + std::wstring(arrow) + L" " + std::to_wstring(endConns), ColorRole::Network, L"network", L"flows", L"flow=burst");
      } else {
        PushLog(L"NETWORK", L"INFO", name + L" socket load: " + std::to_wstring(previousConnections) + L" \xe2\x86\x92 " + std::to_wstring(connections) + L".", ColorRole::Network, L"network", L"flows", L"flow=delta");
      }
      state_.flowFirstDeltaMs.erase(name);
      state_.flowBurstStart.erase(name);
      state_.flowBurstEnd.erase(name);
    }
  }

  std::vector<std::wstring> toRemove;
  for (const auto& entry : state_.flowFirstDeltaMs) {
    const std::wstring& name = entry.first;
    if (currentFlowCounts.find(name) == currentFlowCounts.end()) {
      toRemove.push_back(name);
    }
  }
  for (const auto& name : toRemove) {
    state_.flowFirstDeltaMs.erase(name);
    state_.flowBurstStart.erase(name);
    state_.flowBurstEnd.erase(name);
  }

  for (const auto& entry : currentFlowCounts) {
    const std::wstring& name = entry.first;
    const int connections = entry.second;
    const int previousConnections = state_.knownFlowCounts.find(name) != state_.knownFlowCounts.end() ? state_.knownFlowCounts[name] : -1;
    if (previousConnections >= 0 && std::abs(connections - previousConnections) >= 3 && state_.flowFirstDeltaMs.find(name) == state_.flowFirstDeltaMs.end()) {
      state_.flowFirstDeltaMs[name] = GetTickCount64();
      state_.flowBurstStart[name] = previousConnections;
      state_.flowBurstEnd[name] = connections;
    }
  }

  state_.knownFlowCounts = currentFlowCounts;

  if (hadPrevious) {
    for (const auto& addr : snapshot.netAdapterAddresses) {
      if (previous.netAdapterAddresses.find(addr) == previous.netAdapterAddresses.end()) {
        PushLog(L"KERNEL", L"WARNING", L"IP address changed: new " + addr, ColorRole::Kernel, L"network", L"adapter", L"event=ip_change");
      }
    }
    for (const auto& addr : previous.netAdapterAddresses) {
      if (snapshot.netAdapterAddresses.find(addr) == snapshot.netAdapterAddresses.end()) {
        PushLog(L"KERNEL", L"WARNING", L"IP address removed: " + addr, ColorRole::Kernel, L"network", L"adapter", L"event=ip_removed");
      }
    }
    if (previous.netAdapterOperStatuses.size() > 0 && snapshot.netAdapterOperStatuses.size() == 0) {
      PushLog(L"KERNEL", L"ERROR", L"Network link down: all adapters went offline.", ColorRole::Error, L"network", L"adapter", L"event=link_down");
    } else if (previous.netAdapterOperStatuses.size() == 0 && snapshot.netAdapterOperStatuses.size() > 0) {
      PushLog(L"KERNEL", L"INFO", L"Network link up: adapter became operational.", ColorRole::Network, L"network", L"adapter", L"event=link_up");
    }
    if (previous.routeTableHash != 0 && snapshot.routeTableHash != 0 && snapshot.routeTableHash != previous.routeTableHash) {
      PushLog(L"KERNEL", L"WARNING", L"Route table changed.", ColorRole::Kernel, L"network", L"routing", L"event=route_change");
    }
    if (previous.proxyEnabled != snapshot.proxyEnabled) {
      PushLog(L"KERNEL", L"ERROR", L"Proxy configuration changed.", ColorRole::Error, L"network", L"adapter", L"event=proxy_change");
    }
    if (!previous.vpnAdapterDescriptions.empty() && snapshot.vpnAdapterDescriptions.empty()) {
      PushLog(L"KERNEL", L"WARNING", L"VPN disconnect: tunnel adapter removed.", ColorRole::Kernel, L"network", L"adapter", L"event=vpn_disconnect");
    } else if (previous.vpnAdapterDescriptions.empty() && !snapshot.vpnAdapterDescriptions.empty()) {
      PushLog(L"KERNEL", L"INFO", L"VPN connect: tunnel adapter activated.", ColorRole::Network, L"network", L"adapter", L"event=vpn_connect");
    }
    if (snapshot.pingRttMs < 0 && previous.pingRttMs >= 0) {
      PushLog(L"KERNEL", L"ERROR", L"Gateway unreachable: ICMP probe failed.", ColorRole::Error, L"network", L"probe", L"event=gateway_down");
    }
    if (snapshot.dnsResolutionOk == 0 && previous.dnsResolutionOk == 1) {
      PushLog(L"KERNEL", L"ERROR", L"DNS resolution failure: probe to google.com failed.", ColorRole::Error, L"network", L"probe", L"event=dns_fail");
    }
    if (snapshot.tcpResets > previous.tcpResets && previous.tcpResets > 0) {
      const uint64_t resetDelta = snapshot.tcpResets - previous.tcpResets;
      if (resetDelta > 50) {
        PushLog(L"KERNEL", L"WARNING", L"TCP reset spike: +" + std::to_wstring(resetDelta) + L" in one sample.", ColorRole::Kernel, L"network", L"tcp", L"event=reset_spike");
      }
    }
    if (snapshot.tcpRetransmits > previous.tcpRetransmits && previous.tcpRetransmits > 0) {
      const uint64_t retransDelta = snapshot.tcpRetransmits - previous.tcpRetransmits;
      if (retransDelta > 40) {
        PushLog(L"KERNEL", L"WARNING", L"TCP retransmission spike: +" + std::to_wstring(retransDelta), ColorRole::Kernel, L"network", L"tcp", L"event=retrans_spike");
      }
    }
    const int udpDelta = snapshot.udpConnectionCount - previous.udpConnectionCount;
    if (udpDelta < -40) {
      PushLog(L"KERNEL", L"WARNING", L"UDP loss spike: " + std::to_wstring(-udpDelta) + L" connections dropped.", ColorRole::Kernel, L"network", L"udp", L"event=udp_loss");
    }
  }

  if (hadPrevious) {
    const int threadDelta = snapshot.threadCount - previous.threadCount;
    if (threadDelta > 100) {
      PushLog(L"KERNEL", L"WARNING", L"Thread creation burst: +" + std::to_wstring(threadDelta) + L" threads.", ColorRole::Kernel, L"scheduler", L"thread", L"event=thread_create");
    } else if (threadDelta < -100) {
      PushLog(L"KERNEL", L"WARNING", L"Thread termination burst: " + std::to_wstring(threadDelta) + L" threads.", ColorRole::Kernel, L"scheduler", L"thread", L"event=thread_term");
    }
    if (snapshot.contextSwitchesPerSec > 50000) {
      PushLog(L"KERNEL", L"WARNING", L"Context switch spike: " + std::to_wstring(snapshot.contextSwitchesPerSec) + L"/s.", ColorRole::Kernel, L"scheduler", L"context", L"event=cs_spike");
    }
    if (snapshot.processorQueueLength >= 6) {
      PushLog(L"KERNEL", L"ERROR", L"Ready queue buildup: depth " + std::to_wstring(snapshot.processorQueueLength) + L".", ColorRole::Error, L"scheduler", L"queue", L"event=queue_buildup");
    }
    if (snapshot.processorQueueLength >= 4 && snapshot.cpuPct < 40.0) {
      PushLog(L"KERNEL", L"ERROR", L"CPU starvation: queue " + std::to_wstring(snapshot.processorQueueLength) + L" at " + std::to_wstring((int)snapshot.cpuPct) + L"% CPU.", ColorRole::Error, L"scheduler", L"starvation", L"event=starvation");
    }
    if (snapshot.interruptsPerSec > 8000) {
      PushLog(L"KERNEL", L"WARNING", L"High DPC/ISR latency: " + std::to_wstring(snapshot.interruptsPerSec) + L" interrupts/s.", ColorRole::Kernel, L"scheduler", L"dpc", L"event=dpc_spike");
    }
    if (snapshot.readyThreadCount > 800 && previous.readyThreadCount <= 800) {
      PushLog(L"KERNEL", L"WARNING", L"Wait chain anomaly: " + std::to_wstring(snapshot.readyThreadCount) + L" ready threads.", ColorRole::Kernel, L"scheduler", L"wait", L"event=wait_anomaly");
    }
    const int rtDelta = snapshot.realtimeThreadCount - previous.realtimeThreadCount;
    if (rtDelta > 5) {
      PushLog(L"KERNEL", L"ERROR", L"Thread priority change: RT threads " + std::to_wstring(previous.realtimeThreadCount) + L" -> " + std::to_wstring(snapshot.realtimeThreadCount) + L".", ColorRole::Error, L"scheduler", L"priority", L"event=priority_change");
    }
  }

  if (hadPrevious) {
    for (const auto& drv : snapshot.driverNames) {
      if (previous.driverNames.find(drv) == previous.driverNames.end()) {
        PushLog(L"KERNEL", L"INFO", L"Driver load: " + drv, ColorRole::Kernel, L"os", L"driver", L"event=driver_load");
      }
    }
    for (const auto& drv : previous.driverNames) {
      if (snapshot.driverNames.find(drv) == snapshot.driverNames.end()) {
        PushLog(L"KERNEL", L"INFO", L"Driver unload: " + drv, ColorRole::Kernel, L"os", L"driver", L"event=driver_unload");
      }
    }
    if (previous.sessionCount > snapshot.sessionCount && snapshot.sessionCount > 0) {
      PushLog(L"KERNEL", L"INFO", L"Session end: " + std::to_wstring(previous.sessionCount) + L" -> " + std::to_wstring(snapshot.sessionCount) + L".", ColorRole::Kernel, L"os", L"session", L"event=session_end");
    }
    if (snapshot.sessionCount > previous.sessionCount && previous.sessionCount > 0) {
      PushLog(L"KERNEL", L"INFO", L"Session start: " + std::to_wstring(previous.sessionCount) + L" -> " + std::to_wstring(snapshot.sessionCount) + L".", ColorRole::Kernel, L"os", L"session", L"event=session_start");
    }
    if (previous.sessionCount > 0 && snapshot.sessionCount == 0) {
      PushLog(L"KERNEL", L"ERROR", L"Critical session collapse: all sessions terminated.", ColorRole::Error, L"os", L"session", L"event=session_collapse");
    }
    if (snapshot.uptimeMs < previous.uptimeMs && previous.uptimeMs > 60000) {
      PushLog(L"KERNEL", L"ERROR", L"Unexpected reboot detected: uptime " + std::to_wstring(previous.uptimeMs / 1000) + L"s -> " + std::to_wstring(snapshot.uptimeMs / 1000) + L"s.", ColorRole::Error, L"os", L"boot", L"event=unexpected_reboot");
    }
    if (previous.systemTime100ns > 0 && snapshot.systemTime100ns > 0) {
      const long long timeDelta = static_cast<long long>(snapshot.systemTime100ns - previous.systemTime100ns);
      const long long expectedDelta = (snapshot.uptimeMs - previous.uptimeMs) * 10000LL;
      const long long drift = timeDelta - expectedDelta;
      if (drift > 100000000LL || drift < -100000000LL) {
        PushLog(L"KERNEL", L"WARNING", L"System time change: drift " + std::to_wstring(drift / 10000) + L"ms.", ColorRole::Kernel, L"os", L"time", L"event=time_change");
      }
    }
  }

  if (hadPrevious) {
    for (const auto& drv : snapshot.unsignedDriverNames) {
      if (previous.unsignedDriverNames.find(drv) == previous.unsignedDriverNames.end()) {
        PushLog(L"SECURITY", L"ERROR", L"Unsigned driver load: " + drv, ColorRole::Error, L"security", L"driver", L"event=unsigned_driver");
      }
    }
    if (previous.selfSignatureValid == 1 && snapshot.selfSignatureValid == 0) {
      PushLog(L"SECURITY", L"ERROR", L"Executable signature became invalid: " + snapshot.selfExePath, ColorRole::Error, L"security", L"signature", L"event=sig_invalid");
    }
    if (previous.selfHashVerified == 1 && snapshot.selfHashVerified == 0) {
      PushLog(L"SECURITY", L"ERROR", L"Executable hash tampered.", ColorRole::Error, L"security", L"hash", L"event=hash_tamper");
    }
    for (const auto& mod : snapshot.suspiciousModules) {
      if (previous.suspiciousModules.find(mod) == previous.suspiciousModules.end()) {
        PushLog(L"SECURITY", L"WARNING", L"Suspicious module loaded: " + mod, ColorRole::Kernel, L"security", L"module", L"event=suspicious_module");
      }
    }
    if (snapshot.suspiciousScriptHosts > previous.suspiciousScriptHosts) {
      PushLog(L"SECURITY", L"WARNING", L"Script host process detected: " + std::to_wstring(snapshot.suspiciousScriptHosts) + L" active.", ColorRole::Kernel, L"security", L"script", L"event=script_host");
    }
    if (snapshot.uacConsentProcesses > previous.uacConsentProcesses) {
      PushLog(L"SECURITY", L"INFO", L"UAC prompt: consent.exe active.", ColorRole::Kernel, L"security", L"uac", L"event=uac_prompt");
    }
    if (snapshot.lsassAccessCount > previous.lsassAccessCount) {
      PushLog(L"SECURITY", L"ERROR", L"LSASS access increase detected: " + std::to_wstring(snapshot.lsassAccessCount) + L" references.", ColorRole::Error, L"security", L"lsass", L"event=lsass_access");
    }
    if (snapshot.hookModulesDetected > previous.hookModulesDetected) {
      PushLog(L"SECURITY", L"WARNING", L"Hook module detected: " + std::to_wstring(snapshot.hookModulesDetected) + L" active.", ColorRole::Kernel, L"security", L"hook", L"event=hook_detected");
    }
    if (snapshot.peHeaderTamper > previous.peHeaderTamper) {
      PushLog(L"SECURITY", L"ERROR", L"PE header tamper detected: " + std::to_wstring(snapshot.peHeaderTamper) + L" module(s).", ColorRole::Error, L"security", L"pe", L"event=pe_tamper");
    }
    if (snapshot.scheduledTaskCount > previous.scheduledTaskCount + 5) {
      PushLog(L"SECURITY", L"INFO", L"Scheduled tasks increased: " + std::to_wstring(previous.scheduledTaskCount) + L" -> " + std::to_wstring(snapshot.scheduledTaskCount) + L".", ColorRole::Kernel, L"security", L"tasks", L"event=new_tasks");
    }
  }

  if (hadPrevious) {
    if (previous.acLineStatus == 0 && snapshot.acLineStatus == 1) {
      PushLog(L"POWER", L"INFO", L"AC power connected.", ColorRole::Success, L"power", L"ac", L"event=ac_connect");
    }
    if (previous.acLineStatus == 1 && snapshot.acLineStatus == 0) {
      PushLog(L"POWER", L"WARNING", L"AC power disconnected.", ColorRole::Kernel, L"power", L"ac", L"event=ac_disconnect");
    }
    if (previous.batteryFlag == 128 && snapshot.batteryFlag != 128) {
      PushLog(L"POWER", L"INFO", L"Battery detected.", ColorRole::Success, L"power", L"battery", L"event=battery_present");
    }
    if (previous.batteryFlag != 128 && snapshot.batteryFlag == 128) {
      PushLog(L"POWER", L"WARNING", L"Battery removed.", ColorRole::Kernel, L"power", L"battery", L"event=battery_removed");
    }
    if (snapshot.batteryLifePercent >= 0 && snapshot.batteryLifePercent <= 10 && snapshot.acLineStatus == 0) {
      PushLog(L"POWER", L"ERROR", L"Battery critical: " + std::to_wstring(snapshot.batteryLifePercent) + L"% remaining.", ColorRole::Error, L"power", L"battery", L"event=battery_critical");
    }
    if (snapshot.powerPlanIndex != previous.powerPlanIndex && previous.powerPlanIndex >= 0 && snapshot.powerPlanIndex >= 0) {
      const wchar_t* plans[] = { L"Power Saver", L"Balanced", L"High Performance" };
      PushLog(L"POWER", L"WARNING", L"Power plan changed: " + std::wstring(plans[previous.powerPlanIndex]) + L" -> " + std::wstring(plans[snapshot.powerPlanIndex]) + L".", ColorRole::Kernel, L"power", L"plan", L"event=plan_change");
    }
    if (snapshot.highPerfActive == 1 && previous.highPerfActive == 0) {
      PushLog(L"POWER", L"WARNING", L"High performance mode activated.", ColorRole::Kernel, L"power", L"plan", L"event=high_perf");
    }
    if (snapshot.batteryWearLevel >= 0 && snapshot.batteryWearLevel > 30 && previous.batteryWearLevel >= 0 && snapshot.batteryWearLevel > previous.batteryWearLevel) {
      PushLog(L"POWER", L"INFO", L"Battery wear increased: " + std::to_wstring(snapshot.batteryWearLevel) + L"%.", ColorRole::Kernel, L"power", L"battery", L"event=battery_wear");
    }
    if (snapshot.batteryTemperature >= 0 && previous.batteryTemperature >= 0 && snapshot.batteryTemperature > previous.batteryTemperature + 10) {
      PushLog(L"POWER", L"WARNING", L"Battery temperature rising: " + std::to_wstring(snapshot.batteryTemperature) + L"C.", ColorRole::Kernel, L"power", L"battery", L"event=battery_temp");
    }
  }

  if (hadPrevious) {
    if (snapshot.cpuTempC > previous.cpuTempC + 10.0 && snapshot.cpuTempC > 50.0) {
      PushLog(L"THERMAL", L"WARNING", L"CPU temperature rising: " + std::to_wstring((int)previous.cpuTempC) + L"C -> " + std::to_wstring((int)snapshot.cpuTempC) + L"C.", ColorRole::Kernel, L"thermal", L"cpu", L"event=cpu_temp_rise");
    }
    if (snapshot.gpuTempC > previous.gpuTempC + 15.0 && snapshot.gpuTempC > 60.0) {
      PushLog(L"THERMAL", L"WARNING", L"GPU temperature rising: " + std::to_wstring((int)previous.gpuTempC) + L"C -> " + std::to_wstring((int)snapshot.gpuTempC) + L"C.", ColorRole::Kernel, L"thermal", L"gpu", L"event=gpu_temp_rise");
    }
    if (snapshot.cpuThrottling == 1 && !previous.cpuThrottling) {
      PushLog(L"THERMAL", L"ERROR", L"Thermal throttling onset at " + std::to_wstring((int)snapshot.cpuTempC) + L"C.", ColorRole::Error, L"thermal", L"throttle", L"event=throttle_onset");
    }
    if (snapshot.cpuThrottling == 0 && previous.cpuThrottling) {
      PushLog(L"THERMAL", L"INFO", L"Thermal throttling cleared.", ColorRole::Success, L"thermal", L"throttle", L"event=throttle_clear");
    }
    if (snapshot.cpuTempC >= 85.0 || snapshot.gpuTempC >= 85.0) {
      PushLog(L"THERMAL", L"ERROR", L"Overtemperature warning: CPU " + std::to_wstring((int)snapshot.cpuTempC) + L"C, GPU " + std::to_wstring((int)snapshot.gpuTempC) + L"C.", ColorRole::Error, L"thermal", L"overtemp", L"event=overtemp");
    }
    if (snapshot.cpuTempC >= 95.0 || snapshot.gpuTempC >= 95.0) {
      PushLog(L"THERMAL", L"ERROR", L"CRITICAL temperature: CPU " + std::to_wstring((int)snapshot.cpuTempC) + L"C, GPU " + std::to_wstring((int)snapshot.gpuTempC) + L"C. Shutdown imminent.", ColorRole::Error, L"thermal", L"critical", L"event=critical_temp");
    }
    if (snapshot.fanCount < previous.fanCount && previous.fanCount > 0) {
      PushLog(L"THERMAL", L"ERROR", L"Fan failure detected: " + std::to_wstring(previous.fanCount) + L" -> " + std::to_wstring(snapshot.fanCount) + L" fans.", ColorRole::Error, L"thermal", L"fan", L"event=fan_failure");
    }
  }

  if (hadPrevious) {
    if (snapshot.tpmReady != previous.tpmReady && previous.tpmPresent == 1) {
      PushLog(L"HARDWARE", L"WARNING", L"TPM state changed: " + std::wstring(previous.tpmReady ? L"ready" : L"not ready") + L" -> " + std::wstring(snapshot.tpmReady ? L"ready" : L"not ready") + L".", ColorRole::Kernel, L"hardware", L"tpm", L"event=tpm_state");
    }
    if (previous.tpmReady == 1 && snapshot.tpmReady == 0) {
      PushLog(L"HARDWARE", L"ERROR", L"TPM failure detected.", ColorRole::Error, L"hardware", L"tpm", L"event=tpm_failure");
    }
    if (snapshot.pcieErrors > previous.pcieErrors) {
      PushLog(L"HARDWARE", L"WARNING", L"PCIe bus error detected.", ColorRole::Kernel, L"hardware", L"pcie", L"event=pcie_error");
    }
    if (snapshot.voltage12V > 0 && previous.voltage12V > 0 && snapshot.voltage12V < previous.voltage12V - 0.3) {
      PushLog(L"HARDWARE", L"WARNING", L"12V rail drop: " + std::to_wstring((int)(previous.voltage12V * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltage12V * 1000)) + L" mV.", ColorRole::Kernel, L"hardware", L"voltage", L"event=v12_drop");
    }
    if (snapshot.voltage5V > 0 && previous.voltage5V > 0 && snapshot.voltage5V < previous.voltage5V - 0.2) {
      PushLog(L"HARDWARE", L"WARNING", L"5V rail drop: " + std::to_wstring((int)(previous.voltage5V * 1000)) + L" -> " + std::to_wstring((int)(snapshot.voltage5V * 1000)) + L" mV.", ColorRole::Kernel, L"hardware", L"voltage", L"event=v5_drop");
    }
    if (snapshot.cmosBatteryOk == 0 && previous.cmosBatteryOk == 1) {
      PushLog(L"HARDWARE", L"INFO", L"CMOS battery low.", ColorRole::Kernel, L"hardware", L"cmos", L"event=cmos_low");
    }
  }

  if (hadPrevious) {
    if (snapshot.fsVolumeDirtyBit == 1 && previous.fsVolumeDirtyBit == 0) {
      PushLog(L"FILESYSTEM", L"ERROR", L"Volume dirty bit set on C:.", ColorRole::Error, L"filesystem", L"dirty", L"event=dirty_bit");
    }
    if (!snapshot.fsVolumeLabel.empty() && !previous.fsVolumeLabel.empty() && snapshot.fsVolumeLabel != previous.fsVolumeLabel) {
      PushLog(L"FILESYSTEM", L"WARNING", L"Volume label changed: " + previous.fsVolumeLabel + L" -> " + snapshot.fsVolumeLabel + L".", ColorRole::Kernel, L"filesystem", L"label", L"event=label_change");
    }
    if (snapshot.fsSystem32HiddenCount > previous.fsSystem32HiddenCount + 3) {
      PushLog(L"FILESYSTEM", L"WARNING", L"Hidden file creation in System32: +" + std::to_wstring(snapshot.fsSystem32HiddenCount - previous.fsSystem32HiddenCount) + L".", ColorRole::Kernel, L"filesystem", L"hidden", L"event=hidden_create");
    }
    if (snapshot.fsSystem32SystemCount != previous.fsSystem32SystemCount && previous.fsSystem32SystemCount > 0) {
      PushLog(L"FILESYSTEM", L"ERROR", L"System file modification in System32.", ColorRole::Error, L"filesystem", L"sysfile", L"event=sys_mod");
    }
    if (snapshot.fsReparsePointCount > previous.fsReparsePointCount + 2) {
      PushLog(L"FILESYSTEM", L"WARNING", L"Reparse point creation detected.", ColorRole::Kernel, L"filesystem", L"reparse", L"event=reparse_create");
    }
    if (snapshot.fsAdsWithDataCount > previous.fsAdsWithDataCount) {
      PushLog(L"FILESYSTEM", L"WARNING", L"Alternate data stream creation on ntoskrnl.exe.", ColorRole::Kernel, L"filesystem", L"ads", L"event=ads_create");
    }
    if (snapshot.regHashRun != previous.regHashRun && previous.regHashRun != 0) {
      PushLog(L"REGISTRY", L"ERROR", L"Run key hash changed. Persistence entries modified.", ColorRole::Error, L"registry", L"run", L"event=run_change");
    }
    if (snapshot.regValueCountRun != previous.regValueCountRun) {
      PushLog(L"REGISTRY", L"WARNING", L"Run key value count changed: " + std::to_wstring(previous.regValueCountRun) + L" -> " + std::to_wstring(snapshot.regValueCountRun) + L".", ColorRole::Warning, L"registry", L"run", L"event=run_count");
    }
    if (snapshot.regHashShell != previous.regHashShell && previous.regHashShell != 0) {
      PushLog(L"REGISTRY", L"ERROR", L"Shell open command hash changed.", ColorRole::Error, L"registry", L"shell", L"event=shell_change");
    }
    if (snapshot.regHashFirewall != previous.regHashFirewall && previous.regHashFirewall != 0) {
      PushLog(L"REGISTRY", L"ERROR", L"Firewall policy hash changed.", ColorRole::Error, L"registry", L"firewall", L"event=fw_change");
    }
    if (snapshot.regHashUac != previous.regHashUac && previous.regHashUac != 0) {
      PushLog(L"REGISTRY", L"ERROR", L"UAC policy hash changed.", ColorRole::Error, L"registry", L"uac", L"event=uac_change");
    }
    if (snapshot.startupFolderCount != previous.startupFolderCount) {
      PushLog(L"REGISTRY", L"WARNING", L"Startup folder item count changed: " + std::to_wstring(previous.startupFolderCount) + L" -> " + std::to_wstring(snapshot.startupFolderCount) + L".", ColorRole::Warning, L"registry", L"startup", L"event=startup_change");
    }
    if (snapshot.regHashEnvPath != previous.regHashEnvPath && previous.regHashEnvPath != 0) {
      PushLog(L"REGISTRY", L"WARNING", L"PATH environment variable changed.", ColorRole::Warning, L"registry", L"path", L"event=path_change");
    }
    if (snapshot.regKeyCountDrivers != previous.regKeyCountDrivers && previous.regKeyCountDrivers > 0) {
      PushLog(L"REGISTRY", L"WARNING", L"Driver service count changed: " + std::to_wstring(previous.regKeyCountDrivers) + L" -> " + std::to_wstring(snapshot.regKeyCountDrivers) + L".", ColorRole::Warning, L"registry", L"driver", L"event=driver_reg");
    }
    if (snapshot.regHashServices != previous.regHashServices && previous.regHashServices != 0) {
      PushLog(L"REGISTRY", L"INFO", L"Service configuration hash changed.", ColorRole::Network, L"registry", L"service", L"event=svc_change");
    }
    if (snapshot.audioOutputDeviceCount != previous.audioOutputDeviceCount && previous.audioOutputDeviceCount > 0) {
      const wchar_t* action = snapshot.audioOutputDeviceCount > previous.audioOutputDeviceCount ? L"connected" : L"disconnected";
      PushLog(L"AUDIO", L"INFO", L"Audio output device " + std::wstring(action) + L" (" + std::to_wstring(previous.audioOutputDeviceCount) + L" -> " + std::to_wstring(snapshot.audioOutputDeviceCount) + L").", ColorRole::Network, L"audio", L"device", L"event=audio_device");
    }
    if (snapshot.audioDeviceHash != previous.audioDeviceHash && previous.audioDeviceHash != 0) {
      PushLog(L"AUDIO", L"ERROR", L"Default audio output device changed.", ColorRole::Error, L"audio", L"output", L"event=audio_output");
    }
    if (snapshot.audioServiceRunning != previous.audioServiceRunning && previous.audioServiceRunning >= 0) {
      PushLog(L"AUDIO", L"ERROR", L"Windows Audio service state changed: " + std::to_wstring(previous.audioServiceRunning) + L" -> " + std::to_wstring(snapshot.audioServiceRunning) + L".", ColorRole::Error, L"audio", L"service", L"event=audio_svc");
    }
    if (snapshot.audioSystemMuted != previous.audioSystemMuted && previous.audioSystemMuted >= 0) {
      PushLog(L"AUDIO", L"INFO", snapshot.audioSystemMuted ? L"System audio muted." : L"System audio unmuted.", ColorRole::Network, L"audio", L"mute", L"event=audio_mute");
    }
    if (snapshot.audioSampleRate != previous.audioSampleRate && previous.audioSampleRate > 0) {
      PushLog(L"AUDIO", L"ERROR", L"Audio sample rate changed.", ColorRole::Error, L"audio", L"format", L"event=audio_format");
    }
    if (snapshot.audioLatencyMs > previous.audioLatencyMs + 80 && previous.audioLatencyMs > 0) {
      PushLog(L"AUDIO", L"WARNING", L"Audio latency spike: " + std::to_wstring(previous.audioLatencyMs) + L"ms -> " + std::to_wstring(snapshot.audioLatencyMs) + L"ms.", ColorRole::Warning, L"audio", L"latency", L"event=audio_latency");
    }
    if (snapshot.sehExceptionCount > previous.sehExceptionCount && previous.sehExceptionCount >= 0) {
      PushLog(L"RELIABILITY", L"ERROR", L"SEH exception count increased: " + std::to_wstring(previous.sehExceptionCount) + L" -> " + std::to_wstring(snapshot.sehExceptionCount) + L".", ColorRole::Error, L"reliability", L"exception", L"event=seh");
    }
    if (snapshot.accessViolationCount > previous.accessViolationCount && previous.accessViolationCount >= 0) {
      PushLog(L"RELIABILITY", L"ERROR", L"Access violation detected: " + std::to_wstring(snapshot.accessViolationCount) + L".", ColorRole::Error, L"reliability", L"access_violation", L"event=av");
    }
    if (snapshot.stackOverflowCount > previous.stackOverflowCount && previous.stackOverflowCount >= 0) {
      PushLog(L"RELIABILITY", L"ERROR", L"Stack overflow detected: " + std::to_wstring(snapshot.stackOverflowCount) + L".", ColorRole::Error, L"reliability", L"stack", L"event=stack_overflow");
    }
    if (snapshot.heapCorruptionDetected > previous.heapCorruptionDetected && previous.heapCorruptionDetected >= 0) {
      PushLog(L"RELIABILITY", L"ERROR", L"Heap corruption detected: " + std::to_wstring(snapshot.heapCorruptionDetected) + L".", ColorRole::Error, L"reliability", L"heap", L"event=heap_corrupt");
    }
    if (snapshot.crashEventsToday > previous.crashEventsToday && previous.crashEventsToday >= 0) {
      PushLog(L"RELIABILITY", L"ERROR", L"Crash dump files found: " + std::to_wstring(snapshot.crashEventsToday) + L" today.", ColorRole::Error, L"reliability", L"crash", L"event=crash");
    }
    if (snapshot.processHash != previous.processHash && previous.processHash != 0) {
      PushLog(L"RELIABILITY", L"INFO", L"Process set changed (hash delta detected).", ColorRole::Network, L"reliability", L"process", L"event=proc_change");
    }
    if (snapshot.moduleHash != previous.moduleHash && previous.moduleHash != 0) {
      PushLog(L"RELIABILITY", L"INFO", L"Module list changed (DLL load/unload).", ColorRole::Network, L"reliability", L"module", L"event=mod_change");
    }
    if (snapshot.mainThreadResponsive == 0 && previous.mainThreadResponsive == 1) {
      PushLog(L"RELIABILITY", L"WARNING", L"Main thread became unresponsive.", ColorRole::Warning, L"reliability", L"hang", L"event=hang");
    }
    if (snapshot.uiResponsivenessMs > 300 && previous.uiResponsivenessMs > 0) {
      PushLog(L"RELIABILITY", L"WARNING", L"UI freeze detected: " + std::to_wstring(snapshot.uiResponsivenessMs) + L"ms response time.", ColorRole::Warning, L"reliability", L"freeze", L"event=freeze");
    }
    if (snapshot.safeModeActive != previous.safeModeActive && previous.safeModeActive >= 0) {
      PushLog(L"RELIABILITY", L"WARNING", snapshot.safeModeActive ? L"Entered safe mode." : L"Exited safe mode.", ColorRole::Warning, L"reliability", L"safe_mode", L"event=safe_mode");
    }
    if (snapshot.assertionFailureCount > previous.assertionFailureCount && previous.assertionFailureCount >= 0) {
      PushLog(L"RELIABILITY", L"ERROR", L"Assertion failure: " + std::to_wstring(snapshot.assertionFailureCount) + L".", ColorRole::Error, L"reliability", L"assert", L"event=assert");
    }
  }

  if (state_.sampleCount % 40 == 0) {
    PushLog(L"NETWORK", L"INFO", L"Latency " + std::to_wstring(snapshot.latencyMs) + L"ms, " + std::to_wstring(snapshot.outboundConnections) + L" established / " + std::to_wstring(snapshot.inboundConnections) + L" listening sockets.", ColorRole::Network);
  }

  if (state_.sampleCount % 60 == 0) {
    PushLog(L"KERNEL", L"INFO", L"Queue ALL " + std::to_wstring(snapshot.processorQueueLength) + L" | switches " + std::to_wstring(snapshot.contextSwitchesPerSec) + L"/s | IRQ " + std::to_wstring(snapshot.interruptsPerSec) + L"/s | threads " + std::to_wstring(snapshot.threadCount) + L" | handles " + std::to_wstring(snapshot.handleCount), ColorRole::Kernel);
  }

  UpdateScramSummary(snapshot);
  {
    const double currentRisk = static_cast<double>(state_.scramRiskScore);
    const double prevRisk = state_.prevScramRisk;
    const double delta = currentRisk - prevRisk;

    state_.smoothedScramRisk = state_.smoothedScramRisk * 0.7 + currentRisk * 0.3;

    int rawSeverity = currentRisk >= 60 ? 2 : currentRisk >= 50 ? 1 : 0;

    if (rawSeverity == 2) {
      state_.scramErrorHold++;
    } else {
      state_.scramErrorHold = 0;
    }
    if (rawSeverity < 2 && state_.scramCurrentSeverity == 3) {
      rawSeverity = 2;
    }
    if (state_.scramErrorHold >= 10 && rawSeverity >= 2) {
      rawSeverity = 3;
    }

    if (rawSeverity > state_.scramCurrentSeverity) {
      state_.scramCurrentSeverity = rawSeverity;
      state_.scramSeverityHold = 0;
    } else if (rawSeverity < state_.scramCurrentSeverity) {
      state_.scramSeverityHold++;
      if (state_.scramSeverityHold < 6) {
        rawSeverity = state_.scramCurrentSeverity;
      } else {
        state_.scramCurrentSeverity = rawSeverity;
        state_.scramSeverityHold = 0;
      }
    } else {
      state_.scramSeverityHold = 0;
    }

    const int prevSeverity = static_cast<int>(prevRisk >= 80 ? 3 : prevRisk >= 60 ? 2 : prevRisk >= 50 ? 1 : 0);
    const bool severityChanged = state_.scramCurrentSeverity != prevSeverity;
    const bool significantChange = std::abs(delta) >= 3.0;

    if (severityChanged || (significantChange && state_.sampleCount % 10 == 0)) {
      if (!state_.scramHeadline.empty()) {
        const wchar_t* trend = delta > 0 ? L"\x2191" : delta < 0 ? L"\x2193" : L"\x2192";
        const ColorRole color = state_.scramCurrentSeverity == 3 ? ColorRole::CriticalScram
                              : state_.scramCurrentSeverity == 2 ? ColorRole::Error
                              : state_.scramCurrentSeverity == 1 ? ColorRole::Warning
                              : ColorRole::Scram;
        const std::wstring sevText = state_.scramCurrentSeverity == 3 ? L"CRITICAL"
                                   : state_.scramCurrentSeverity == 2 ? L"ERROR"
                                   : state_.scramCurrentSeverity == 1 ? L"WARNING"
                                   : L"INFO";
        const int displayRisk = static_cast<int>(state_.smoothedScramRisk + 0.5);
        std::wstring msg = state_.scramHeadline;
        msg += L" [risk=" + std::to_wstring(displayRisk) + L" " + std::wstring(trend) + L"]";
        PushLog(L"SCRAM", sevText, msg, color, L"scram", L"risk");
      }
    }
    state_.prevScramRisk = currentRisk;
  }
  state_.snapshot = std::move(snapshot);
  g_phase = "CONSUME:DONE";
}

LRESULT CALLBACK MonixApp::StaticWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    auto* self = static_cast<MonixApp*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }

  auto* self = reinterpret_cast<MonixApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (self) {
    return self->WndProc(hwnd, message, wParam, lParam);
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool MonixApp::HitTestTabs(const RECT& clientRect, POINT point, Tab& tab) const {
  const auto rects = TabRects(clientRect);
  for (std::size_t index = 0; index < rects.size(); ++index) {
    if (PtInRect(&rects[index], point)) {
      tab = static_cast<Tab>(index);
      return true;
    }
  }
  return false;
}

int MonixApp::VisibleLogLines(const RECT& logRect) const {
  const int available = std::max(0, static_cast<int>(logRect.bottom - logRect.top));
  return std::max(1, available / std::max(18, logLineHeight_ + 2));
}

int MonixApp::VisibleTaskRows(const RECT& tableRect) const {
  const int rowHeight = std::max(38, bodyLineHeight_ + 14);
  return std::max(4, static_cast<int>(tableRect.bottom - tableRect.top - 56) / rowHeight);
}

std::vector<SettingActionRect> MonixApp::BuildSettingActionRects(const RECT& clientRect) const {
  using namespace monix::renderer_vk;
  const RECT outer = ContentRect(clientRect);
  const int subTabH = 32;
  const RECT contentArea { outer.left, outer.top + subTabH + 8, outer.right, outer.bottom };

  const int rowHeight = std::max(26, smallLineHeight_ + 4);
  std::vector<SettingActionRect> rows;

  const auto appendPanel = [&](const RECT& panel, const auto& settings, int firstRowY) {
    int y = firstRowY;
    for (const SettingId id : settings) {
      SettingActionRect action;
      action.id = id;
      action.row = RECT { panel.left + 18, y, panel.right - 18, y + rowHeight };
      action.decrease = RECT { action.row.right - 84, action.row.top + 4, action.row.right - 52, action.row.bottom - 4 };
      action.increase = RECT { action.row.right - 42, action.row.top + 4, action.row.right - 10, action.row.bottom - 4 };
      action.value = RECT { action.row.right - 246, action.row.top, action.row.right - 94, action.row.bottom };
      rows.push_back(action);
      y += rowHeight + 6;
    }
  };

  if (state_.shaderBrowser.subTab == SettingsSubTab::General) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    RECT rightPanel = contentArea;
    rightPanel.left = leftPanel.right + 14;
    RECT leftBottom = contentArea;
    leftBottom.top = leftPanel.top + 58 + static_cast<int>(RuntimeSettings().size() * (rowHeight + 6)) + 14;
    appendPanel(leftPanel, RuntimeSettings(), leftPanel.top + 58);
    appendPanel(leftBottom, IntroAnimSettings(), leftBottom.top + 8);
    appendPanel(rightPanel, GeneralExtendedSettings(), rightPanel.top + 58);
  } else if (state_.shaderBrowser.subTab == SettingsSubTab::Display) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(220, static_cast<int>((contentArea.bottom - contentArea.top) * 0.30));
    RECT rightMid = contentArea;
    rightMid.left = leftPanel.right + 14;
    rightMid.top = rightTop.bottom + 14;
    rightMid.bottom = rightTop.bottom + 14 + std::max(120, static_cast<int>((contentArea.bottom - contentArea.top) * 0.16));
    RECT rightMid2 = contentArea;
    rightMid2.left = leftPanel.right + 14;
    rightMid2.top = rightMid.bottom + 14;
    rightMid2.bottom = rightMid.bottom + 14 + std::max(100, static_cast<int>((contentArea.bottom - contentArea.top) * 0.14));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightMid2.bottom + 14;
    RECT rightBottom2 = contentArea;
    rightBottom2.left = leftPanel.right + 14;
    rightBottom2.top = rightBottom.bottom + 14;
    appendPanel(rightBottom2, BorderSettings(), rightBottom2.top + 58);
  } else if (state_.shaderBrowser.subTab == SettingsSubTab::Logging) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    appendPanel(leftPanel, LoggingSettings(), leftPanel.top + 58);
  } else if (state_.shaderBrowser.subTab == SettingsSubTab::System) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(320, static_cast<int>((contentArea.bottom - contentArea.top) * 0.45));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightTop.bottom + 14;
    appendPanel(leftPanel, ThemeSettings(), leftPanel.top + 58);
    appendPanel(rightTop, WindowStartupSettings(), rightTop.top + 58);
    appendPanel(rightTop, SystemExtendedSettings(), rightTop.top + 58 + static_cast<int>((rightTop.bottom - rightTop.top) * 0.50));
    appendPanel(rightBottom, HotkeySettings(), rightBottom.top + 58);
  } else if (state_.shaderBrowser.subTab == SettingsSubTab::Performance) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    appendPanel(leftPanel, PerformanceSettings(), leftPanel.top + 58);
  } else {
    appendPanel(contentArea, RuntimeSettings(), contentArea.top + 58);
  }
  return rows;
}

bool MonixApp::HandleSettingsClick(const RECT& clientRect, POINT point) {
  using namespace monix::renderer_vk;
  const RECT outer = ContentRect(clientRect);

  const int subTabW = 86;
  const int subTabH = 32;
  const int subTabGap = 4;
  const int subTabY = outer.top;

  RECT generalTab { outer.left, subTabY, outer.left + subTabW, subTabY + subTabH };
  RECT displayTab { outer.left + subTabW + subTabGap, subTabY, outer.left + subTabW * 2 + subTabGap, subTabY + subTabH };
  RECT loggingTab { outer.left + subTabW * 2 + subTabGap * 2, subTabY, outer.left + subTabW * 3 + subTabGap * 2, subTabY + subTabH };
  RECT shadersTab { outer.left + subTabW * 3 + subTabGap * 3, subTabY, outer.left + subTabW * 4 + subTabGap * 3, subTabY + subTabH };
  RECT systemTab { outer.left + subTabW * 4 + subTabGap * 4, subTabY, outer.left + subTabW * 5 + subTabGap * 4, subTabY + subTabH };
  RECT perfTab { outer.left + subTabW * 5 + subTabGap * 5, subTabY, outer.left + subTabW * 6 + subTabGap * 5, subTabY + subTabH };

  if (PtInRect(&generalTab, point)) {
    state_.shaderBrowser.subTab = SettingsSubTab::General;
    return true;
  }
  if (PtInRect(&displayTab, point)) {
    state_.shaderBrowser.subTab = SettingsSubTab::Display;
    return true;
  }
  if (PtInRect(&loggingTab, point)) {
    state_.shaderBrowser.subTab = SettingsSubTab::Logging;
    return true;
  }
  if (PtInRect(&shadersTab, point)) {
    state_.shaderBrowser.subTab = SettingsSubTab::Shaders;
    return true;
  }
  if (PtInRect(&systemTab, point)) {
    state_.shaderBrowser.subTab = SettingsSubTab::System;
    return true;
  }
  if (PtInRect(&perfTab, point)) {
    state_.shaderBrowser.subTab = SettingsSubTab::Performance;
    return true;
  }

  if (state_.shaderBrowser.subTab == SettingsSubTab::General) {
    const auto rows = BuildSettingActionRects(clientRect);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      if (!PtInRect(&row.row, point)) {
        continue;
      }
      state_.selectedSettingIndex = static_cast<int>(index);
      int direction = 0;
      if (PtInRect(&row.decrease, point)) {
        direction = -1;
      } else if (PtInRect(&row.increase, point) || PtInRect(&row.value, point)) {
        direction = 1;
      }
      if (direction != 0) {
        CommitSettingMutation(AdjustSetting(row.id, direction));
      }
      return true;
    }
    return false;
  }

  if (state_.shaderBrowser.subTab == SettingsSubTab::Display || state_.shaderBrowser.subTab == SettingsSubTab::Logging || state_.shaderBrowser.subTab == SettingsSubTab::Performance) {
    const auto rows = BuildSettingActionRects(clientRect);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      if (!PtInRect(&row.row, point)) {
        continue;
      }
      state_.selectedSettingIndex = static_cast<int>(index);
      int direction = 0;
      if (PtInRect(&row.decrease, point)) {
        direction = -1;
      } else if (PtInRect(&row.increase, point) || PtInRect(&row.value, point)) {
        direction = 1;
      }
      if (direction != 0) {
        CommitSettingMutation(AdjustSetting(row.id, direction));
      }
      return true;
    }
    return false;
  }

  if (state_.shaderBrowser.subTab == SettingsSubTab::System) {
    const auto rows = BuildSettingActionRects(clientRect);
    for (std::size_t index = 0; index < rows.size(); ++index) {
      const auto& row = rows[index];
      if (!PtInRect(&row.row, point)) {
        continue;
      }
      state_.selectedSettingIndex = static_cast<int>(index);
      int direction = 0;
      if (PtInRect(&row.decrease, point)) {
        direction = -1;
      } else if (PtInRect(&row.increase, point) || PtInRect(&row.value, point)) {
        direction = 1;
      }
      if (direction != 0) {
        CommitSettingMutation(AdjustSetting(row.id, direction));
      }
      return true;
    }
    return false;
  }

  if (state_.shaderBrowser.subTab == SettingsSubTab::Shaders) {
    const RECT contentArea { outer.left, outer.top + subTabH + 8, outer.right, outer.bottom };

    const int filterBtnW = 80;
    const int filterBtnH = 26;
    const int filterGap = 8;
    int fx = contentArea.left + 14;

    auto hitFilterTab = [&](ShaderLanguageFilter tabFilter) -> bool {
      RECT btn { fx, contentArea.top + 4, fx + filterBtnW, contentArea.top + 4 + filterBtnH };
      fx += filterBtnW + filterGap;
      if (PtInRect(&btn, point)) {
        state_.shaderBrowser.filter = tabFilter;
        state_.shaderBrowser.selectedShaderIndex = -1;
        state_.shaderBrowser.scrollOffset = 0;
        return true;
      }
      return false;
    };

    if (hitFilterTab(ShaderLanguageFilter::GLSL)) return true;
    if (hitFilterTab(ShaderLanguageFilter::Slang)) return true;
    if (hitFilterTab(ShaderLanguageFilter::Preset)) return true;

    RECT favBtn { fx, contentArea.top + 4, fx + filterBtnW + 20, contentArea.top + 4 + filterBtnH };
    if (PtInRect(&favBtn, point)) {
      state_.shaderShowFavoritesOnly = !state_.shaderShowFavoritesOnly;
      state_.shaderBrowser.scrollOffset = 0;
      return true;
    }
    fx += filterBtnW + 20 + filterGap;

    const int listW = static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT filterBar = contentArea;
    filterBar.bottom = filterBar.top + 34;

    RECT searchBox { fx, contentArea.top + 4, contentArea.left + listW - 14, contentArea.top + 4 + filterBtnH };
    if (PtInRect(&searchBox, point)) {
      state_.shaderSearchFocused = true;
      return true;
    } else {
      state_.shaderSearchFocused = false;
    }

    RECT listPanel = contentArea;
    listPanel.top = filterBar.bottom + 6;
    listPanel.right = contentArea.left + listW;

    RECT detailPanel = contentArea;
    detailPanel.left = listPanel.right + 14;
    detailPanel.top = filterBar.bottom + 6;

    const int rowH = std::max(26, smallLineHeight_ + 4);
    const int innerPad = 18;

    RECT actionBar { listPanel.left, listPanel.bottom - 42, listPanel.right, listPanel.bottom - 8 };

    const int actionBtnW = 80;
    const int actionBtnH = 28;
    const int actionGap = 6;
    int abx = actionBar.left + innerPad;

    auto hitActionBtn = [&](const wchar_t* label) -> bool {
      RECT btn { abx, actionBar.top, abx + actionBtnW, actionBar.top + actionBtnH };
      abx += actionBtnW + actionGap;
      if (PtInRect(&btn, point)) {
        if (wcscmp(label, L"IMPORT") == 0) {
          OPENFILENAMEA ofn;
          ZeroMemory(&ofn, sizeof(ofn));
          char szFile[MAX_PATH] = "";
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hwnd_;
          ofn.lpstrFilter = "GLSL\0*.glsl\0GLSLPreset\0*.glslp\0Slang\0*.slang\0SlangPreset\0*.slangp\0All\0*.*\0";
          ofn.lpstrFile = szFile;
          ofn.nMaxFile = MAX_PATH;
          ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
          if (GetOpenFileNameA(&ofn)) {
            std::string srcPath = szFile;
            std::filesystem::path src(srcPath);
            std::string ext = src.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            std::string destDir;
            if (ext == ".glsl") destDir = "GLSL";
            else if (ext == ".glslp") destDir = "GLSLP";
            else if (ext == ".slang") destDir = "SLANG";
            else if (ext == ".slangp") destDir = "SLANGP";
            else return true;
            std::filesystem::path destRoot = paths_.rootDir / "Shaders";
            std::filesystem::path dest = destRoot / destDir / src.filename();
            std::error_code ec;
            std::filesystem::create_directories(dest.parent_path(), ec);
            std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
            if (shaderLibrary_) {
              shaderLibrary_->rescan();
              if (shaderBrowserPanel_) {
                shaderBrowserPanel_->setSearchQuery("");
                state_.shaderSearchText.clear();
              }
            }
          }
          return true;
        }
        if (wcscmp(label, L"DELETE") == 0) {
          if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
            const auto* entry = shaderBrowserPanel_->selectedEntry();
            if (entry && entry->sourceKind == monix::renderer_vk::ShaderSourceKind::User) {
              int resp = MessageBoxA(hwnd_, ("Delete shader: " + entry->name + "?").c_str(),
                "Confirm Delete", MB_YESNO | MB_ICONQUESTION);
              if (resp == IDYES) {
                std::error_code ec;
                std::filesystem::remove(entry->path, ec);
                if (shaderLibrary_) shaderLibrary_->rescan();
                state_.shaderBrowser.selectedShaderIndex = -1;
              }
            }
          }
          return true;
        }
        if (wcscmp(label, L"REFRESH") == 0) {
          if (shaderLibrary_) shaderLibrary_->rescan();
          return true;
        }
        if (wcscmp(label, L"OPEN") == 0) {
          std::wstring wsPath = L"\"" + paths_.rootDir.wstring() + L"\\Shaders\"";
          ShellExecuteW(nullptr, L"open", L"explorer.exe", wsPath.c_str(), nullptr, SW_SHOWDEFAULT);
          return true;
        }
        return true;
      }
      return false;
    };

    if (hitActionBtn(L"IMPORT")) return true;
    if (hitActionBtn(L"DELETE")) return true;
    if (hitActionBtn(L"REFRESH")) return true;
    if (hitActionBtn(L"OPEN")) return true;

    if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
      RECT loadBtn { detailPanel.left + 18, detailPanel.bottom - 52, detailPanel.left + 120, detailPanel.bottom - 22 };
      RECT reloadBtn { detailPanel.left + 130, detailPanel.bottom - 52, detailPanel.left + 240, detailPanel.bottom - 22 };

      if (PtInRect(&loadBtn, point)) {
        shaderBrowserPanel_->requestLoad(txState_);
        return true;
      }
      if (PtInRect(&reloadBtn, point)) {
        shaderBrowserPanel_->requestReload(txState_);
        return true;
      }
    }

    if (PtInRect(&listPanel, point)) {
      int y = listPanel.top + 48;
      int globalRow = 0;
      const auto categories = shaderBrowserPanel_ ? shaderBrowserPanel_->categoryGroups() : std::vector<ShaderCategoryGroup>{};

      for (const auto& cat : categories) {
        if (y + rowH > listPanel.bottom - 50) break;
        y += rowH + 2;

        for (const size_t libIdx : cat.entryIndices) {
          if (globalRow >= state_.shaderBrowser.scrollOffset) {
            if (y + rowH > listPanel.bottom - 50) break;
            RECT rowRect { listPanel.left + innerPad + 12, y, listPanel.right - innerPad, y + rowH };
            if (PtInRect(&rowRect, point)) {
              state_.shaderBrowser.selectedShaderIndex = globalRow;
              return true;
            }
            y += rowH + 2;
          }
          ++globalRow;
        }
      }
    }

    return false;
  }

  return false;
}

bool MonixApp::HandleSettingsKey(WPARAM key) {
  using namespace monix::renderer_vk;
  if (state_.shaderBrowser.subTab == SettingsSubTab::Shaders) {
    if (state_.shaderSearchFocused) {
      if (key == VK_ESCAPE) {
        state_.shaderSearchFocused = false;
        state_.shaderSearchText.clear();
        if (shaderBrowserPanel_) shaderBrowserPanel_->setSearchQuery("");
        return true;
      }
      return true;
    }

    if (key == VK_UP) {
      state_.shaderBrowser.selectedShaderIndex = std::max(0, state_.shaderBrowser.selectedShaderIndex - 1);
      return true;
    }
    if (key == VK_DOWN) {
      int maxIdx = shaderBrowserPanel_ ? shaderBrowserPanel_->filteredShaderCount() - 1 : 0;
      state_.shaderBrowser.selectedShaderIndex = std::min(maxIdx, state_.shaderBrowser.selectedShaderIndex + 1);
      return true;
    }
    if (key == VK_RETURN) {
      if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
        shaderBrowserPanel_->requestLoad(txState_);
      }
      return true;
    }
    if (key == VK_DELETE) {
      if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
        const auto* entry = shaderBrowserPanel_->selectedEntry();
        if (entry && entry->sourceKind == monix::renderer_vk::ShaderSourceKind::User) {
          int resp = MessageBoxA(hwnd_, ("Delete shader: " + entry->name + "?").c_str(),
            "Confirm Delete", MB_YESNO | MB_ICONQUESTION);
          if (resp == IDYES) {
            std::error_code ec;
            std::filesystem::remove(entry->path, ec);
            if (shaderLibrary_) shaderLibrary_->rescan();
            state_.shaderBrowser.selectedShaderIndex = -1;
          }
        }
      }
      return true;
    }
    return false;
  }

  const auto getTabSettings = [&]() -> std::vector<SettingId> {
    using namespace monix::renderer_vk;
    switch (state_.shaderBrowser.subTab) {
      case SettingsSubTab::General: {
        std::vector<SettingId> v;
        for (auto id : RuntimeSettings()) v.push_back(id);
        for (auto id : IntroAnimSettings()) v.push_back(id);
        for (auto id : GeneralExtendedSettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::Display: {
        std::vector<SettingId> v;
        for (auto id : FullCrtSettings()) v.push_back(id);
        for (auto id : AdvancedCrtSettings()) v.push_back(id);
        for (auto id : ColorCorrectionSettings()) v.push_back(id);
        for (auto id : ScanlineStyleSettings()) v.push_back(id);
        for (auto id : BurnInSettings()) v.push_back(id);
        for (auto id : BorderSettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::Logging: {
        std::vector<SettingId> v;
        for (auto id : LoggingSettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::System: {
        std::vector<SettingId> v;
        for (auto id : ThemeSettings()) v.push_back(id);
        for (auto id : WindowStartupSettings()) v.push_back(id);
        for (auto id : SystemExtendedSettings()) v.push_back(id);
        for (auto id : HotkeySettings()) v.push_back(id);
        return v;
      }
      case SettingsSubTab::Performance: {
        std::vector<SettingId> v;
        for (auto id : PerformanceSettings()) v.push_back(id);
        return v;
      }
      default: return {};
    }
  };

  const auto tabSettings = getTabSettings();
  if (tabSettings.empty()) {
    return false;
  }

  state_.selectedSettingIndex = std::clamp(state_.selectedSettingIndex, 0, static_cast<int>(tabSettings.size()) - 1);
  if (key == VK_UP) {
    state_.selectedSettingIndex = std::max(0, state_.selectedSettingIndex - 1);
    return true;
  }
  if (key == VK_DOWN) {
    state_.selectedSettingIndex = std::min(static_cast<int>(tabSettings.size()) - 1, state_.selectedSettingIndex + 1);
    return true;
  }
  if (key == VK_LEFT) {
    CommitSettingMutation(AdjustSetting(tabSettings[state_.selectedSettingIndex], -1));
    return true;
  }
  if (key == VK_RIGHT || key == VK_RETURN || key == VK_SPACE) {
    CommitSettingMutation(AdjustSetting(tabSettings[state_.selectedSettingIndex], 1));
    return true;
  }

  return false;
}

int MonixApp::HitTestTaskRow(const RECT& clientRect, POINT point) const {
  const RECT outer = ContentRect(clientRect);
  RECT tableRect = outer;
  tableRect.right = outer.right - 380;
  tableRect.top += 68;
  tableRect.bottom -= 18;

  if (!PtInRect(&tableRect, point)) {
    return -1;
  }

  const int headerHeight = 48;
  if (point.y < tableRect.top + headerHeight) {
    return -1;
  }

  const int rowHeight = std::max(38, bodyLineHeight_ + 14);
  const int rowIndex = (point.y - tableRect.top - headerHeight) / rowHeight;
  const int visibleRows = VisibleTaskRows(tableRect);
  if (rowIndex < 0 || rowIndex >= visibleRows) {
    return -1;
  }

  const int processIndex = state_.taskScroll + rowIndex;
  if (processIndex >= static_cast<int>(state_.snapshot.processes.size())) {
    return -1;
  }
  return processIndex;
}

void MonixApp::OpenTaskMenu(const RECT& clientRect, POINT point, int processIndex) {
  ContextMenuState menu;
  menu.visible = true;
  menu.processIndex = processIndex;
  menu.hoverIndex = -1;
  const int itemCount = 9;
  menu.rect = RECT {
    std::clamp(point.x, clientRect.left + 12, clientRect.right - kTaskMenuWidth - 12),
    std::clamp(point.y, clientRect.top + 12, clientRect.bottom - itemCount * kTaskMenuItemHeight - 18),
    0,
    0
  };
  menu.rect.right = menu.rect.left + kTaskMenuWidth;
  menu.rect.bottom = menu.rect.top + itemCount * kTaskMenuItemHeight;
  state_.taskMenu = menu;
}

bool MonixApp::HandleTaskMenuClick(POINT point) {
  if (!state_.taskMenu.visible || !PtInRect(&state_.taskMenu.rect, point)) {
    state_.taskMenu.visible = false;
    return false;
  }

  const int index = (point.y - state_.taskMenu.rect.top) / kTaskMenuItemHeight;
  if (index < 0 || index >= 9) {
    state_.taskMenu.visible = false;
    return false;
  }

  ExecuteTaskMenuAction(index);
  state_.taskMenu.visible = false;
  return true;
}

bool MonixApp::ApplyPriorityToProcess(int pid, DWORD priorityClass) {
  // First verify the process identity by querying its image name
  HANDLE queryHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!queryHandle) {
    return false;
  }
  wchar_t imageName[MAX_PATH] = {};
  DWORD imageNameLen = MAX_PATH;
  if (QueryFullProcessImageNameW(queryHandle, 0, imageName, &imageNameLen)) {
    OutputDebugStringA("[PROC] ApplyPriority: verified process identity\n");
  }
  CloseHandle(queryHandle);

  // Now open with set permission and perform the operation
  HANDLE handle = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!handle) {
    return false;
  }
  const bool success = SetPriorityClass(handle, priorityClass) != 0;
  CloseHandle(handle);
  return success;
}

bool MonixApp::TerminateProcessById(int pid) {
  // First verify the process identity by querying its image name
  HANDLE queryHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!queryHandle) {
    return false;
  }
  wchar_t imageName[MAX_PATH] = {};
  DWORD imageNameLen = MAX_PATH;
  if (QueryFullProcessImageNameW(queryHandle, 0, imageName, &imageNameLen)) {
    OutputDebugStringA("[PROC] Terminate: verified process identity\n");
  }
  CloseHandle(queryHandle);

  // Now open with terminate permission and perform the operation
  HANDLE handle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!handle) {
    return false;
  }
  const bool success = TerminateProcess(handle, 1) != 0;
  CloseHandle(handle);
  return success;
}

void MonixApp::CopyToClipboard(const std::wstring& text) const {
  if (!OpenClipboard(hwnd_)) {
    return;
  }
  const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (memory) {
    EmptyClipboard();
    void* buffer = GlobalLock(memory);
    if (buffer) {
      std::memcpy(buffer, text.c_str(), bytes);
      GlobalUnlock(memory);
      SetClipboardData(CF_UNICODETEXT, memory);
    } else {
      GlobalFree(memory);
    }
  }
  CloseClipboard();
}

void MonixApp::ExecuteTaskMenuAction(int itemIndex) {
  if (state_.taskMenu.processIndex < 0 || state_.taskMenu.processIndex >= static_cast<int>(state_.snapshot.processes.size())) {
    return;
  }

  auto& process = state_.snapshot.processes[state_.taskMenu.processIndex];
  switch (itemIndex) {
    case 0:
      state_.selectedTaskIndex = state_.taskMenu.processIndex;
      SetToast(L"Task inspection pinned");
      PushLog(L"TASKS", L"INFO", L"Inspection focus moved to " + process.name + L".", ColorRole::Primary);
      break;
    case 1:
      state_.trackedPid = process.pid;
      state_.activeTab = Tab::Scram;
      SetToast(L"Task linked into S.C.R.A.M watch");
      PushLog(L"SCRAM", L"INFO", process.name + L" linked into the live interpretation watchlist.", ColorRole::Scram);
      break;
    case 2:
      CopyToClipboard(process.name + L" | PID " + std::to_wstring(process.pid));
      SetToast(L"Task name and PID copied");
      break;
    case 3:
      if (ApplyPriorityToProcess(process.pid, HIGH_PRIORITY_CLASS)) {
        process.priority = L"HIGH";
        PushLog(L"TASKS", L"SUCCESS", process.name + L" priority raised to HIGH.", ColorRole::Success);
        SetToast(L"Priority raised");
      } else {
        PushLog(L"TASKS", L"ERROR", L"Unable to raise priority for " + process.name + L".", ColorRole::Error);
        SetToast(L"Priority change failed");
      }
      break;
    case 4:
      if (ApplyPriorityToProcess(process.pid, IDLE_PRIORITY_CLASS)) {
        process.priority = L"LOW";
        PushLog(L"TASKS", L"SUCCESS", process.name + L" priority dropped to LOW.", ColorRole::Success);
        SetToast(L"Priority lowered");
      } else {
        PushLog(L"TASKS", L"ERROR", L"Unable to lower priority for " + process.name + L".", ColorRole::Error);
        SetToast(L"Priority change failed");
      }
      break;
    case 5:
      if (TerminateProcessById(process.pid)) {
        PushLog(L"TASKS", L"CRITICAL", process.name + L" was terminated from MONIX control center.", ColorRole::Fatal, L"tasks", L"process-control", L"action=terminate");
        SetToast(L"Process terminated");
      } else {
        PushLog(L"TASKS", L"ERROR", L"Unable to terminate " + process.name + L".", ColorRole::Error, L"tasks", L"process-control", L"action=terminate");
        SetToast(L"Terminate failed");
      }
      break;
    case 6:
      ExportLogsJson();
      SetToast(L"Logs exported");
      PushLog(L"LOG", L"INFO", L"JSON export triggered from process control menu.", ColorRole::UserInput, L"log", L"export", L"source=task-menu");
      break;
    case 7:
      RequestRefresh();
      SetToast(L"Immediate refresh requested");
      break;
    case 8: {
      std::wstring cmd = L"/select,\"" + process.name + L"\"";
      ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOW);
      SetToast(L"Opening in Explorer");
      break;
    }
  }
}

void MonixApp::ComputeViewport(const RECT& client) {
  state_.viewport_ = client;
}

POINT MonixApp::MapToViewport(POINT windowPt) const {
  if (IsWin98ThemeActive()) return windowPt;
  const RECT& vp = state_.viewport_;
  const int vw = vp.right - vp.left;
  const int vh = vp.bottom - vp.top;
  if (vw <= 0 || vh <= 0) return windowPt;
  const double scale = 4.0 * vh / (3.0 * vw);
  const double centerX = vp.left + vw * 0.5;
  const double centerY = vp.top + vh * 0.5;
  POINT out;
  out.x = static_cast<long>(centerX + (windowPt.x - centerX) / scale);
  out.y = windowPt.y;
  out.x = std::clamp(out.x, vp.left, vp.right - 1);
  out.y = std::clamp(out.y, vp.top, vp.bottom - 1);
  return out;
}

void MonixApp::TickAnimations(const RECT& clientRect) {
  if (state_.toastUntilMs > 0 && GetTickCount64() > state_.toastUntilMs) {
    state_.toastMessage.clear();
    state_.toastUntilMs = 0;
  }

  state_.notifications.erase(
    std::remove_if(state_.notifications.begin(), state_.notifications.end(), [](const NotificationItem& item) {
      return GetTickCount64() > item.expiresAtMs;
    }),
    state_.notifications.end()
  );

  FlushLogQueues(false);

  if (!state_.intro.active) {
    return;
  }

  if (!state_.intro.soundPlayed && std::filesystem::exists(paths_.introWave)) {
    PlaySoundW(paths_.introWave.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    state_.intro.soundPlayed = true;
  }

  int cellW = 0;
  int cellH = 0;
  int logoWidth = 0;
  int logoHeight = 0;
  int maxColumns = 0;
  ComputeIntroLogoMetrics(clientRect, cellW, cellH, logoWidth, logoHeight, maxColumns);
  const int targetY = clientRect.top + ((clientRect.bottom - clientRect.top) - logoHeight) / 2 - 12;
  if (state_.intro.logoY == -9999) {
    state_.intro.logoY = -logoHeight;
  }

  if (state_.intro.logoY < targetY) {
    state_.intro.logoY = std::min(targetY, state_.intro.logoY + config_.introStepPx);
    return;
  }

  if (state_.intro.settledAtMs == 0) {
    state_.intro.settledAtMs = GetTickCount64();
  }

  const ULONGLONG elapsed = GetTickCount64() - state_.intro.settledAtMs;
  if (elapsed >= config_.introCreditDelayMs && state_.intro.creditOffset > 0) {
    state_.intro.creditOffset = std::max(0, state_.intro.creditOffset - std::max(3, config_.introStepPx / 3));
  }

  if (elapsed >= config_.introHoldMs) {
    state_.intro.active = false;
    SetToast(L"Telemetry viewport ready");
  }
}

void MonixApp::ComputeIntroLogoMetrics(const RECT& clientRect, int& cellW, int& cellH, int& logoWidth, int& logoHeight, int& maxColumns) const {
  const auto logo = IntroLogo();
  maxColumns = 0;
  for (const auto& line : logo) {
    maxColumns = std::max(maxColumns, static_cast<int>(line.size()));
  }

  const int maxWidth = std::max(540, static_cast<int>((clientRect.right - clientRect.left) * 0.78));
  cellW = std::clamp(maxWidth / std::max(1, maxColumns), 7, 16);
  cellH = cellW;
  logoWidth = maxColumns * cellW;
  logoHeight = static_cast<int>(logo.size()) * cellH;
}

LRESULT MonixApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
      info->ptMinTrackSize.x = kWindowMinWidth;
      info->ptMinTrackSize.y = kWindowMinHeight;
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_LBUTTONDOWN: {
      SetFocus(hwnd);
      RECT client {};
      GetClientRect(hwnd, &client);
      ComputeViewport(client);
      POINT wndPt { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      POINT point = MapToViewport(wndPt);
      if (state_.clickDebugMode) {
        state_.clickDebugWnd = wndPt;
        state_.clickDebugBmp = point;
        state_.clickDebugMs = GetTickCount64();
      }
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (!state_.loggedIn) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      PlayClickSound();

      if (state_.taskMenu.visible && HandleTaskMenuClick(point)) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      state_.taskMenu.visible = false;

      if (!state_.intro.active) {
        if (IsCoreMonitorThemeActive()) {
          int direction = 0;
          if (monix::ui::CoreMonitorTheme::HitTestThemeControl(client, point, direction)) {
            CommitSettingMutation(AdjustSetting(SettingId::ThemeMode, direction));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }

          int menuIndex = 0;
          if (monix::ui::CoreMonitorTheme::HitTestMenu(client, point, menuIndex)) {
            state_.coreMonitorMenuIndex = menuIndex;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }

          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }

        if (IsWin98ThemeActive()) {
          auto canvas = monix::ui::Win98Theme::MakeCanvas(client, win98Fonts_, win98Assets_);

          if (state_.updateState.dialogVisible) {
            int result = -1;
            if (monix::ui::Win98Theme::HitTestUpdateDialog(canvas, state_.updateState, point, result)) {
              if (result == -1) {
                state_.updateState.dialogVisible = false;
              } else if (result == 0) {
                if (state_.updateState.updateAvailable && !state_.updateState.downloadUrl.empty()) {
                  ShellExecuteW(nullptr, L"open", state_.updateState.downloadUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
                state_.updateState.dialogVisible = false;
              } else if (result == 1) {
                state_.updateState.dialogVisible = false;
              }
              InvalidateRect(hwnd, nullptr, FALSE);
              return 0;
            }
          }

          int menuIndex = 0;
          if (monix::ui::Win98Theme::HitTestMenuBar(canvas, point, menuIndex)) {
            state_.coreMonitorMenuIndex = menuIndex;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }

          int tabIndex = 0;
          if (monix::ui::Win98Theme::HitTestTabs(client, point, tabIndex)) {
            state_.coreMonitorMenuIndex = tabIndex;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }

          const int activeTab = std::clamp(state_.coreMonitorMenuIndex, 0, 4);
          switch (activeTab) {
            case 0: {
              const int contentY = 94;
              const int logBtn = monix::ui::Win98Theme::HitTestLogToolbar(client, point, contentY);
              if (logBtn >= 0) {
                switch (logBtn) {
                  case 0: state_.logs.clear(); break;
                  case 1: state_.livePaused = !state_.livePaused; break;
                  case 2: break;
                  case 3: break;
                  case 4: break;
                  case 5: break;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              const int filterBtn = monix::ui::Win98Theme::HitTestLogFilters(client, point, contentY, &state_.logs);
              if (filterBtn >= 0) {
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              break;
            }
            case 1: {
              const int toolbarBtn = monix::ui::Win98Theme::HitTestTaskToolbar(client, point);
              if (toolbarBtn >= 0) {
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              auto taskHit = monix::ui::Win98Theme::HitTestTaskRow(client, point);
              if (taskHit.hit && taskHit.index >= 0) {
                std::vector<monix::ProcessInfo> sorted = state_.snapshot.processes;
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
                  if (a.cpuPct != b.cpuPct) return a.cpuPct > b.cpuPct;
                  return a.ramBytes > b.ramBytes;
                });
                const int processIndex = state_.taskScroll + taskHit.index;
                if (processIndex >= 0 && processIndex < static_cast<int>(sorted.size())) {
                  win98SelectedTaskPid_ = sorted[processIndex].pid;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              const int scrollDir = monix::ui::Win98Theme::HitTestTaskScrollbar(client, point);
              if (scrollDir != 0) {
                const int maxScroll = std::max(0, static_cast<int>(state_.snapshot.processes.size()) - 49);
                state_.taskScroll = std::clamp(state_.taskScroll + scrollDir * 3, 0, maxScroll);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              break;
            }
            case 2: {
              const int scramBtn = monix::ui::Win98Theme::HitTestScramButton(client, point);
              if (scramBtn >= 0) {
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              break;
            }
            case 3: {
              break;
            }
            case 4: {
              int catIdx = 0;
              if (monix::ui::Win98Theme::HitTestSettingsCategories(client, point, catIdx)) {
                win98SettingsCategory_ = catIdx;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
              }
              int toggleIdx = -1;
              if (monix::ui::Win98Theme::HitTestSettingsToggle(client, point, toggleIdx)) {
                if (toggleIdx >= 0) {
                  InvalidateRect(hwnd, nullptr, FALSE);
                  return 0;
                }
              }
              int adjRow = -1, adjDir = 0;
              if (monix::ui::Win98Theme::HitTestSettingsAdjust(client, point, adjRow, adjDir)) {
                if (adjRow >= 0) {
                  InvalidateRect(hwnd, nullptr, FALSE);
                  return 0;
                }
              }
              break;
            }
          }

          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }

        Tab tab = Tab::Log;
        if (HitTestTabs(client, point, tab)) {
          if (tab != state_.activeTab) {
            state_.activeTab = tab;
          }
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }

        if (state_.activeTab == Tab::Tasks) {
          const int processIndex = HitTestTaskRow(client, point);
          if (processIndex >= 0) {
            state_.selectedTaskIndex = processIndex;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        } else if (state_.activeTab == Tab::Log) {
          if (HitTestLogToolbar(client, point)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
          if (HitTestLogFilters(client, point)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        } else if (state_.activeTab == Tab::Settings) {
          if (HandleSettingsClick(client, point)) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
          }
        }
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_RBUTTONDOWN: {
      PlayClickSound();
      RECT client {};
      GetClientRect(hwnd, &client);
      ComputeViewport(client);
      POINT wndPt { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
      POINT point = MapToViewport(wndPt);
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (!state_.intro.active && IsCoreMonitorThemeActive()) {
        state_.taskMenu.visible = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.intro.active && state_.activeTab == Tab::Tasks) {
        const int processIndex = HitTestTaskRow(client, point);
        if (processIndex >= 0) {
          state_.selectedTaskIndex = processIndex;
          OpenTaskMenu(client, point, processIndex);
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
      }
      state_.taskMenu.visible = false;
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MOUSEMOVE: {
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (state_.taskMenu.visible) {
        RECT client {};
        GetClientRect(hwnd, &client);
        ComputeViewport(client);
        POINT point { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        point = MapToViewport(point);
        if (PtInRect(&state_.taskMenu.rect, point)) {
          state_.taskMenu.hoverIndex = std::clamp(static_cast<int>((point.y - state_.taskMenu.rect.top) / kTaskMenuItemHeight), 0, 8);
        } else {
          state_.taskMenu.hoverIndex = -1;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_MOUSEWHEEL: {
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (state_.intro.active) {
        return 0;
      }
      if (IsCoreMonitorThemeActive()) {
        return 0;
      }

      const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
      const int step = delta > 0 ? -1 : 1;
      if (state_.activeTab == Tab::Log) {
        RECT client {};
        GetClientRect(hwnd, &client);
        const RECT contentRect = ContentRect(client);
        const RECT listRect {
          contentRect.left + 14,
          contentRect.top + 46,
          contentRect.right - 14,
          contentRect.bottom - 14
        };
        const int maxScroll = std::max(0, static_cast<int>(state_.logs.size()) - VisibleLogLines(listRect));
        state_.logScroll = std::clamp(state_.logScroll + step, 0, maxScroll);
      } else if (state_.activeTab == Tab::Tasks) {
        const int maxScroll = std::max(0, static_cast<int>(state_.snapshot.processes.size()) - 1);
        state_.taskScroll = std::clamp(state_.taskScroll + step, 0, maxScroll);
      } else if (state_.activeTab == Tab::Settings && state_.shaderBrowser.subTab == monix::renderer_vk::SettingsSubTab::Shaders) {
        int maxScroll = 0;
        if (shaderBrowserPanel_) {
          const auto cats = shaderBrowserPanel_->categoryGroups();
          int totalRows = 0;
          for (const auto& cat : cats) totalRows += static_cast<int>(cat.entryIndices.size());
          maxScroll = std::max(0, totalRows - 15);
        }
        state_.shaderBrowser.scrollOffset = std::clamp(state_.shaderBrowser.scrollOffset + step, 0, maxScroll);
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_CHAR: {
      std::lock_guard<std::mutex> lock(stateMutex_);
      if (!state_.loggedIn) {
        kernel_.HandleChar(wParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.intro.active && IsCoreMonitorThemeActive()) {
        return 0;
      }
      if (state_.activeTab == Tab::Settings &&
          state_.shaderBrowser.subTab == monix::renderer_vk::SettingsSubTab::Shaders &&
          state_.shaderSearchFocused) {
        if (wParam == VK_BACK) {
          if (!state_.shaderSearchText.empty()) {
            state_.shaderSearchText.pop_back();
          }
        } else if (wParam >= 32 && wParam < 127) {
          state_.shaderSearchText += static_cast<wchar_t>(wParam);
        }
        if (shaderBrowserPanel_) {
          shaderBrowserPanel_->setSearchQuery(WideToUtf8(state_.shaderSearchText));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_KEYDOWN: {
      std::lock_guard<std::mutex> lock(stateMutex_);
      const bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      if (!state_.loggedIn && ctrlHeld && shiftHeld && wParam == 'K') {
        kernel_.SkipTests();
        SetToast(L"TESTS SKIPPED (DEV MODE) — Ctrl+Shift+K");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (ctrlHeld && shiftHeld && wParam == 'L') {
        state_.clickDebugMode = !state_.clickDebugMode;
        SetToast(state_.clickDebugMode ? L"CLICK DEBUG: ON" : L"CLICK DEBUG: OFF");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (!state_.loggedIn) {
        bool loggedIn = false, shutdown = false;
        kernel_.HandleKeyDown(wParam, loggedIn, shutdown);
        if (shutdown) {
          PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (state_.intro.active) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (IsCoreMonitorThemeActive()) {
        if (wParam >= '1' && wParam <= '9') {
          state_.coreMonitorMenuIndex = static_cast<int>(wParam - '1');
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_UP) {
          state_.coreMonitorMenuIndex = (state_.coreMonitorMenuIndex + monix::ui::CoreMonitorTheme::kMenuCount - 1) % monix::ui::CoreMonitorTheme::kMenuCount;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_DOWN) {
          state_.coreMonitorMenuIndex = (state_.coreMonitorMenuIndex + 1) % monix::ui::CoreMonitorTheme::kMenuCount;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if ((wParam == VK_LEFT || wParam == VK_RIGHT) &&
            state_.coreMonitorMenuIndex == monix::ui::CoreMonitorTheme::kMenuCount - 1) {
          CommitSettingMutation(AdjustSetting(SettingId::ThemeMode, wParam == VK_LEFT ? -1 : 1));
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_F5 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
          state_.logs.clear();
          state_.counters = SessionCounters{};
          state_.logScroll = 0;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (wParam == VK_ESCAPE) {
          state_.taskMenu.visible = false;
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        return 0;
      }
      if (wParam >= '1' && wParam <= '6') {
        Tab newTab = static_cast<Tab>(wParam - '1');
        if (newTab != state_.activeTab) {
          state_.activeTab = newTab;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_ESCAPE) {
        state_.taskMenu.visible = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (state_.activeTab == Tab::Settings && HandleSettingsKey(wParam)) {
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (ctrlHeld && wParam == 'F' && state_.activeTab == Tab::Settings) {
        state_.shaderBrowser.subTab = monix::renderer_vk::SettingsSubTab::Shaders;
        state_.shaderSearchFocused = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_F5 && state_.activeTab == Tab::Settings &&
          state_.shaderBrowser.subTab == monix::renderer_vk::SettingsSubTab::Shaders) {
        if (shaderLibrary_) shaderLibrary_->rescan();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }



      if (wParam == VK_F1) {
        // Toggle pass output debug view
        openGl_.debugViewMode = (openGl_.debugViewMode == 1) ? 0 : 1;
        openGl_.debugPassIndex = 0;
        std::wstring msg = openGl_.debugViewMode == 1
          ? L"Debug: Pass output (F1=off, Up/Down=pass)"
          : L"Debug: OFF";
        SetToast(msg);
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=pass");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F2) {
        // Toggle InfoCache texture view
        openGl_.debugViewMode = (openGl_.debugViewMode == 2) ? 0 : 2;
        std::wstring msg = openGl_.debugViewMode == 2
          ? L"Debug: InfoCache geometry"
          : L"Debug: OFF";
        SetToast(msg);
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=infocache");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F3) {
        // Toggle feedback texture view
        openGl_.debugViewMode = (openGl_.debugViewMode == 3) ? 0 : 3;
        std::wstring msg = openGl_.debugViewMode == 3
          ? L"Debug: Feedback history"
          : L"Debug: OFF";
        SetToast(msg);
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=feedback");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F4 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        // Toggle external texture status view
        openGl_.debugViewMode = (openGl_.debugViewMode == 4) ? 0 : 4;
        std::wstring msg;
        if (openGl_.debugViewMode == 4) {
          msg = L"Debug: External textures";
          // Dead GL code removed - shader pipeline no longer used
        } else {
          msg = L"Debug: OFF";
        }
        SetToast(openGl_.debugViewMode == 4 ? L"Debug: External textures" : L"Debug: OFF");
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=external");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }



      if (wParam == VK_F5 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        // Toggle HSM parameters debug view
        openGl_.debugViewMode = (openGl_.debugViewMode == 5) ? 0 : 5;
        std::wstring msg;
        if (openGl_.debugViewMode == 5) {
          msg = L"Debug: HSM Parameters";
          // Dead GL code removed - shader pipeline no longer used
        } else {
          msg = L"Debug: OFF";
        }
        SetToast(openGl_.debugViewMode == 5 ? L"Debug: HSM Parameters" : L"Debug: OFF");
        PushLog(L"OPENGL", L"INFO", msg, ColorRole::UserInput, L"opengl", L"debug", L"mode=hsm");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F5 && (GetKeyState(VK_SHIFT) & 0x8000)) {
        // Shift+F5: reload config (was F5 before Phase 4)
        LoadConfig(true);
        CreateUiFonts();
        SetFrameTimer();
        RequestRefresh();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F6 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        ExportLogsJson();
        SetToast(L"Logs exported to JSON");
        PushLog(L"LOG", L"INFO", L"Structured log export generated in exports folder.", ColorRole::UserInput, L"log", L"export", L"format=json");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F7 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        ExportLogsCsv();
        SetToast(L"Logs exported to CSV");
        PushLog(L"LOG", L"INFO", L"CSV log export generated in exports folder.", ColorRole::UserInput, L"log", L"export", L"format=csv");
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F8 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        const auto candidate = std::max_element(state_.snapshot.processes.begin(), state_.snapshot.processes.end(), [](const ProcessInfo& left, const ProcessInfo& right) {
          const double leftScore = left.cpuPct * 2.0 + (left.ramBytes / 1048576.0) * 0.02 + left.gpuPct * 1.5;
          const double rightScore = right.cpuPct * 2.0 + (right.ramBytes / 1048576.0) * 0.02 + right.gpuPct * 1.5;
          return leftScore < rightScore;
        });
        if (candidate != state_.snapshot.processes.end()) {
          state_.trackedPid = candidate->pid;
          state_.activeTab = Tab::Scram;
          PushLog(L"SCRAM", L"INFO", L"Realtime triage pinned " + candidate->name + L" as the current risk focus.", ColorRole::Scram, L"scram", L"triage", L"action=autotrack pid=" + std::to_wstring(candidate->pid));
          SetToast(L"S.C.R.A.M triage pinned task");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }

      if (wParam == VK_F9 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        SwitchFont(1);
        const auto& font = paths_.fontList[state_.currentFontIndex];
        PushLog(L"ENGINE", L"INFO", L"Font switched to " + font.displayName + L" (" + font.faceName + L").", ColorRole::UserInput, L"engine", L"font", L"name=" + font.displayName);
        SetToast(L"Font: " + font.displayName);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_F10 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        // Screenshot capture to PNG
        if (openGl_.vkAvailable && openGl_.vk.isInitialized()) {
          openGl_.screenshotRequested = true;
          RECT cr;
          GetClientRect(hwnd, &cr);
          openGl_.screenshotWidth = cr.right - cr.left;
          openGl_.screenshotHeight = cr.bottom - cr.top;
          SetToast(L"Screenshot requested (" + std::to_wstring(openGl_.screenshotWidth) + L"x" + std::to_wstring(openGl_.screenshotHeight) + L")");
          PushLog(L"OPENGL", L"INFO", L"Screenshot capture scheduled.", ColorRole::UserInput, L"opengl", L"screenshot", L"action=request");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == 'B' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (!paths_.borderFiles.empty()) {
          currentBorderIndex_ = (currentBorderIndex_ + 1) % static_cast<int>(paths_.borderFiles.size());
          config_.borderImage = paths_.borderFiles[currentBorderIndex_].wstring();
          std::wstring borderName = paths_.borderFiles[currentBorderIndex_].stem().wstring();
          PushLog(L"OPENGL", L"INFO", L"Border switched to " + borderName + L".", ColorRole::UserInput, L"opengl", L"border", L"name=" + borderName);
          SetToast(L"Border: " + borderName);
        } else {
          SetToast(L"No border images found");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (wParam == VK_F11) {
        SpawnDemoLogs();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }



      return 0;
    }
    case WM_TIMER: {
      RECT client {};
      GetClientRect(hwnd, &client);
      ComputeViewport(client);
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        TickAnimations(state_.viewport_);
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }
    case WM_MONIX_UPDATE:
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps {};
      HDC hdc = BeginPaint(hwnd, &ps);
      RECT client {};
      GetClientRect(hwnd, &client);
      LARGE_INTEGER frameStart, frameEnd, freq;
      QueryPerformanceFrequency(&freq);
      QueryPerformanceCounter(&frameStart);
      bool painted = RenderOpenGlFrame(client);
      if (!painted) {
        HDC memoryDc = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, client.right - client.left, client.bottom - client.top);
        HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
        Render(memoryDc, client);
        BitBlt(hdc, 0, 0, client.right, client.bottom, memoryDc, 0, 0, SRCCOPY);
        SelectObject(memoryDc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memoryDc);
      }
      QueryPerformanceCounter(&frameEnd);
      {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.snapshot.frameTimeMs = static_cast<double>(frameEnd.QuadPart - frameStart.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      KillTimer(hwnd, kFrameTimerId);
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

void MonixApp::DrawTextRect(HDC dc, const RECT& rect, const std::wstring& text, ColorRole color, HFONT font, UINT format) const {
  SetBkMode(dc, TRANSPARENT);
  HGDIOBJ oldFont = SelectObject(dc, font);

  RECT mainRect = rect;
  SetTextColor(dc, ResolveColor(color));
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &mainRect, format);

  SelectObject(dc, oldFont);
}

void MonixApp::DrawTextLine(HDC dc, int x, int y, int width, const std::wstring& text, ColorRole color, HFONT font, UINT format) const {
  RECT rect { x, y, x + width, y + std::max(bodyLineHeight_ + 8, 28) };
  DrawTextRect(dc, rect, text, color, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS | format);
}

void MonixApp::DrawPanel(HDC dc, const RECT& rect, const std::wstring& title, ColorRole accent, HFONT titleFont) const {
  FillSolid(dc, rect, RGB(8, 10, 8));
  DrawRectOutline(dc, rect, ResolveColor(ColorRole::Accent));
  RECT header = rect;
  header.bottom = header.top + 38;
  FillSolid(dc, header, RGB(10, 14, 10));
  DrawRectOutline(dc, header, ResolveColor(accent));
  DrawTextRect(dc, RECT { header.left + 12, header.top + 4, header.right - 12, header.bottom }, title, accent, titleFont, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawProgressBar(HDC dc, const RECT& rect, double pct, ColorRole accent) const {
  FillSolid(dc, rect, RGB(7, 18, 9));
  DrawRectOutline(dc, rect, ResolveColor(ColorRole::Accent));
  RECT fill = ShrinkRect(rect, 2);
  fill.right = fill.left + static_cast<int>((fill.right - fill.left) * std::clamp(pct, 0.0, 100.0) / 100.0);
  if (fill.right > fill.left) {
    FillSolid(dc, fill, ResolveColor(accent));
  }
}

void MonixApp::DrawSparkline(HDC dc, const RECT& rect, const std::vector<double>& values, double maxValue, ColorRole color) const {
  FillSolid(dc, rect, RGB(4, 12, 5));
  DrawRectOutline(dc, rect, ResolveColor(ColorRole::Accent));
  if (values.size() < 2 || maxValue <= 0.0) {
    return;
  }

  HPEN pen = CreatePen(PS_SOLID, 2, ResolveColor(color));
  HGDIOBJ oldPen = SelectObject(dc, pen);
  const int width = std::max(1L, rect.right - rect.left - 10);
  const int height = std::max(1L, rect.bottom - rect.top - 10);
  const double step = static_cast<double>(width) / static_cast<double>(std::max<std::size_t>(1, values.size() - 1));

  for (std::size_t i = 0; i < values.size(); ++i) {
    const double normalized = std::clamp(values[i] / maxValue, 0.0, 1.0);
    const int x = rect.left + 5 + static_cast<int>(i * step);
    const int y = rect.bottom - 5 - static_cast<int>(normalized * height);
    if (i == 0) {
      MoveToEx(dc, x, y, nullptr);
    } else {
      LineTo(dc, x, y);
    }
  }

  SelectObject(dc, oldPen);
  DeleteObject(pen);
}

RECT MonixApp::ContentRect(const RECT& clientRect) const {
  return RECT { clientRect.left + 72, clientRect.top + 118, clientRect.right - 72, clientRect.bottom - 66 };
}

std::array<RECT, 6> MonixApp::TabRects(const RECT& clientRect) const {
  const int top = 26;
  const int height = 54;
  const int startX = 72;
  const std::array<int, 6> baseWidths { 170, 220, 220, 260, 170, 250 };
  const int badgeWidth = 290;
  const int available = std::max(900, static_cast<int>(clientRect.right - startX - 72 - badgeWidth));
  const double scale = std::min(1.0, static_cast<double>(available) / 1290.0);
  std::array<RECT, 6> rects {};
  int cursor = startX;
  for (std::size_t i = 0; i < rects.size(); ++i) {
    const int width = static_cast<int>(baseWidths[i] * scale);
    rects[i] = RECT { cursor, top, cursor + width, top + height };
    cursor += width;
  }
  return rects;
}

std::wstring MonixApp::ComposeLogLine(const LogEntry& entry) const {
  std::wstring line;
  switch (config_.logViewMode) {
    case LogViewMode::Timeline:
      line = L">> " + entry.time + L" [" + entry.domain + L"] " + entry.severity + L" " + entry.message;
      break;
    case LogViewMode::Compact:
      line = L"[" + entry.severity + L"] " + entry.message;
      break;
    case LogViewMode::Structured:
    default:
      line = L"#" + std::to_wstring(entry.eventId) +
        L" " + entry.time +
        L" " + entry.severity +
        L" " + entry.domain +
        L" mod=" + entry.module +
        L" pid=" + std::to_wstring(entry.processId) +
        L" tid=" + std::to_wstring(entry.threadId) +
        L" :: " + entry.message;
      if (!entry.metadata.empty()) {
        line += L" {" + entry.metadata + L"}";
      }
      break;
  }
  if (entry.repeatCount > 1) {
    line += L" x" + std::to_wstring(entry.repeatCount);
  }
  return line;
}

void MonixApp::DrawBackground(HDC dc, const RECT& clientRect) {
  FillSolid(dc, clientRect, RGB(5, 5, 5));

  const int height = std::max(1L, clientRect.bottom - clientRect.top);
  const int bands = 42;
  for (int index = 0; index < bands; ++index) {
    const int bandTop = clientRect.top + (height * index) / bands;
    const int bandBottom = clientRect.top + (height * (index + 1)) / bands;
    const double position = static_cast<double>(bandTop - clientRect.top) / static_cast<double>(height);
    const double centerBias = 1.0 - std::fabs((position * 2.0) - 1.0);
    const int green = static_cast<int>(10 + centerBias * 12.0);
    RECT bandRect { clientRect.left, bandTop, clientRect.right, std::max(bandTop + 1, bandBottom) };
    FillSolid(dc, bandRect, RGB(4, green, 6));
  }

  RECT topFalloff { clientRect.left, clientRect.top, clientRect.right, clientRect.top + 90 };
  FillSolid(dc, topFalloff, RGB(8, 12, 8));
}

void MonixApp::DrawTabs(HDC dc, const RECT& clientRect) {
  const auto rects = TabRects(clientRect);
  const std::array<std::wstring, 6> labels { L"LOG", L"TASKS", L"HARDWARE", L"NETWORK", L"SCRAM", L"SETTINGS" };

  for (std::size_t i = 0; i < rects.size(); ++i) {
    RECT rect = rects[i];
    const Tab tab = static_cast<Tab>(i);
    const bool active = state_.activeTab == tab;
    FillSolid(dc, rect, active ? RGB(22, 86, 38) : RGB(8, 12, 8));
    DrawRectOutline(dc, rect, active ? ResolveColor(ColorRole::Primary) : ResolveColor(ColorRole::Accent));
    DrawTextRect(dc, rect, labels[i], active ? ColorRole::White : ColorRole::Primary, tabFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  RECT badge {
    rects.back().right + 12,
    rects.back().top,
    clientRect.right - 72,
    rects.back().bottom
  };
  FillSolid(dc, badge, RGB(8, 12, 8));
  DrawRectOutline(dc, badge, ResolveColor(ColorRole::Accent));
  const std::wstring badgeText = L"MONIX CORE 3.0 :: " + state_.snapshot.host;
  DrawTextRect(dc, badge, badgeText, ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawFooter(HDC dc, const RECT& clientRect) {
  RECT footer { 72, clientRect.bottom - 50, clientRect.right - 72, clientRect.bottom - 18 };
  FillSolid(dc, footer, RGB(8, 12, 8));
  DrawRectOutline(dc, footer, ResolveColor(ColorRole::Accent));

  std::wstring shaderName = L"base";
  std::wstring fontDisplayName = L"Terminal";
  if (!paths_.fontList.empty() && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
    fontDisplayName = paths_.fontList[state_.currentFontIndex].displayName;
  }
  const std::wstring footerText =
    L"Delay " + std::to_wstring(config_.telemetryIntervalMs) + L"ms | "
    + fontDisplayName + L" | "
    + shaderName + L" | "
    L"View " + LogViewModeText(config_.logViewMode) + L" | "
    L"F1 debug | F2 cache | F3 feedback | F4 textures | F5 reload | F6 JSON | F7 CSV | F8 triage | F9 font | F10 shot | F11 demo";
  DrawTextRect(dc, RECT { footer.left + 12, footer.top + 2, footer.right - 12, footer.bottom }, footerText, ColorRole::Dim, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawToast(HDC dc, const RECT& clientRect) {
  if (state_.toastMessage.empty()) {
    return;
  }

  RECT toast {
    clientRect.right - 390,
    clientRect.bottom - 108,
    clientRect.right - 72,
    clientRect.bottom - 68
  };
  FillSolid(dc, toast, RGB(10, 16, 10));
  DrawRectOutline(dc, toast, ResolveColor(ColorRole::UserInput));
  DrawTextRect(dc, RECT { toast.left + 10, toast.top + 2, toast.right - 10, toast.bottom }, state_.toastMessage, ColorRole::UserInput, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawClickDebug(HDC dc, const RECT& clientRect) {
  if (!state_.clickDebugMode) return;

  const int cw = clientRect.right - clientRect.left;
  const int ch = clientRect.bottom - clientRect.top;

  double scale = 4.0 * ch / (3.0 * cw);
  int visW = static_cast<int>(cw * scale + 0.5);
  int visX = (cw - visW) / 2;

  DrawRectOutline(dc, RECT { clientRect.left + visX, clientRect.top, clientRect.left + visX + visW, clientRect.top + ch }, RGB(120, 120, 120));

  int cx = clientRect.left + cw / 2;
  int cy = clientRect.top + ch / 2;
  HPEN crossPen = CreatePen(PS_DOT, 1, RGB(100, 100, 100));
  HGDIOBJ oldCrossPen = SelectObject(dc, crossPen);
  MoveToEx(dc, cx - 20, cy, nullptr);
  LineTo(dc, cx + 20, cy);
  MoveToEx(dc, cx, cy - 20, nullptr);
  LineTo(dc, cx, cy + 20);
  SelectObject(dc, oldCrossPen);
  DeleteObject(crossPen);

  POINT cursorPt {};
  GetCursorPos(&cursorPt);
  ScreenToClient(hwnd_, &cursorPt);
  POINT cursorBmp = MapToViewport(cursorPt);

  HPEN curPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
  HGDIOBJ oldCurPen = SelectObject(dc, curPen);
  int cr = 8;
  Ellipse(dc, cursorBmp.x - cr, cursorBmp.y - cr, cursorBmp.x + cr, cursorBmp.y + cr);
  SelectObject(dc, oldCurPen);
  DeleteObject(curPen);

  if (state_.clickDebugMs > 0 && GetTickCount64() - state_.clickDebugMs < 5000) {
    HPEN hitPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HGDIOBJ oldHitPen = SelectObject(dc, hitPen);
    HBRUSH hitBrush = CreateSolidBrush(RGB(255, 255, 255));
    HGDIOBJ oldBrush = SelectObject(dc, hitBrush);
    int r = 6;
    Ellipse(dc, state_.clickDebugBmp.x - r, state_.clickDebugBmp.y - r, state_.clickDebugBmp.x + r, state_.clickDebugBmp.y + r);
    SelectObject(dc, oldHitPen);
    DeleteObject(hitPen);
    SelectObject(dc, oldBrush);
    DeleteObject(hitBrush);
  }

  HFONT oldFont = static_cast<HFONT>(SelectObject(dc, smallFont_.get()));
  SetBkMode(dc, OPAQUE);
  SetBkColor(dc, RGB(0, 0, 0));
  SetTextColor(dc, RGB(255, 255, 255));

  wchar_t line1[160];
  swprintf_s(line1, L"CURSOR WND(%ld,%ld) -> BMP(%ld,%ld) | visX=%d visW=%d cw=%d ch=%d",
    cursorPt.x, cursorPt.y, cursorBmp.x, cursorBmp.y, visX, visW, cw, ch);
  RECT textR1 { clientRect.left + 4, clientRect.bottom - 50, clientRect.right - 4, clientRect.bottom - 30 };
  DrawText(dc, line1, -1, &textR1, DT_LEFT | DT_SINGLELINE);

  wchar_t line2[160];
  if (state_.clickDebugMs > 0 && GetTickCount64() - state_.clickDebugMs < 5000) {
    swprintf_s(line2, L"CLICK WND(%ld,%ld) -> BMP(%ld,%ld)",
      state_.clickDebugWnd.x, state_.clickDebugWnd.y, state_.clickDebugBmp.x, state_.clickDebugBmp.y);
  } else {
    wcscpy_s(line2, L"Click anywhere to debug mapping");
  }
  RECT textR2 { clientRect.left + 4, clientRect.bottom - 28, clientRect.right - 4, clientRect.bottom - 8 };
  DrawText(dc, line2, -1, &textR2, DT_LEFT | DT_SINGLELINE);

  SelectObject(dc, oldFont);
}

void MonixApp::DrawNotifications(HDC dc, const RECT& clientRect) {
  int y = 84;
  for (std::size_t i = 0; i < state_.notifications.size(); ++i) {
    const auto& item = state_.notifications[i];
    RECT card {
      clientRect.right - kNotificationWidth - 18,
      y,
      clientRect.right - 18,
      y + kNotificationHeight
    };
    FillSolid(dc, card, RGB(10, 14, 10));
    DrawRectOutline(dc, card, ResolveColor(item.color));
    DrawTextRect(dc, RECT { card.left + 12, card.top + 8, card.right - 12, card.top + 28 }, item.title, item.color, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    DrawTextRect(dc, RECT { card.left + 12, card.top + 28, card.right - 12, card.bottom - 8 }, item.message, ColorRole::White, smallFont_.get(), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    y += kNotificationHeight + 10;
  }
}

void MonixApp::DrawIntro(HDC dc, const RECT& clientRect) {
  const auto logo = IntroLogo();
  int cellW = 0;
  int cellH = 0;
  int logoWidth = 0;
  int logoHeight = 0;
  int maxColumns = 0;
  ComputeIntroLogoMetrics(clientRect, cellW, cellH, logoWidth, logoHeight, maxColumns);

  const int logoLeft = clientRect.left + ((clientRect.right - clientRect.left) - logoWidth) / 2;
  int y = state_.intro.logoY;
  for (const auto& line : logo) {
    int x = logoLeft;
    for (wchar_t ch : line) {
      if (ch == L'█' || ch == L'░') {
        RECT cell { x, y, x + cellW - 1, y + cellH - 1 };
        FillSolid(dc, cell, ch == L'█' ? ResolveColor(ColorRole::Primary) : ResolveColor(ColorRole::Dim));
      }
      x += cellW;
    }
    y += cellH;
  }

  if (state_.intro.settledAtMs != 0 && GetTickCount64() - state_.intro.settledAtMs >= config_.introCreditDelayMs) {
    const int creditY = state_.intro.logoY + logoHeight + 20 - state_.intro.creditOffset;
    DrawTextRect(dc, RECT { clientRect.left, creditY, clientRect.right, creditY + smallLineHeight_ + 10 }, L"Created by: belfegor442", ColorRole::Dim, bodyFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  DrawTextRect(dc, RECT { clientRect.left, clientRect.bottom - 140, clientRect.right, clientRect.bottom - 88 }, L"Booting native telemetry, S.C.R.A.M watcher and kernel probes...", ColorRole::Dim, bodyFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

bool MonixApp::IsCoreMonitorThemeActive() const {
  return config_.themeMode == monix::ui::kCoreMonitorThemeMode;
}

bool MonixApp::IsWin98ThemeActive() const {
  return config_.themeMode == monix::ui::kWin98ThemeMode;
}

void MonixApp::RenderCoreMonitorTheme(HDC dc, const RECT& clientRect) {
  monix::ui::CoreMonitorThemeContext ctx;
  ctx.snapshot = &state_.snapshot;
  ctx.logs = &state_.logs;
  ctx.counters = &state_.counters;
  ctx.cpuHistory = &state_.cpuHistory;
  ctx.ramHistory = &state_.ramHistory;
  ctx.gpuHistory = &state_.gpuHistory;
  ctx.netHistory = &state_.netHistory;
  ctx.netUploadHistory = &state_.netUploadHistory;
  ctx.config = config_;
  ctx.fonts.title = titleFont_.get();
  ctx.fonts.body = bodyFont_.get();
  ctx.fonts.smallText = smallFont_.get();
  ctx.fonts.logText = logFont_.get();
  ctx.fonts.bodyLineHeight = bodyLineHeight_;
  ctx.fonts.smallLineHeight = smallLineHeight_;
  ctx.fonts.logLineHeight = logLineHeight_;
  ctx.rendererName = openGl_.renderer.empty() ? L"VULKAN 1.3" : openGl_.renderer;
  ctx.shaderName = paths_.crtShaderFile.empty() ? L"crt-lottes-with-bezel" : paths_.crtShaderFile.stem().wstring();
  ctx.fontName = L"Terminal";
  if (!paths_.fontList.empty() && state_.currentFontIndex >= 0 && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
    ctx.fontName = paths_.fontList[state_.currentFontIndex].displayName;
  }
  ctx.menuIndex = state_.coreMonitorMenuIndex;
  ctx.livePaused = state_.livePaused || config_.pauseLiveLogs;
  ctx.frameCount = openGl_.frameCount;
  monix::ui::CoreMonitorTheme::Render(dc, clientRect, ctx);
}

void MonixApp::RenderWin98Theme(HDC dc, const RECT& clientRect) {
  if (!win98AssetsLoaded_) {
    monix::ui::Win98Theme::LoadAssets(win98Assets_, paths_.rootDir);
    win98AssetsLoaded_ = true;
  }
  monix::ui::CoreMonitorThemeContext ctx;
  ctx.snapshot = &state_.snapshot;
  ctx.logs = &state_.logs;
  ctx.counters = &state_.counters;
  ctx.cpuHistory = &state_.cpuHistory;
  ctx.ramHistory = &state_.ramHistory;
  ctx.gpuHistory = &state_.gpuHistory;
  ctx.netHistory = &state_.netHistory;
  ctx.netUploadHistory = &state_.netUploadHistory;
  ctx.config = config_;
  ctx.fonts.title = titleFont_.get();
  ctx.fonts.body = bodyFont_.get();
  ctx.fonts.smallText = smallFont_.get();
  ctx.fonts.logText = logFont_.get();
  ctx.fonts.bodyLineHeight = bodyLineHeight_;
  ctx.fonts.smallLineHeight = smallLineHeight_;
  ctx.fonts.logLineHeight = logLineHeight_;
  ctx.rendererName = openGl_.renderer.empty() ? L"VULKAN 1.3" : openGl_.renderer;
  ctx.shaderName = paths_.crtShaderFile.empty() ? L"crt-lottes-with-bezel" : paths_.crtShaderFile.stem().wstring();
  ctx.fontName = L"Terminal";
  if (!paths_.fontList.empty() && state_.currentFontIndex >= 0 && state_.currentFontIndex < static_cast<int>(paths_.fontList.size())) {
    ctx.fontName = paths_.fontList[state_.currentFontIndex].displayName;
  }
  ctx.menuIndex = state_.coreMonitorMenuIndex;
  ctx.livePaused = state_.livePaused || config_.pauseLiveLogs;
  ctx.frameCount = openGl_.frameCount;
  ctx.updateState = &state_.updateState;
  ctx.settingsCategory = win98SettingsCategory_;
  ctx.taskScroll = state_.taskScroll;
  ctx.selectedTaskPid = win98SelectedTaskPid_;
  monix::ui::Win98Theme::Render(dc, clientRect, ctx, win98Fonts_, win98Assets_, paths_.rootDir);
}

void MonixApp::Render(HDC dc, const RECT& clientRect) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  DrawBackground(dc, clientRect);

  if (!state_.loggedIn) {
    kernel_.Update();
    if (!kernel_.IsRunning()) {
      kernelDisplay_.Draw(dc, clientRect, kernel_, bodyFont_.get(), smallFont_.get(), dpiScale_);
      DrawToast(dc, clientRect);
      DrawClickDebug(dc, clientRect);
      return;
    }
    state_.loggedIn = true;
    state_.intro.active = config_.introEnabled;
    PushLog(L"SYSTEM", L"SUCCESS", L"Kernel initialized — terminal unlocked", ColorRole::Success);
  }

  if (state_.intro.active) {
    DrawIntro(dc, clientRect);
    return;
  }

  if (IsCoreMonitorThemeActive()) {
    RenderCoreMonitorTheme(dc, clientRect);
    DrawToast(dc, clientRect);
    DrawClickDebug(dc, clientRect);
    return;
  }

  if (IsWin98ThemeActive()) {
    RenderWin98Theme(dc, clientRect);
    DrawToast(dc, clientRect);
    DrawClickDebug(dc, clientRect);
    return;
  }

  DrawTabs(dc, clientRect);

  switch (state_.activeTab) {
    case Tab::Log: DrawLogView(dc, clientRect); break;
    case Tab::Tasks: DrawTasksView(dc, clientRect); break;
    case Tab::Hardware: DrawHardwareView(dc, clientRect); break;
    case Tab::Network: DrawNetworkView(dc, clientRect); break;
    case Tab::Scram: DrawScramView(dc, clientRect); break;
    case Tab::Settings: DrawSettingsView(dc, clientRect); break;
  }

  if (state_.taskMenu.visible) {
    DrawTaskContextMenu(dc);
  }

  DrawFooter(dc, clientRect);
  DrawToast(dc, clientRect);
  DrawClickDebug(dc, clientRect);
  DrawNotifications(dc, clientRect);
}

LogToolbarRect MonixApp::LogToolbarRects(const RECT& clientRect) const {
  LogToolbarRect r;
  const RECT outer = ContentRect(clientRect);
  const int btnW = 90;
  const int btnH = 28;
  const int gap = 6;
  const int startY = outer.top + 44;
  int x = outer.left + 14;
  r.clear = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.pause = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.search = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.json = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.csv = { x, startY, x + btnW, startY + btnH }; x += btnW + gap;
  r.copy = { x, startY, x + btnW, startY + btnH };
  return r;
}

std::array<RECT, 6> MonixApp::LogFilterRects(const RECT& clientRect) const {
  std::array<RECT, 6> rects{};
  const RECT outer = ContentRect(clientRect);
  const int btnH = 26;
  const int gap = 8;
  const int sidebarW = 300;
  const int availableW = (outer.right - sidebarW - 14) - outer.left - 28;
  const int btnW = (availableW - gap * 5) / 6;
  int x = outer.left + 14;
  for (std::size_t i = 0; i < 6; ++i) {
    rects[i] = { x, outer.top + 80, x + btnW, outer.top + 80 + btnH };
    x += btnW + gap;
  }
  return rects;
}

bool MonixApp::HitTestLogToolbar(const RECT& clientRect, POINT point) {
  auto r = LogToolbarRects(clientRect);
  if (PtInRect(&r.clear, point)) {
    state_.logs.clear();
    state_.counters = SessionCounters{};
    state_.logScroll = 0;
    SetToast(L"Logs cleared");
    return true;
  }
  if (PtInRect(&r.pause, point)) {
    state_.livePaused = !state_.livePaused;
    SetToast(state_.livePaused ? L"Live logs paused" : L"Live logs resumed");
    return true;
  }
  if (PtInRect(&r.search, point)) {
    SetToast(L"Search: use Ctrl+F to search logs");
    return true;
  }
  if (PtInRect(&r.json, point)) {
    ExportLogsJson();
    SetToast(L"Exported logs as JSON");
    return true;
  }
  if (PtInRect(&r.csv, point)) {
    ExportLogsCsv();
    SetToast(L"Exported logs as CSV");
    return true;
  }
  if (PtInRect(&r.copy, point)) {
    if (!state_.logs.empty()) {
      std::wstring all;
      for (const auto& e : state_.logs) {
        all += ComposeLogLine(e) + L"\r\n";
      }
      CopyToClipboard(all);
      SetToast(L"Copied all logs to clipboard");
    }
    return true;
  }
  return false;
}

bool MonixApp::HitTestLogFilters(const RECT& clientRect, POINT point) {
  auto rects = LogFilterRects(clientRect);
  const std::array<LogFilter, 6> filters = {
    LogFilter::All, LogFilter::Warn, LogFilter::Net,
    LogFilter::Crit, LogFilter::Err, LogFilter::Kernel
  };
  for (std::size_t i = 0; i < 6; ++i) {
    if (PtInRect(&rects[i], point)) {
      state_.activeLogFilter = filters[i];
      state_.logScroll = 0;
      return true;
    }
  }
  return false;
}

int MonixApp::CountFilteredLogs() const {
  if (state_.activeLogFilter == LogFilter::All) {
    return static_cast<int>(state_.logs.size());
  }
  int count = 0;
  for (const auto& e : state_.logs) {
    switch (state_.activeLogFilter) {
      case LogFilter::Warn:
        if (e.level == LogLevel::Warn) ++count;
        break;
      case LogFilter::Err:
        if (e.level == LogLevel::Error) ++count;
        break;
      case LogFilter::Crit:
        if (e.level == LogLevel::Critical) ++count;
        break;
      case LogFilter::Net:
        if (e.domain == L"NETWORK") ++count;
        break;
      case LogFilter::Kernel:
        if (e.domain == L"KERNEL") ++count;
        break;
      default:
        break;
    }
  }
  return count;
}

void MonixApp::DrawLogToolbar(HDC dc, const RECT& clientRect) {
  auto r = LogToolbarRects(clientRect);
  const int btnH = 30;

  auto drawBtn = [&](RECT btn, const std::wstring& label, ColorRole accent) {
    FillSolid(dc, btn, RGB(8, 12, 8));
    DrawRectOutline(dc, btn, ResolveColor(accent));
    DrawTextRect(dc, btn, label, accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  };

  drawBtn(r.clear, L"CLEAR", ColorRole::White);
  drawBtn(r.pause, state_.livePaused ? L"RESUME" : L"PAUSE", state_.livePaused ? ColorRole::Success : ColorRole::Warning);
  drawBtn(r.search, L"SEARCH", ColorRole::Primary);
  drawBtn(r.json, L"JSON", ColorRole::Dim);
  drawBtn(r.csv, L"CSV", ColorRole::Dim);
  drawBtn(r.copy, L"COPY", ColorRole::Dim);
}

void MonixApp::DrawLogFilterBadges(HDC dc, const RECT& clientRect) {
  auto rects = LogFilterRects(clientRect);
  const std::array<std::wstring, 6> labels = { L"ALL", L"WARN", L"NET", L"CRIT", L"ERR", L"KERNEL" };
  const std::array<ColorRole, 6> colors = {
    ColorRole::White, ColorRole::Warning, ColorRole::Network,
    ColorRole::Fatal, ColorRole::Error, ColorRole::Kernel
  };
  const std::array<int, 6> counts = {
    static_cast<int>(state_.logs.size()),
    state_.counters.warnings,
    state_.counters.network,
    state_.counters.critical,
    state_.counters.errors,
    state_.counters.kernel
  };
  const std::array<LogFilter, 6> filters = {
    LogFilter::All, LogFilter::Warn, LogFilter::Net,
    LogFilter::Crit, LogFilter::Err, LogFilter::Kernel
  };

  for (std::size_t i = 0; i < 6; ++i) {
    const bool active = (state_.activeLogFilter == filters[i]);
    COLORREF bg = active ? RGB(18, 50, 20) : RGB(8, 12, 8);
    FillSolid(dc, rects[i], bg);
    DrawRectOutline(dc, rects[i], active ? ResolveColor(colors[i]) : ResolveColor(ColorRole::Accent));
    std::wstring text = labels[i] + L": " + std::to_wstring(counts[i]);
    RECT textRect = { rects[i].left + 4, rects[i].top, rects[i].right - 4, rects[i].bottom };
    DrawTextRect(dc, textRect, text, active ? colors[i] : ColorRole::Dim, smallFont_.get(),
      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }
}

void MonixApp::DrawLogColumnHeaders(HDC dc, const RECT& logArea) {
  const int headerH = 26;
  RECT header = { logArea.left, logArea.top, logArea.right, logArea.top + headerH };
  FillSolid(dc, header, RGB(10, 16, 10));
  DrawRectOutline(dc, header, ResolveColor(ColorRole::Accent));

  const int totalW = header.right - header.left;
  const int timeW = static_cast<int>(totalW * 0.12);
  const int catW = static_cast<int>(totalW * 0.18);
  const int subcatW = static_cast<int>(totalW * 0.12);
  const int contentW = totalW - timeW - catW - subcatW;

  int x = header.left + 10;
  DrawTextRect(dc, { x, header.top, x + timeW, header.bottom }, L"TIME", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  x += timeW;
  DrawTextRect(dc, { x, header.top, x + catW, header.bottom }, L"CATEGORY", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  x += catW;
  DrawTextRect(dc, { x, header.top, x + subcatW, header.bottom }, L"SUB-CATEGORY", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  x += subcatW;
  DrawTextRect(dc, { x, header.top, header.right - 10, header.bottom }, L"CONTENT", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawLogSidebar(HDC dc, const RECT& sidebarRect) {
  DrawPanel(dc, sidebarRect, L"STATUS", ColorRole::Primary, smallFont_.get());

  const int chartH = 150;
  RECT chartRect = { sidebarRect.left + 14, sidebarRect.top + 46, sidebarRect.right - 14, sidebarRect.top + 46 + chartH };
  FillSolid(dc, chartRect, RGB(4, 4, 5));
  DrawRectOutline(dc, chartRect, ResolveColor(ColorRole::Accent));

  auto drawOverlay = [&](HDC dc, const RECT& rect, const std::vector<double>& values, ColorRole color) {
    if (values.size() < 2) return;
    HPEN pen = CreatePen(PS_SOLID, 2, ResolveColor(color));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const int width = std::max(1, static_cast<int>(rect.right - rect.left - 10));
    const int height = std::max(1, static_cast<int>(rect.bottom - rect.top - 10));
    const double step = static_cast<double>(width) / static_cast<double>(std::max<std::size_t>(1, values.size() - 1));
    for (std::size_t i = 0; i < values.size(); ++i) {
      const double normalized = std::clamp(values[i], 0.0, 1.0);
      const int x = rect.left + 5 + static_cast<int>(i * step);
      const int y = rect.bottom - 5 - static_cast<int>(normalized * height);
      if (i == 0) MoveToEx(dc, x, y, nullptr);
      else LineTo(dc, x, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  };

  drawOverlay(dc, chartRect, state_.logWarnHistory,   ColorRole::Warning);
  drawOverlay(dc, chartRect, state_.logErrHistory,    ColorRole::Error);
  drawOverlay(dc, chartRect, state_.logCritHistory,   ColorRole::Fatal);
  drawOverlay(dc, chartRect, state_.logKernelHistory, ColorRole::Kernel);
  drawOverlay(dc, chartRect, state_.logNetHistory,    ColorRole::Network);

  RECT legendRect = { chartRect.left + 4, chartRect.bottom + 2, chartRect.right - 4, chartRect.bottom + 18 };
  const int dotSpacing = (legendRect.right - legendRect.left) / 5;
  auto drawDot = [&](int x, const std::wstring& label, ColorRole c) {
    HBRUSH br = CreateSolidBrush(ResolveColor(c));
    RECT dot = { x, legendRect.top + 4, x + 8, legendRect.bottom - 4 };
    FillRect(dc, &dot, br);
    DeleteObject(br);
    RECT lr = { x + 10, legendRect.top, x + 50, legendRect.bottom };
    DrawTextRect(dc, lr, label, ColorRole::Dim, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  };
  int lx = legendRect.left;
  drawDot(lx, L"WARN", ColorRole::Warning);   lx += dotSpacing;
  drawDot(lx, L"ERR", ColorRole::Error);      lx += dotSpacing;
  drawDot(lx, L"CRIT", ColorRole::Fatal);     lx += dotSpacing;
  drawDot(lx, L"KERN", ColorRole::Kernel);    lx += dotSpacing;
  drawDot(lx, L"NET", ColorRole::Network);

  RECT counterPanel = { sidebarRect.left + 14, chartRect.bottom + 22, sidebarRect.right - 14, sidebarRect.bottom - 14 };
  FillSolid(dc, counterPanel, RGB(8, 10, 8));
  DrawRectOutline(dc, counterPanel, ResolveColor(ColorRole::Accent));

  RECT cTitle = counterPanel;
  cTitle.bottom = cTitle.top + 30;
  FillSolid(dc, cTitle, RGB(10, 14, 10));
  DrawTextRect(dc, cTitle, L"COUNTER", ColorRole::Engine, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  const int rowH = 28;
  int y = cTitle.bottom + 6;
  auto drawCounter = [&](const std::wstring& label, int value, ColorRole color) {
    if (y + rowH > counterPanel.bottom) return;
    RECT row = { counterPanel.left + 8, y, counterPanel.right - 8, y + rowH };
    DrawTextRect(dc, row, label, ColorRole::Dim, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT valRect = { counterPanel.right - 70, y, counterPanel.right - 8, y + rowH };
    DrawTextRect(dc, valRect, std::to_wstring(value), color, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    y += rowH;
  };

  drawCounter(L"Warnings:", state_.counters.warnings, ColorRole::Warning);
  drawCounter(L"Error:", state_.counters.errors, ColorRole::Error);
  drawCounter(L"Network:", state_.counters.network, ColorRole::Network);
  drawCounter(L"Critical:", state_.counters.critical, ColorRole::Fatal);
  drawCounter(L"Kernel:", state_.counters.kernel, ColorRole::Kernel);
}

void MonixApp::DrawLogView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  DrawPanel(dc, outer, L"LOG CORE", ColorRole::Engine, smallFont_.get());

  const std::wstring counterText =
    L"TOTAL " + std::to_wstring(CountFilteredLogs()) +
    L" | INFO " + std::to_wstring(state_.counters.info) +
    L" | WARN " + std::to_wstring(state_.counters.warnings) +
    L" | ERROR " + std::to_wstring(state_.counters.errors) +
    L" | CRIT " + std::to_wstring(state_.counters.critical);
  DrawTextRect(dc, RECT { outer.left + 220, outer.top + 10, outer.right - 18, outer.top + 38 }, counterText, ColorRole::Dim, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  DrawLogToolbar(dc, clientRect);
  DrawLogFilterBadges(dc, clientRect);

  const int sidebarW = 300;
  const int sidebarGap = 14;
  const int colHeaderH = 26;
  const int contentStartY = outer.top + 114;
  RECT sidebarRect = { outer.right - sidebarW - 14, contentStartY, outer.right - 14, outer.bottom - 14 };
  DrawLogSidebar(dc, sidebarRect);

  RECT logArea = { outer.left + 14, contentStartY, sidebarRect.left - sidebarGap, outer.bottom - 14 };
  DrawLogColumnHeaders(dc, logArea);

  RECT listRect = { logArea.left, logArea.top + colHeaderH, logArea.right, logArea.bottom };
  const int visibleLines = VisibleLogLines(listRect);

  std::vector<int> filteredIndices;
  filteredIndices.reserve(state_.logs.size());
  for (int i = 0; i < static_cast<int>(state_.logs.size()); ++i) {
    const auto& e = state_.logs[i];
    switch (state_.activeLogFilter) {
      case LogFilter::All:
        filteredIndices.push_back(i);
        break;
      case LogFilter::Warn:
        if (e.level == LogLevel::Warn) filteredIndices.push_back(i);
        break;
      case LogFilter::Err:
        if (e.level == LogLevel::Error) filteredIndices.push_back(i);
        break;
      case LogFilter::Crit:
        if (e.level == LogLevel::Critical) filteredIndices.push_back(i);
        break;
      case LogFilter::Net:
        if (e.domain == L"NETWORK") filteredIndices.push_back(i);
        break;
      case LogFilter::Kernel:
        if (e.domain == L"KERNEL") filteredIndices.push_back(i);
        break;
    }
  }

  const int totalFiltered = static_cast<int>(filteredIndices.size());
  const int maxScroll = std::max(0, totalFiltered - visibleLines);
  state_.logScroll = std::clamp(state_.logScroll, 0, maxScroll);

  int y = listRect.top;
  if (filteredIndices.empty()) {
    DrawTextRect(dc, listRect, state_.logs.empty() ? L"Waiting for live telemetry logs..." : L"No logs match the current filter.",
      ColorRole::Dim, bodyFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  const int totalW = listRect.right - listRect.left;
  const int timeW = static_cast<int>(totalW * 0.12);
  const int catW = static_cast<int>(totalW * 0.18);
  const int subcatW = static_cast<int>(totalW * 0.12);

  for (int i = 0; i < visibleLines && state_.logScroll + i < totalFiltered; ++i) {
    const auto& entry = state_.logs[filteredIndices[state_.logScroll + i]];

    RECT rowRect = { listRect.left, y, listRect.right, y + logLineHeight_ + 2 };
    if ((state_.logScroll + i) % 2 == 0) {
      FillSolid(dc, rowRect, RGB(6, 8, 6));
    }

    int x = listRect.left + 6;
    std::wstring timeLabel = entry.time;
    if (entry.sessionId.size() > 4) {
      timeLabel += L" " + entry.sessionId.substr(entry.sessionId.size() - 4);
    }
    DrawTextRect(dc, { x, y, x + timeW - 4, y + logLineHeight_ }, timeLabel, ColorRole::Dim, logFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    x += timeW;
    DrawTextRect(dc, { x, y, x + catW - 4, y + logLineHeight_ }, L"[" + entry.domain + L"]", entry.color, logFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    x += catW;
    DrawTextRect(dc, { x, y, x + subcatW - 4, y + logLineHeight_ }, entry.severity, entry.color, logFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    x += subcatW;
    DrawTextRect(dc, { x, y, listRect.right - 4, y + logLineHeight_ }, entry.message, entry.color, logFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

    y += logLineHeight_ + 2;
  }
}

void MonixApp::DrawTasksView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  RECT tablePanel = outer;
  tablePanel.right -= 380;
  RECT detailPanel = outer;
  detailPanel.left = tablePanel.right + 14;

  DrawPanel(dc, tablePanel, L"TASK SURFACE", ColorRole::Primary, smallFont_.get());
  DrawPanel(dc, detailPanel, L"PROCESS INSPECTOR", ColorRole::Scram, smallFont_.get());

  RECT tableRect = ShrinkRect(tablePanel, 14);
  tableRect.top += 34;
  const int headerHeight = 48;
  RECT header = tableRect;
  header.bottom = header.top + headerHeight;
  FillSolid(dc, header, RGB(8, 24, 10));
  DrawRectOutline(dc, header, ResolveColor(ColorRole::Accent));

  const int width = tableRect.right - tableRect.left;
  const int nameEnd = tableRect.left + static_cast<int>(width * 0.28);
  const int pidEnd = tableRect.left + static_cast<int>(width * 0.39);
  const int cpuEnd = tableRect.left + static_cast<int>(width * 0.51);
  const int ramEnd = tableRect.left + static_cast<int>(width * 0.67);
  const int gpuEnd = tableRect.left + static_cast<int>(width * 0.79);
  const int priEnd = tableRect.left + static_cast<int>(width * 0.89);

  DrawTextRect(dc, RECT { tableRect.left + 10, header.top, nameEnd - 6, header.bottom }, L"PROCESS", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { nameEnd + 6, header.top, pidEnd - 6, header.bottom }, L"PID", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { pidEnd + 6, header.top, cpuEnd - 6, header.bottom }, L"CPU", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { cpuEnd + 6, header.top, ramEnd - 6, header.bottom }, L"RAM", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { ramEnd + 6, header.top, gpuEnd - 6, header.bottom }, L"GPU", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { gpuEnd + 6, header.top, priEnd - 6, header.bottom }, L"PRI", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { priEnd + 6, header.top, tableRect.right - 6, header.bottom }, L"STATE", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  const int rowHeight = std::max(38, bodyLineHeight_ + 14);
  const int visibleRows = VisibleTaskRows(tableRect);
  const int maxScroll = std::max(0, static_cast<int>(state_.snapshot.processes.size()) - visibleRows);
  state_.taskScroll = std::clamp(state_.taskScroll, 0, maxScroll);
  const std::size_t subGigabyteProcesses = std::count_if(state_.snapshot.processes.begin(), state_.snapshot.processes.end(), [](const ProcessInfo& process) {
    return process.ramBytes < 1024ull * 1024ull * 1024ull;
  });
  const auto cpuColor = [&](const ProcessInfo& process) {
    if (process.cpuPct >= 35.0 || (state_.snapshot.cpuPct >= 70.0 && process.cpuPct >= 12.0)) return ColorRole::Warning;
    if (process.cpuPct >= 8.0) return ColorRole::Primary;
    return ColorRole::White;
  };
  const auto ramColor = [&](const ProcessInfo& process) {
    if (state_.snapshot.ramTotalBytes == 0) return ColorRole::White;
    const double share = (static_cast<double>(process.ramBytes) / static_cast<double>(state_.snapshot.ramTotalBytes)) * 100.0;
    if (process.ramBytes < 1024ull * 1024ull * 1024ull && subGigabyteProcesses >= 8) return ColorRole::White;
    if (share >= 10.0 || process.ramBytes >= 3ull * 1024ull * 1024ull * 1024ull) return ColorRole::Error;
    if (share >= 4.0 || process.ramBytes >= 1024ull * 1024ull * 1024ull) return ColorRole::Warning;
    return ColorRole::White;
  };
  const auto gpuColor = [&](const ProcessInfo& process) {
    if (process.gpuPct >= 50.0) return ColorRole::Error;
    if (process.gpuPct >= 15.0 || (state_.snapshot.gpuPct >= 70.0 && process.gpuPct >= 8.0)) return ColorRole::Warning;
    return ColorRole::White;
  };

  for (int row = 0; row < visibleRows; ++row) {
    const int processIndex = state_.taskScroll + row;
    if (processIndex >= static_cast<int>(state_.snapshot.processes.size())) {
      break;
    }

    RECT rowRect {
      tableRect.left,
      header.bottom + row * rowHeight,
      tableRect.right,
      header.bottom + (row + 1) * rowHeight
    };

    const auto& process = state_.snapshot.processes[processIndex];
    const bool selected = processIndex == state_.selectedTaskIndex;
    const bool tracked = process.pid == state_.trackedPid;

    FillSolid(dc, rowRect, selected ? RGB(18, 44, 20) : RGB(4, 12, 5));
    DrawRectOutline(dc, rowRect, tracked ? ResolveColor(ColorRole::Scram) : ResolveColor(ColorRole::Accent));

    DrawTextLine(dc, rowRect.left + 10, rowRect.top + 7, nameEnd - rowRect.left - 16, process.name, ColorRole::White, bodyFont_.get());
    DrawTextLine(dc, nameEnd + 6, rowRect.top + 7, pidEnd - nameEnd - 12, std::to_wstring(process.pid), ColorRole::Dim, bodyFont_.get());
    DrawTextLine(dc, pidEnd + 6, rowRect.top + 7, cpuEnd - pidEnd - 12, FormatPercent(process.cpuPct), cpuColor(process), bodyFont_.get());
    DrawTextLine(dc, cpuEnd + 6, rowRect.top + 7, ramEnd - cpuEnd - 12, FormatBytes(process.ramBytes), ramColor(process), bodyFont_.get());
    DrawTextLine(dc, ramEnd + 6, rowRect.top + 7, gpuEnd - ramEnd - 12, FormatPercent(process.gpuPct), gpuColor(process), bodyFont_.get());
    DrawTextLine(dc, gpuEnd + 6, rowRect.top + 7, priEnd - gpuEnd - 12, process.priority, ColorRole::Dim, bodyFont_.get());
    DrawTextLine(dc, priEnd + 6, rowRect.top + 7, tableRect.right - priEnd - 12, process.status, tracked ? ColorRole::Scram : ColorRole::Primary, bodyFont_.get());
  }

  const int selectedIndex = std::clamp(state_.selectedTaskIndex, 0, std::max(0, static_cast<int>(state_.snapshot.processes.size()) - 1));
  const ProcessInfo* selected = state_.snapshot.processes.empty() ? nullptr : &state_.snapshot.processes[selectedIndex];

  if (selected) {
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 56, detailPanel.right - detailPanel.left - 36, L"Name: " + selected->name, ColorRole::White, bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 96, detailPanel.right - detailPanel.left - 36, L"PID: " + std::to_wstring(selected->pid), ColorRole::Dim, bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 136, detailPanel.right - detailPanel.left - 36, L"PPID: " + std::to_wstring(selected->parentPid) + L" | Session: " + std::to_wstring(selected->sessionId), ColorRole::Dim, bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 176, detailPanel.right - detailPanel.left - 36, L"CPU: " + FormatPercent(selected->cpuPct), cpuColor(*selected), bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 216, detailPanel.right - detailPanel.left - 36, L"RAM: " + FormatBytes(selected->ramBytes), ramColor(*selected), bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 256, detailPanel.right - detailPanel.left - 36, L"GPU: " + FormatPercent(selected->gpuPct), gpuColor(*selected), bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 296, detailPanel.right - detailPanel.left - 36, L"Priority: " + selected->priority + L" | " + selected->status, ColorRole::White, bodyFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 336, detailPanel.right - detailPanel.left - 36, L"GUID: " + selected->processGuid, ColorRole::Dim, smallFont_.get());
    DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 370, detailPanel.right - detailPanel.left - 36, L"Tracked in S.C.R.A.M: " + std::wstring(selected->pid == state_.trackedPid ? L"YES" : L"NO"), selected->pid == state_.trackedPid ? ColorRole::Scram : ColorRole::Dim, bodyFont_.get());
  }

  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 414, detailPanel.right - detailPanel.left - 36, L"Process count: " + std::to_wstring(state_.snapshot.processCount), ColorRole::Dim, smallFont_.get());
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 446, detailPanel.right - detailPanel.left - 36, L"Thread count: " + std::to_wstring(state_.snapshot.threadCount), ColorRole::Dim, smallFont_.get());
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 478, detailPanel.right - detailPanel.left - 36, L"Handles: " + std::to_wstring(state_.snapshot.handleCount), ColorRole::Dim, smallFont_.get());
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 510, detailPanel.right - detailPanel.left - 36, L"Queue length: " + std::to_wstring(state_.snapshot.processorQueueLength), ColorRole::Kernel, smallFont_.get());

  DrawTextRect(dc, RECT { detailPanel.left + 16, detailPanel.bottom - 170, detailPanel.right - 16, detailPanel.bottom - 20 }, L"Custom menu:\nInspect\nTrack in S.C.R.A.M\nCopy PID\nPriority HIGH\nPriority LOW\nRefresh snapshot\n\nRight click any row to open it.", ColorRole::Dim, smallFont_.get(), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
}

void MonixApp::DrawHardwareView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);

  const double ramPct = state_.snapshot.ramTotalBytes == 0 ? 0.0 :
    (static_cast<double>(state_.snapshot.ramUsedBytes) / static_cast<double>(state_.snapshot.ramTotalBytes)) * 100.0;

  RECT topPanel = outer;
  topPanel.bottom = outer.top + 240;
  RECT midLeft = outer;
  midLeft.top = topPanel.bottom + 14;
  midLeft.right = outer.left + (outer.right - outer.left) / 2 - 7;
  midLeft.bottom = midLeft.top + 200;
  RECT midRight = outer;
  midRight.top = topPanel.bottom + 14;
  midRight.left = midLeft.right + 14;
  midRight.bottom = midRight.top + 200;
  RECT botLeft = outer;
  botLeft.top = midLeft.bottom + 14;
  botLeft.right = midLeft.right;
  botLeft.bottom = botLeft.top + 160;
  RECT botRight = outer;
  botRight.top = midRight.bottom + 14;
  botRight.left = midLeft.right + 14;
  botRight.bottom = botRight.top + 160;

  DrawPanel(dc, topPanel, L"HARDWARE OVERVIEW", ColorRole::Primary, smallFont_.get());
  DrawPanel(dc, midLeft, L"CPU & THERMAL", ColorRole::Warning, smallFont_.get());
  DrawPanel(dc, midRight, L"MEMORY", ColorRole::Thermal, smallFont_.get());
  DrawPanel(dc, botLeft, L"STORAGE & DISK", ColorRole::Storage, smallFont_.get());
  DrawPanel(dc, botRight, L"NETWORK ADAPTER", ColorRole::Network, smallFont_.get());

  RECT cpuBar { topPanel.left + 22, topPanel.top + 64, topPanel.right - 320, topPanel.top + 88 };
  RECT ramBar { topPanel.left + 22, topPanel.top + 112, topPanel.right - 320, topPanel.top + 136 };
  RECT gpuBar { topPanel.left + 22, topPanel.top + 160, topPanel.right - 320, topPanel.top + 184 };
  DrawTextLine(dc, cpuBar.left, cpuBar.top - 24, 320, L"CPU  " + BuildBar(state_.snapshot.cpuPct) + L"  " + FormatPercent(state_.snapshot.cpuPct), ColorRole::Primary, bodyFont_.get());
  DrawProgressBar(dc, cpuBar, state_.snapshot.cpuPct, state_.snapshot.cpuPct >= 85.0 ? ColorRole::Warning : ColorRole::Success);
  DrawTextLine(dc, ramBar.left, ramBar.top - 24, 320, L"RAM  " + BuildBar(ramPct) + L"  " + FormatPercent(ramPct), ColorRole::Primary, bodyFont_.get());
  DrawProgressBar(dc, ramBar, ramPct, ramPct >= 82.0 ? ColorRole::Warning : ColorRole::Success);
  DrawTextLine(dc, gpuBar.left, gpuBar.top - 24, 320, L"GPU  " + BuildBar(state_.snapshot.gpuPct) + L"  " + FormatPercent(state_.snapshot.gpuPct), ColorRole::Primary, bodyFont_.get());
  DrawProgressBar(dc, gpuBar, state_.snapshot.gpuPct, state_.snapshot.gpuPct >= 85.0 ? ColorRole::Warning : ColorRole::Success);

  RECT cpuSpark { topPanel.right - 290, topPanel.top + 56, topPanel.right - 24, topPanel.top + 102 };
  RECT ramSpark { topPanel.right - 290, topPanel.top + 108, topPanel.right - 24, topPanel.top + 154 };
  RECT gpuSpark { topPanel.right - 290, topPanel.top + 160, topPanel.right - 24, topPanel.top + 206 };
  DrawSparkline(dc, cpuSpark, state_.cpuHistory, 100.0, ColorRole::Success);
  DrawSparkline(dc, ramSpark, state_.ramHistory, 100.0, ColorRole::Thermal);
  DrawSparkline(dc, gpuSpark, state_.gpuHistory, 100.0, ColorRole::Engine);

  DrawTextLine(dc, topPanel.left + 22, topPanel.top + 210, topPanel.right - topPanel.left - 44,
    L"Host: " + state_.snapshot.host + L"  |  Uptime: " + FormatDuration(state_.snapshot.uptimeSeconds),
    ColorRole::Dim, smallFont_.get());

  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 56, midLeft.right - midLeft.left - 40, L"CPU temp: " + FormatTemperature(state_.snapshot.cpuTempC, state_.snapshot.cpuTempEstimated), ColorRole::Warning, bodyFont_.get());
  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 96, midLeft.right - midLeft.left - 40, L"GPU temp: " + FormatTemperature(state_.snapshot.gpuTempC, state_.snapshot.gpuTempEstimated), ColorRole::Warning, bodyFont_.get());
  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 136, midLeft.right - midLeft.left - 40, L"Storage temp: " + FormatTemperature(state_.snapshot.storageTempC, state_.snapshot.storageTempEstimated), ColorRole::Warning, bodyFont_.get());
  DrawTextLine(dc, midLeft.left + 20, midLeft.top + 176, midLeft.right - midLeft.left - 40, L"Queue: " + std::to_wstring(state_.snapshot.processorQueueLength) + L"  |  Ctx sw/s: " + std::to_wstring(state_.snapshot.contextSwitchesPerSec), ColorRole::Kernel, bodyFont_.get());

  DrawTextLine(dc, midRight.left + 20, midRight.top + 56, midRight.right - midRight.left - 40, L"Total: " + FormatBytes(state_.snapshot.ramTotalBytes), ColorRole::Primary, bodyFont_.get());
  DrawTextLine(dc, midRight.left + 20, midRight.top + 96, midRight.right - midRight.left - 40, L"Used: " + FormatBytes(state_.snapshot.ramUsedBytes), ColorRole::Warning, bodyFont_.get());
  DrawTextLine(dc, midRight.left + 20, midRight.top + 136, midRight.right - midRight.left - 40, L"Pagefile: " + FormatBytes(state_.snapshot.pageFileUsedBytes) + L" / " + FormatBytes(state_.snapshot.pageFileTotalBytes), ColorRole::Dim, bodyFont_.get());
  DrawTextLine(dc, midRight.left + 20, midRight.top + 176, midRight.right - midRight.left - 40, L"IRPs/s: " + std::to_wstring(state_.snapshot.interruptsPerSec) + L"  |  Syscalls: " + std::to_wstring(state_.snapshot.systemCallsPerSec), ColorRole::Kernel, bodyFont_.get());

  DrawTextLine(dc, botLeft.left + 20, botLeft.top + 56, botLeft.right - botLeft.left - 40, L"Disk read: " + FormatRate(state_.snapshot.diskReadBytesPerSec), ColorRole::Primary, bodyFont_.get());
  DrawTextLine(dc, botLeft.left + 20, botLeft.top + 96, botLeft.right - botLeft.left - 40, L"Disk write: " + FormatRate(state_.snapshot.diskWriteBytesPerSec), ColorRole::Primary, bodyFont_.get());
  DrawTextLine(dc, botLeft.left + 20, botLeft.top + 136, botLeft.right - botLeft.left - 40, L"Processes: " + std::to_wstring(state_.snapshot.processCount) + L"  |  Threads: " + std::to_wstring(state_.snapshot.threadCount) + L"  |  Handles: " + std::to_wstring(state_.snapshot.handleCount), ColorRole::Dim, bodyFont_.get());

  DrawTextLine(dc, botRight.left + 20, botRight.top + 56, botRight.right - botRight.left - 40, L"Upload: " + FormatRate(state_.snapshot.netUpBytesPerSec), ColorRole::Network, bodyFont_.get());
  DrawTextLine(dc, botRight.left + 20, botRight.top + 96, botRight.right - botRight.left - 40, L"Download: " + FormatRate(state_.snapshot.netDownBytesPerSec), ColorRole::Network, bodyFont_.get());
  DrawTextLine(dc, botRight.left + 20, botRight.top + 136, botRight.right - botRight.left - 40, L"Inbound: " + std::to_wstring(state_.snapshot.inboundConnections) + L"  |  Outbound: " + std::to_wstring(state_.snapshot.outboundConnections), ColorRole::Dim, bodyFont_.get());
}

void MonixApp::DrawNetworkView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  RECT statsPanel = outer;
  statsPanel.bottom = outer.top + 188;
  RECT flowPanel = outer;
  flowPanel.top = statsPanel.bottom + 14;
  flowPanel.right = outer.left + static_cast<int>((outer.right - outer.left) * 0.62);
  flowPanel.bottom = outer.bottom - 226;
  RECT detailPanel = outer;
  detailPanel.left = flowPanel.right + 14;
  detailPanel.top = statsPanel.bottom + 14;
  detailPanel.bottom = flowPanel.bottom;
  RECT logPanel = outer;
  logPanel.top = flowPanel.bottom + 14;

  DrawPanel(dc, statsPanel, L"NETWORK OVERVIEW", ColorRole::Engine, smallFont_.get());
  DrawPanel(dc, flowPanel, L"LIVE SOCKET FLOWS", ColorRole::Network, smallFont_.get());
  DrawPanel(dc, detailPanel, L"NETWORK DIAGNOSTICS", ColorRole::Primary, smallFont_.get());
  DrawPanel(dc, logPanel, L"NETWORK EVENT LOG", ColorRole::Engine, smallFont_.get());

  const auto historyMax = [](const std::vector<double>& values, double minimum) {
    double peak = minimum;
    for (double value : values) {
      peak = std::max(peak, value);
    }
    return peak;
  };

  const auto topFlow = std::max_element(state_.snapshot.flows.begin(), state_.snapshot.flows.end(), [](const NetworkFlow& left, const NetworkFlow& right) {
    return left.activeConnections < right.activeConnections;
  });
  const std::wstring busiestFlow = topFlow == state_.snapshot.flows.end() ?
    L"Awaiting socket inventory" :
    topFlow->name + L" (" + std::to_wstring(topFlow->activeConnections) + L")";

  const int columnWidth = (statsPanel.right - statsPanel.left - 42) / 3;
  const int cardHeight = 52;
  const std::array<std::wstring, 6> captions {
    L"Download: " + FormatRate(state_.snapshot.netDownBytesPerSec),
    L"Upload: " + FormatRate(state_.snapshot.netUpBytesPerSec),
    L"Latency: " + std::to_wstring(state_.snapshot.latencyMs) + L"ms",
    L"Established: " + std::to_wstring(state_.snapshot.outboundConnections),
    L"Listening: " + std::to_wstring(state_.snapshot.inboundConnections),
    L"Top talker: " + busiestFlow
  };

  for (int i = 0; i < static_cast<int>(captions.size()); ++i) {
    const int row = i / 3;
    const int column = i % 3;
    RECT card {
      statsPanel.left + 16 + column * columnWidth,
      statsPanel.top + 54 + row * (cardHeight + 12),
      statsPanel.left + 16 + (column + 1) * columnWidth - 12,
      statsPanel.top + 54 + row * (cardHeight + 12) + cardHeight
    };
    FillSolid(dc, card, RGB(8, 18, 10));
    DrawRectOutline(dc, card, ResolveColor(ColorRole::Accent));
    DrawTextRect(dc, card, captions[i], i < 2 ? ColorRole::Network : (i == 2 ? ColorRole::Primary : ColorRole::White), smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  RECT downSpark { detailPanel.left + 18, detailPanel.top + 56, detailPanel.right - 18, detailPanel.top + 106 };
  RECT upSpark { detailPanel.left + 18, detailPanel.top + 126, detailPanel.right - 18, detailPanel.top + 176 };
  RECT latencySpark { detailPanel.left + 18, detailPanel.top + 196, detailPanel.right - 18, detailPanel.top + 246 };
  DrawSparkline(dc, downSpark, state_.netHistory, historyMax(state_.netHistory, 8.0), ColorRole::Network);
  DrawSparkline(dc, upSpark, state_.netUploadHistory, historyMax(state_.netUploadHistory, 4.0), ColorRole::Primary);
  DrawSparkline(dc, latencySpark, state_.latencyHistory, historyMax(state_.latencyHistory, 25.0), ColorRole::UserInput);

  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 252, detailPanel.right - detailPanel.left - 36, L"DNS estimator: " + std::to_wstring(state_.snapshot.dnsPseudo) + L"/s", ColorRole::Engine, smallFont_.get());
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 282, detailPanel.right - detailPanel.left - 36, L"Tracked process count: " + std::to_wstring(static_cast<int>(state_.snapshot.flows.size())), ColorRole::Dim, smallFont_.get());
  DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 312, detailPanel.right - detailPanel.left - 36, L"Collector: TCP inventory grouped by owning process", ColorRole::Dim, smallFont_.get());
  DrawTextRect(dc, RECT { detailPanel.left + 18, detailPanel.top + 346, detailPanel.right - 18, detailPanel.bottom - 18 }, L"Charts are stable and non-reactive: traffic, latency and noise do not alter the CRT treatment. Only the data changes, never the display personality.", ColorRole::Dim, smallFont_.get(), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

  RECT header { flowPanel.left + 14, flowPanel.top + 46, flowPanel.right - 14, flowPanel.top + 88 };
  FillSolid(dc, header, RGB(8, 18, 10));
  DrawRectOutline(dc, header, ResolveColor(ColorRole::Accent));
  DrawTextRect(dc, RECT { header.left + 10, header.top, header.left + 220, header.bottom }, L"PROCESS", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { header.left + 230, header.top, header.left + 350, header.bottom }, L"EST.", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { header.left + 360, header.top, header.right - 110, header.bottom }, L"REMOTE", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  DrawTextRect(dc, RECT { header.right - 104, header.top, header.right - 12, header.bottom }, L"STATE", ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  int y = header.bottom;
  const int rowHeight = std::max(36, bodyLineHeight_ + 12);
  for (std::size_t i = 0; i < state_.snapshot.flows.size() && i < 10; ++i) {
    if (y + rowHeight > flowPanel.bottom) break;
    RECT row { header.left, y, header.right, y + rowHeight };
    FillSolid(dc, row, RGB(4, 12, 5));
    DrawRectOutline(dc, row, ResolveColor(ColorRole::Accent));
    const auto& flow = state_.snapshot.flows[i];
    DrawTextLine(dc, row.left + 10, row.top + 6, 200, flow.name, ColorRole::White, smallFont_.get());
    DrawTextLine(dc, row.left + 230, row.top + 6, 110, std::to_wstring(flow.activeConnections), ColorRole::Network, smallFont_.get());
    DrawTextLine(dc, row.left + 360, row.top + 6, row.right - row.left - 490, flow.remote, ColorRole::Dim, smallFont_.get());
    DrawTextLine(dc, row.right - 104, row.top + 6, 90, flow.state, flow.state == L"ACTIVE" ? ColorRole::Success : ColorRole::Dim, smallFont_.get());
    y += rowHeight;
  }

  std::vector<LogEntry> networkLogs;
  for (const auto& entry : state_.logs) {
    if (entry.domain == L"NETWORK") {
      networkLogs.push_back(entry);
    }
  }
  y = logPanel.top + 52;
  if (networkLogs.empty()) {
    DrawTextRect(dc, RECT { logPanel.left + 16, logPanel.top + 50, logPanel.right - 16, logPanel.bottom - 18 }, L"No network events emitted yet.", ColorRole::Dim, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    return;
  }

  for (std::size_t i = 0; i < networkLogs.size() && i < 6; ++i) {
    DrawTextLine(dc, logPanel.left + 16, y, logPanel.right - logPanel.left - 32, ComposeLogLine(networkLogs[i]), networkLogs[i].color, smallFont_.get());
    y += logLineHeight_ + 6;
  }
}

void MonixApp::DrawScramView(HDC dc, const RECT& clientRect) {
  const RECT outer = ContentRect(clientRect);
  DrawPanel(dc, outer, L"SCRAM", ColorRole::Scram, smallFont_.get());
  DrawTextRect(dc, outer, L"...", ColorRole::Dim, bodyFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void MonixApp::DrawSettingsView(HDC dc, const RECT& clientRect) {
  using namespace monix::renderer_vk;
  const RECT outer = ContentRect(clientRect);

  const int subTabW = 86;
  const int subTabH = 32;
  const int subTabGap = 4;
  const int subTabY = outer.top;

  RECT generalTab { outer.left, subTabY, outer.left + subTabW, subTabY + subTabH };
  RECT displayTab { outer.left + subTabW + subTabGap, subTabY, outer.left + subTabW * 2 + subTabGap, subTabY + subTabH };
  RECT loggingTab { outer.left + subTabW * 2 + subTabGap * 2, subTabY, outer.left + subTabW * 3 + subTabGap * 2, subTabY + subTabH };
  RECT shadersTab { outer.left + subTabW * 3 + subTabGap * 3, subTabY, outer.left + subTabW * 4 + subTabGap * 3, subTabY + subTabH };
  RECT systemTab { outer.left + subTabW * 4 + subTabGap * 4, subTabY, outer.left + subTabW * 5 + subTabGap * 4, subTabY + subTabH };
  RECT perfTab { outer.left + subTabW * 5 + subTabGap * 5, subTabY, outer.left + subTabW * 6 + subTabGap * 5, subTabY + subTabH };

  const bool showGeneral = state_.shaderBrowser.subTab == SettingsSubTab::General;
  const bool showDisplay = state_.shaderBrowser.subTab == SettingsSubTab::Display;
  const bool showLogging = state_.shaderBrowser.subTab == SettingsSubTab::Logging;
  const bool showShaders = state_.shaderBrowser.subTab == SettingsSubTab::Shaders;
  const bool showSystem = state_.shaderBrowser.subTab == SettingsSubTab::System;
  const bool showPerf = state_.shaderBrowser.subTab == SettingsSubTab::Performance;

  FillSolid(dc, generalTab, showGeneral ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, generalTab, ResolveColor(showGeneral ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, generalTab, L"GENERAL", showGeneral ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, displayTab, showDisplay ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, displayTab, ResolveColor(showDisplay ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, displayTab, L"DISPLAY", showDisplay ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, loggingTab, showLogging ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, loggingTab, ResolveColor(showLogging ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, loggingTab, L"LOGGING", showLogging ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, shadersTab, showShaders ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, shadersTab, ResolveColor(showShaders ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, shadersTab, L"SHADERS", showShaders ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, systemTab, showSystem ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, systemTab, ResolveColor(showSystem ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, systemTab, L"SYSTEM", showSystem ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  FillSolid(dc, perfTab, showPerf ? RGB(22, 86, 38) : RGB(8, 12, 8));
  DrawRectOutline(dc, perfTab, ResolveColor(showPerf ? ColorRole::Primary : ColorRole::Accent));
  DrawTextRect(dc, perfTab, L"PERF", showPerf ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

  const RECT contentArea { outer.left, outer.top + subTabH + 8, outer.right, outer.bottom };

  if (showGeneral) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    RECT rightPanel = contentArea;
    rightPanel.left = leftPanel.right + 14;

    DrawPanel(dc, leftPanel, L"RUNTIME", ColorRole::Security, smallFont_.get());
    DrawPanel(dc, rightPanel, L"UI PREFERENCES", ColorRole::Engine, smallFont_.get());

    const auto rows = BuildSettingActionRects(clientRect);
    state_.selectedSettingIndex = std::clamp(state_.selectedSettingIndex, 0, static_cast<int>(rows.size()) - 1);
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.selectedSettingIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_.get());
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(RuntimeSettings(), ColorRole::Security, ColorRole::White);
    drawRows(IntroAnimSettings(), ColorRole::Security, ColorRole::White);
    drawRows(GeneralExtendedSettings(), ColorRole::Engine, ColorRole::White);

  } else if (showDisplay) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(220, static_cast<int>((contentArea.bottom - contentArea.top) * 0.30));
    RECT rightMid = contentArea;
    rightMid.left = leftPanel.right + 14;
    rightMid.top = rightTop.bottom + 14;
    rightMid.bottom = rightTop.bottom + 14 + std::max(120, static_cast<int>((contentArea.bottom - contentArea.top) * 0.16));
    RECT rightMid2 = contentArea;
    rightMid2.left = leftPanel.right + 14;
    rightMid2.top = rightMid.bottom + 14;
    rightMid2.bottom = rightMid.bottom + 14 + std::max(100, static_cast<int>((contentArea.bottom - contentArea.top) * 0.14));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightMid2.bottom + 14;
    RECT rightBottom2 = contentArea;
    rightBottom2.left = leftPanel.right + 14;
    rightBottom2.top = rightBottom.bottom + 14;

    DrawPanel(dc, rightBottom2, L"BORDER OVERLAY", ColorRole::Engine, smallFont_.get());

    const auto rows = BuildSettingActionRects(clientRect);
    state_.selectedSettingIndex = std::clamp(state_.selectedSettingIndex, 0, static_cast<int>(rows.size()) - 1);
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.selectedSettingIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_.get());
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(BorderSettings(), ColorRole::Engine, ColorRole::Engine);

  } else if (showLogging) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.52);
    RECT rightPanel = contentArea;
    rightPanel.left = leftPanel.right + 14;

    DrawPanel(dc, leftPanel, L"LOGGING CONFIGURATION", ColorRole::Engine, smallFont_.get());
    DrawPanel(dc, rightPanel, L"LOG OUTPUT INFO", ColorRole::Accent, smallFont_.get());

    const auto rows = BuildSettingActionRects(clientRect);
    state_.selectedSettingIndex = std::clamp(state_.selectedSettingIndex, 0, static_cast<int>(rows.size()) - 1);
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.selectedSettingIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_.get());
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(LoggingSettings(), ColorRole::Engine, ColorRole::Engine);

    const std::wstring logInfo =
      L"Log buffer: " + std::to_wstring(config_.logBufferSize) + L" entries\n"
      L"Visible lines: " + std::to_wstring(config_.logVisibleLines) + L"\n"
      L"Flush interval: " + std::to_wstring(config_.logFlushIntervalMs) + L" ms\n"
      L"Retention: " + std::to_wstring(config_.logRetentionDays) + L" days\n"
      L"Deduplication: " + std::wstring(config_.logDeduplicate ? L"ON" : L"OFF") + L"\n"
      L"Format: " + std::wstring(config_.logJsonEnabled ? L"JSON Lines" : L"Plain text") + L"\n"
      L"Live logs: " + std::wstring(config_.pauseLiveLogs ? L"PAUSED" : L"STREAMING") + L"\n\n"
      L"Log files rotate daily and are compressed\n"
      L"after the configured retention period.";
    DrawTextRect(dc, RECT { rightPanel.left + 18, rightPanel.top + 56, rightPanel.right - 18, rightPanel.bottom - 18 }, logInfo, ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
  } else if (showShaders) {
    const int listW = static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    const int detailW = contentArea.right - contentArea.left - listW - 14;

    RECT filterBar = contentArea;
    filterBar.bottom = filterBar.top + 34;

    RECT listPanel = contentArea;
    listPanel.top = filterBar.bottom + 6;
    listPanel.right = contentArea.left + listW;

    RECT detailPanel = contentArea;
    detailPanel.left = listPanel.right + 14;
    detailPanel.top = filterBar.bottom + 6;

    const int filterBtnW = 80;
    const int filterBtnH = 26;
    const int filterGap = 8;
    int fx = contentArea.left + 14;

    auto drawFilterTab = [&](const wchar_t* label, ShaderLanguageFilter tabFilter) {
      RECT btn { fx, filterBar.top + 4, fx + filterBtnW, filterBar.top + 4 + filterBtnH };
      const bool active = state_.shaderBrowser.filter == tabFilter;
      FillSolid(dc, btn, active ? RGB(22, 86, 38) : RGB(8, 12, 8));
      DrawRectOutline(dc, btn, ResolveColor(active ? ColorRole::Primary : ColorRole::Accent));
      DrawTextRect(dc, btn, label, active ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      fx += filterBtnW + filterGap;
    };

    drawFilterTab(L"GLSL", ShaderLanguageFilter::GLSL);
    drawFilterTab(L"SLANG", ShaderLanguageFilter::Slang);
    drawFilterTab(L"PRESET", ShaderLanguageFilter::Preset);

    RECT favBtn { fx, filterBar.top + 4, fx + filterBtnW + 20, filterBar.top + 4 + filterBtnH };
    FillSolid(dc, favBtn, state_.shaderShowFavoritesOnly ? RGB(22, 86, 38) : RGB(8, 12, 8));
    DrawRectOutline(dc, favBtn, ResolveColor(state_.shaderShowFavoritesOnly ? ColorRole::Warning : ColorRole::Accent));
    DrawTextRect(dc, favBtn, L"\x2605 Favorites", state_.shaderShowFavoritesOnly ? ColorRole::Warning : ColorRole::Dim, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    fx += filterBtnW + 20 + filterGap;

    RECT searchBox { fx, filterBar.top + 4, contentArea.left + listW - 14, filterBar.top + 4 + filterBtnH };
    FillSolid(dc, searchBox, state_.shaderSearchFocused ? RGB(12, 18, 12) : RGB(7, 12, 8));
    DrawRectOutline(dc, searchBox, ResolveColor(state_.shaderSearchFocused ? ColorRole::Primary : ColorRole::Accent));
    std::wstring searchText = state_.shaderSearchText.empty() && !state_.shaderSearchFocused
      ? L"Search shaders..." : state_.shaderSearchText;
    if (state_.shaderSearchFocused) searchText += L"_";
    DrawTextRect(dc, searchBox, searchText, state_.shaderSearchText.empty() ? ColorRole::Dim : ColorRole::White, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_EDITCONTROL);

    DrawPanel(dc, listPanel, L"SHADER LIBRARY", ColorRole::Security, smallFont_.get());
    DrawPanel(dc, detailPanel, L"SHADER DETAIL", ColorRole::Primary, smallFont_.get());

    const int rowH = std::max(26, smallLineHeight_ + 4);
    const int innerPad = 18;

    const auto categories = shaderBrowserPanel_ ? shaderBrowserPanel_->categoryGroups() : std::vector<monix::renderer_vk::ShaderCategoryGroup>{};

    int y = listPanel.top + 48;
    int globalRow = 0;

    for (const auto& cat : categories) {
      if (y + rowH > listPanel.bottom - 50) break;

      RECT catRect { listPanel.left + innerPad, y, listPanel.right - innerPad, y + rowH };
      FillSolid(dc, catRect, RGB(10, 18, 12));
      const std::wstring catLabel = L"\x25BC " + Utf8ToWide(cat.name);
      DrawTextLine(dc, catRect.left + 6, catRect.top + 2, catRect.right - catRect.left - 12, catLabel, ColorRole::Dim, smallFont_.get());
      y += rowH + 2;

      for (const size_t libIdx : cat.entryIndices) {
        if (globalRow >= state_.shaderBrowser.scrollOffset) {
          if (y + rowH > listPanel.bottom - 50) break;

          const bool selected = globalRow == state_.shaderBrowser.selectedShaderIndex;
          RECT rowRect { listPanel.left + innerPad + 12, y, listPanel.right - innerPad, y + rowH };
          FillSolid(dc, rowRect, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
          DrawRectOutline(dc, rowRect, ResolveColor(selected ? ColorRole::Primary : ColorRole::Accent));

          const auto* entry = shaderLibrary_ ? shaderLibrary_->entry(libIdx) : nullptr;
          if (entry) {
            const std::wstring indicator = [s = entry->status]() -> std::wstring {
              switch (s) {
              case monix::renderer_vk::ShaderEntryStatus::Unknown:   return L"\x25CB";
              case monix::renderer_vk::ShaderEntryStatus::Pending:   return L"\x25CB";
              case monix::renderer_vk::ShaderEntryStatus::Compiling: return L"\x25CF";
              case monix::renderer_vk::ShaderEntryStatus::Compiled:  return L"\x25CF";
              case monix::renderer_vk::ShaderEntryStatus::Active:    return L"\x25D4";
              case monix::renderer_vk::ShaderEntryStatus::Error:     return L"\x25CF";
              }
              return L"\x25CB";
            }();
            const ColorRole indicatorColor = [s = entry->status]() -> ColorRole {
              switch (s) {
              case monix::renderer_vk::ShaderEntryStatus::Unknown:   return ColorRole::Dim;
              case monix::renderer_vk::ShaderEntryStatus::Pending:   return ColorRole::Dim;
              case monix::renderer_vk::ShaderEntryStatus::Compiling: return ColorRole::Warning;
              case monix::renderer_vk::ShaderEntryStatus::Compiled:  return ColorRole::Success;
              case monix::renderer_vk::ShaderEntryStatus::Active:    return ColorRole::Primary;
              case monix::renderer_vk::ShaderEntryStatus::Error:     return ColorRole::Error;
              }
              return ColorRole::Dim;
            }();

            DrawTextLine(dc, rowRect.left + 6, rowRect.top + 2, 20, indicator, indicatorColor, smallFont_.get());
            DrawTextLine(dc, rowRect.left + 28, rowRect.top + 2, rowRect.right - rowRect.left - 56, Utf8ToWide(entry->name), selected ? ColorRole::White : ColorRole::Dim, smallFont_.get());
            if (shaderBrowserPanel_ && shaderBrowserPanel_->isFavorite(libIdx)) {
              DrawTextLine(dc, rowRect.right - 24, rowRect.top + 2, 18, L"\x2605", ColorRole::Warning, smallFont_.get());
            }
          }
          y += rowH + 2;
        }
        ++globalRow;
      }
    }

    RECT actionBar { listPanel.left, listPanel.bottom - 42, listPanel.right, listPanel.bottom - 8 };
    FillSolid(dc, actionBar, RGB(10, 14, 10));

    const int actionBtnW = 76;
    const int actionBtnH = 28;
    const int actionGap = 6;
    int abx = actionBar.left + innerPad;

    auto drawActionBtn = [&](const wchar_t* label) {
      RECT btn { abx, actionBar.top + 2, abx + actionBtnW, actionBar.top + 2 + actionBtnH };
      FillSolid(dc, btn, RGB(22, 86, 38));
      DrawRectOutline(dc, btn, ResolveColor(ColorRole::Primary));
      DrawTextRect(dc, btn, label, ColorRole::White, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      abx += actionBtnW + actionGap;
    };

    drawActionBtn(L"IMPORT");
    drawActionBtn(L"DELETE");
    drawActionBtn(L"REFRESH");
    drawActionBtn(L"OPEN");

    const int totalFiltered = shaderBrowserPanel_ ? shaderBrowserPanel_->filteredShaderCount() : 0;
    const auto summary = shaderBrowserPanel_ ? shaderBrowserPanel_->statusSummary() : std::string();

    RECT summaryBar { listPanel.left + innerPad, actionBar.top - 20, listPanel.right - innerPad, actionBar.top - 2 };
    DrawTextLine(dc, summaryBar.left, summaryBar.top, summaryBar.right - summaryBar.left, Utf8ToWide(summary), ColorRole::Dim, smallFont_.get());

    if (shaderBrowserPanel_ && shaderBrowserPanel_->hasSelection()) {
      const auto detail = shaderBrowserPanel_->detailInfo();
      int dy = detailPanel.top + 56;
      const int labelW = 130;
      const int lineH = smallLineHeight_ + 2;

      auto drawDetailLine = [&](const wchar_t* label, const std::wstring& value) {
        if (dy + lineH > detailPanel.bottom - 180) return;
        DrawTextLine(dc, detailPanel.left + 18, dy, labelW, label, ColorRole::Dim, smallFont_.get());
        DrawTextLine(dc, detailPanel.left + 18 + labelW, dy, detailPanel.right - detailPanel.left - 36 - labelW, value, ColorRole::White, smallFont_.get());
        dy += lineH;
      };

      auto drawPerfLine = [&](const wchar_t* label, double ms) {
        if (dy + lineH > detailPanel.bottom - 180) return;
        DrawTextLine(dc, detailPanel.left + 18, dy, labelW, label, ColorRole::Dim, smallFont_.get());
        std::wstring val = (ms > 0.0) ? (std::to_wstring(ms).substr(0, 6) + L" ms") : L"N/A";
        DrawTextLine(dc, detailPanel.left + 18 + labelW, dy, detailPanel.right - detailPanel.left - 36 - labelW, val, ColorRole::White, smallFont_.get());
        dy += lineH;
      };

      drawDetailLine(L"Name:", Utf8ToWide(detail.name));
      drawDetailLine(L"Path:", Utf8ToWide(detail.path));
      drawDetailLine(L"Language:", Utf8ToWide(detail.language));
      drawDetailLine(L"Category:", Utf8ToWide(detail.category));
      drawDetailLine(L"Extension:", Utf8ToWide(detail.extension));
      drawDetailLine(L"Status:", Utf8ToWide(shaderEntryStatusName(detail.status)));
      drawDetailLine(L"Source:", detail.isActive ? L"Active" : L"Inactive");
      drawDetailLine(L"Hash:", std::to_wstring(detail.contentHash));
      drawDetailLine(L"File Size:", std::to_wstring(detail.fileSize) + L" bytes");
      drawDetailLine(L"Active:", detail.isActive ? L"Yes" : L"No");
      drawDetailLine(L"Compiled:", detail.hasCompiledModule ? L"Yes" : L"No");
      drawDetailLine(L"SPIR-V:", detail.spirvSizeBytes > 0 ? (std::to_wstring(detail.spirvSizeBytes) + L" bytes") : L"N/A");
      drawDetailLine(L"Cache:", detail.lastCacheStatus.empty() ? L"N/A" : Utf8ToWide(detail.lastCacheStatus));

      dy += 6;
      DrawTextLine(dc, detailPanel.left + 18, dy, detailPanel.right - detailPanel.left - 36, L"PERFORMANCE", ColorRole::Security, smallFont_.get());
      dy += lineH;
      drawPerfLine(L"Compile:", detail.compileDurationMs);
      drawPerfLine(L"Validation:", detail.validationDurationMs);
      drawPerfLine(L"Pipeline:", detail.pipelineDurationMs);
      drawPerfLine(L"Total:", detail.totalActivationMs);

      RECT loadBtn { detailPanel.left + 18, detailPanel.bottom - 52, detailPanel.left + 120, detailPanel.bottom - 22 };
      RECT reloadBtn { detailPanel.left + 130, detailPanel.bottom - 52, detailPanel.left + 240, detailPanel.bottom - 22 };
      FillSolid(dc, loadBtn, RGB(22, 86, 38));
      DrawRectOutline(dc, loadBtn, ResolveColor(ColorRole::Primary));
      DrawTextRect(dc, loadBtn, L"LOAD", ColorRole::White, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      FillSolid(dc, reloadBtn, RGB(22, 86, 38));
      DrawRectOutline(dc, reloadBtn, ResolveColor(ColorRole::Primary));
      DrawTextRect(dc, reloadBtn, L"RELOAD", ColorRole::White, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      if (!detail.firstError.empty()) {
        RECT errPanel { detailPanel.left, detailPanel.bottom - 280, detailPanel.right, detailPanel.bottom - 180 };
        DrawPanel(dc, errPanel, L"ERROR", ColorRole::Error, smallFont_.get());
        int ey = errPanel.top + 56;
        const int errPad = 18;
        DrawTextLine(dc, errPanel.left + errPad, ey, errPanel.right - errPanel.left - errPad * 2, Utf8ToWide(detail.firstError), ColorRole::Error, smallFont_.get());
      }
    } else {
      DrawTextLine(dc, detailPanel.left + 18, detailPanel.top + 56, detailPanel.right - detailPanel.left - 36, L"Select a shader to view details.", ColorRole::Dim, smallFont_.get());

      RECT loadBtn { detailPanel.left + 18, detailPanel.bottom - 52, detailPanel.left + 120, detailPanel.bottom - 22 };
      RECT reloadBtn { detailPanel.left + 130, detailPanel.bottom - 52, detailPanel.left + 240, detailPanel.bottom - 22 };
      FillSolid(dc, loadBtn, RGB(12, 20, 12));
      DrawRectOutline(dc, loadBtn, ResolveColor(ColorRole::Accent));
      DrawTextRect(dc, loadBtn, L"LOAD", ColorRole::Dim, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      FillSolid(dc, reloadBtn, RGB(12, 20, 12));
      DrawRectOutline(dc, reloadBtn, ResolveColor(ColorRole::Accent));
      DrawTextRect(dc, reloadBtn, L"RELOAD", ColorRole::Dim, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    RECT cachePanel { detailPanel.left, detailPanel.bottom - 170, detailPanel.right, detailPanel.bottom - 58 };
    {
      DrawPanel(dc, cachePanel, L"CACHE / DIAGNOSTICS", ColorRole::Engine, smallFont_.get());
      int cy = cachePanel.top + 56;
      const int cLabelW = 110;
      const int cLineH = smallLineHeight_ + 2;

      if (shaderBrowserPanel_) {
        auto cs = shaderBrowserPanel_->cacheStats();
        auto drawCacheLine = [&](const wchar_t* label, const std::wstring& value) {
          if (cy + cLineH > cachePanel.bottom - 8) return;
          DrawTextLine(dc, cachePanel.left + 18, cy, cLabelW, label, ColorRole::Dim, smallFont_.get());
          DrawTextLine(dc, cachePanel.left + 18 + cLabelW, cy, cachePanel.right - cachePanel.left - 36 - cLabelW, value, ColorRole::White, smallFont_.get());
          cy += cLineH;
        };

        drawCacheLine(L"Hits:", std::to_wstring(cs.hits));
        drawCacheLine(L"Misses:", std::to_wstring(cs.misses));
        double rate = (cs.hits + cs.misses) > 0 ? (100.0 * cs.hits / (cs.hits + cs.misses)) : 0.0;
        drawCacheLine(L"Hit Rate:", (std::to_wstring(rate).substr(0, 5) + L"%"));
        drawCacheLine(L"Corrupted:", std::to_wstring(cs.corrupted));
        drawCacheLine(L"Entries:", std::to_wstring(cs.totalEntries));
        double mb = cs.totalSizeBytes / (1024.0 * 1024.0);
        drawCacheLine(L"Size:", (std::to_wstring(mb).substr(0, 6) + L" MB"));
        double limMB = cs.memoryLimitBytes / (1024.0 * 1024.0);
        drawCacheLine(L"Limit:", (std::to_wstring(limMB).substr(0, 6) + L" MB"));

        cy += 4;
        auto diag = shaderBrowserPanel_->rendererDiagnostics();
        drawCacheLine(L"GPU:", diag.gpuVendor.empty() ? L"N/A" : Utf8ToWide(diag.gpuVendor));
        drawCacheLine(L"Renderer:", diag.gpuRenderer.empty() ? L"N/A" : Utf8ToWide(diag.gpuRenderer));
        drawCacheLine(L"API:", diag.apiVersion.empty() ? L"N/A" : Utf8ToWide(diag.apiVersion));
        drawCacheLine(L"Validation:", diag.validationEnabled ? L"Enabled" : L"Disabled");
      }
    }

    auto hre = shaderBrowserPanel_ ? shaderBrowserPanel_->lastHotReloadEvent() : monix::renderer_vk::ShaderBrowserPanel::HotReloadEvent{};
    if (hre.active) {
      RECT hotRect { contentArea.left + 14, contentArea.bottom - 24, contentArea.left + 250, contentArea.bottom - 4 };
      const bool ok = hre.status == "Active" || hre.status == "Compiled";
      DrawTextLine(dc, hotRect.left, hotRect.top, hotRect.right - hotRect.left,
        ok ? L"RELOADED" : L"RELOAD FAILED", ok ? ColorRole::Success : ColorRole::Error, smallFont_.get());
    }

    const auto& errHist = shaderBrowserPanel_ ? shaderBrowserPanel_->errorHistory() : std::deque<monix::renderer_vk::ErrorHistoryEntry>{};
    if (!errHist.empty()) {
      RECT histPanel { listPanel.left, listPanel.bottom - 180, listPanel.right, listPanel.bottom - 46 };
      DrawPanel(dc, histPanel, L"ERROR HISTORY", ColorRole::Error, smallFont_.get());
      int hy = histPanel.top + 56;
      const int hLineH = smallLineHeight_ + 2;
      int showCount = std::min(static_cast<int>(errHist.size()), 4);
      for (int i = showCount - 1; i >= 0; --i) {
        if (hy + hLineH > histPanel.bottom - 8) break;
        const auto& e = errHist[i];
        std::wstring line = Utf8ToWide(e.shader) + L" [" + Utf8ToWide(e.stage) + L"] " + Utf8ToWide(e.error);
        DrawTextLine(dc, histPanel.left + 18, hy, histPanel.right - histPanel.left - 36, line, ColorRole::Error, smallFont_.get());
        hy += hLineH;
      }
    }
  } else if (showSystem) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);
    RECT rightTop = contentArea;
    rightTop.left = leftPanel.right + 14;
    rightTop.bottom = contentArea.top + std::max(320, static_cast<int>((contentArea.bottom - contentArea.top) * 0.45));
    RECT rightBottom = contentArea;
    rightBottom.left = leftPanel.right + 14;
    rightBottom.top = rightTop.bottom + 14;

    DrawPanel(dc, leftPanel, L"THEME & APPEARANCE", ColorRole::Primary, smallFont_.get());
    DrawPanel(dc, rightTop, L"WINDOW & SYSTEM", ColorRole::Security, smallFont_.get());
    DrawPanel(dc, rightBottom, L"HOTKEYS", ColorRole::Warning, smallFont_.get());

    const auto rows = BuildSettingActionRects(clientRect);
    state_.selectedSettingIndex = std::clamp(state_.selectedSettingIndex, 0, static_cast<int>(rows.size()) - 1);
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.selectedSettingIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_.get());
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(ThemeSettings(), ColorRole::Primary, ColorRole::White);
    drawRows(WindowStartupSettings(), ColorRole::Security, ColorRole::White);
    drawRows(SystemExtendedSettings(), ColorRole::Security, ColorRole::White);
    drawRows(HotkeySettings(), ColorRole::Warning, ColorRole::Warning);
  } else if (showPerf) {
    RECT leftPanel = contentArea;
    leftPanel.right = contentArea.left + static_cast<int>((contentArea.right - contentArea.left) * 0.50);

    DrawPanel(dc, leftPanel, L"PERFORMANCE", ColorRole::Thermal, smallFont_.get());

    const auto rows = BuildSettingActionRects(clientRect);
    state_.selectedSettingIndex = std::clamp(state_.selectedSettingIndex, 0, static_cast<int>(rows.size()) - 1);
    std::size_t cursor = 0;
    const auto drawRows = [&](const auto& settings, ColorRole accent, ColorRole valueColor) {
      for (std::size_t local = 0; local < settings.size() && cursor < rows.size(); ++local, ++cursor) {
        const auto& action = rows[cursor];
        const bool selected = static_cast<int>(cursor) == state_.selectedSettingIndex;
        FillSolid(dc, action.row, selected ? RGB(16, 30, 18) : RGB(7, 12, 8));
        DrawRectOutline(dc, action.row, ResolveColor(selected ? accent : ColorRole::Accent));
        FillSolid(dc, action.decrease, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        FillSolid(dc, action.increase, selected ? RGB(18, 34, 18) : RGB(10, 16, 10));
        DrawRectOutline(dc, action.decrease, ResolveColor(accent));
        DrawRectOutline(dc, action.increase, ResolveColor(accent));
        DrawTextRect(dc, action.decrease, L"-", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextRect(dc, action.increase, L"+", accent, smallFont_.get(), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DrawTextLine(dc, action.row.left + 10, action.row.top + 2, action.row.right - action.row.left - 270, SettingLabel(action.id), selected ? ColorRole::White : ColorRole::Dim, smallFont_.get());
        DrawTextRect(dc, action.value, SettingValueText(action.id), valueColor, smallFont_.get(), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      }
    };

    drawRows(PerformanceSettings(), ColorRole::Thermal, ColorRole::Thermal);
  }
}

void MonixApp::DrawTaskContextMenu(HDC dc) const {
  if (!state_.taskMenu.visible) {
    return;
  }

  static const std::array<std::wstring, 9> items {
    L"Inspect process",
    L"Track in S.C.R.A.M",
    L"Copy name and PID",
    L"Set priority HIGH",
    L"Set priority LOW",
    L"Terminate process",
    L"Export logs JSON",
    L"Refresh snapshot",
    L"Open in Explorer"
  };

  FillSolid(dc, state_.taskMenu.rect, RGB(8, 24, 10));
  DrawRectOutline(dc, state_.taskMenu.rect, ResolveColor(ColorRole::Scram));

  for (int i = 0; i < static_cast<int>(items.size()); ++i) {
    RECT itemRect {
      state_.taskMenu.rect.left,
      state_.taskMenu.rect.top + i * kTaskMenuItemHeight,
      state_.taskMenu.rect.right,
      state_.taskMenu.rect.top + (i + 1) * kTaskMenuItemHeight
    };
    if (i == state_.taskMenu.hoverIndex) {
      FillSolid(dc, itemRect, RGB(24, 52, 28));
    }
    DrawRectOutline(dc, itemRect, ResolveColor(ColorRole::Accent));
    DrawTextRect(dc, RECT { itemRect.left + 12, itemRect.top + 2, itemRect.right - 12, itemRect.bottom }, items[i], i == state_.taskMenu.hoverIndex ? ColorRole::White : ColorRole::Primary, smallFont_.get(), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32) {
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto pSetDpiAwarenessContext = reinterpret_cast<SetDpiAwarenessContextFn>(
      GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (pSetDpiAwarenessContext) {
      pSetDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
      HMODULE shcore = LoadLibraryW(L"shcore.dll");
      if (shcore) {
        using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
        auto pSetDpi = reinterpret_cast<SetProcessDpiAwarenessFn>(
          GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (pSetDpi) {
          pSetDpi(2);
        }
        FreeLibrary(shcore);
      } else {
        SetProcessDPIAware();
      }
    }
  }
  MonixApp app;
  bool testMode = false;
  bool testVulkanMode = false;
  bool captureMode = false;
  bool compareMode = false;
  std::wstring referenceDir;
  bool runtimeStateMode = false;
  std::wstring retroarchSnapshot;
  bool regressionTestMode = false;
  std::wstring regressionPresetsDir;
  int regressionFrames = 3;
  for (int i = 1; i < __argc; i++) {
    if (wcscmp(__wargv[i], L"--test") == 0 || wcscmp(__wargv[i], L"-test") == 0) testMode = true;
    if (wcscmp(__wargv[i], L"--test-vulkan") == 0 || wcscmp(__wargv[i], L"-test-vulkan") == 0) testVulkanMode = true;
    if (wcscmp(__wargv[i], L"--capture") == 0 || wcscmp(__wargv[i], L"-capture") == 0) captureMode = true;
    if (wcscmp(__wargv[i], L"--compare") == 0 || wcscmp(__wargv[i], L"-compare") == 0) compareMode = true;
    if ((wcscmp(__wargv[i], L"--reference") == 0 || wcscmp(__wargv[i], L"-reference") == 0) && i + 1 < __argc) {
      referenceDir = __wargv[++i];
    }
    if (wcscmp(__wargv[i], L"--runtime-state") == 0 || wcscmp(__wargv[i], L"-runtime-state") == 0) runtimeStateMode = true;
    if ((wcscmp(__wargv[i], L"--retroarch-snapshot") == 0 || wcscmp(__wargv[i], L"-retroarch-snapshot") == 0) && i + 1 < __argc) {
      retroarchSnapshot = __wargv[++i];
    }
    if (wcscmp(__wargv[i], L"--regression-test") == 0 || wcscmp(__wargv[i], L"-regression-test") == 0) regressionTestMode = true;
    if ((wcscmp(__wargv[i], L"--presets-dir") == 0 || wcscmp(__wargv[i], L"-presets-dir") == 0) && i + 1 < __argc) {
      regressionPresetsDir = __wargv[++i];
    }
    if ((wcscmp(__wargv[i], L"--frames") == 0 || wcscmp(__wargv[i], L"-frames") == 0) && i + 1 < __argc) {
      regressionFrames = _wtoi(__wargv[++i]);
    }
  }
  if (testMode) {
    app.SetTestMode();
    extern void runCompilationPipelineTests();
    runCompilationPipelineTests();
    extern void runMegadrivePresetTests();
    runMegadrivePresetTests();
    return 0;
  }
  if (testVulkanMode) {
    extern int runVulkanRuntimeTests(HINSTANCE instance);
    return runVulkanRuntimeTests(instance);
  }
  if (captureMode || compareMode) {
    app.SetParityMode(captureMode, compareMode, referenceDir);
  }
  if (runtimeStateMode) {
    app.SetRuntimeStateMode(true, retroarchSnapshot);
  }
  if (regressionTestMode) {
    app.SetRegressionTestMode(true, regressionPresetsDir, regressionFrames);
  }
  return app.Run(instance, showCommand);
}
