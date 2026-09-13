#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../settings/MonixConfigTypes.hpp"
#include "../core/TextUtils.hpp"
#include "../settings/SettingsRegistry.hpp"

namespace monix {

struct FontEntry {
  std::wstring displayName;
  std::wstring filePath;
  std::wstring faceName;
};

inline std::wstring ReadTtfFaceName(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return L"";
  uint32_t header[3];
  f.read(reinterpret_cast<char*>(header), 12);
  if (f.gcount() != 12) return L"";
  uint16_t numTables = _byteswap_ushort(reinterpret_cast<uint16_t*>(&header[1])[0]);
  for (uint16_t i = 0; i < numTables; ++i) {
    uint32_t tag[4];
    f.read(reinterpret_cast<char*>(tag), 16);
    if (f.gcount() != 16) return L"";
    uint32_t tableTag = _byteswap_ulong(tag[0]);
    uint32_t tableLen = _byteswap_ulong(tag[3]);
    uint32_t tableOff = _byteswap_ulong(tag[2]);
    if (tableTag == 0x6E616D65) {
      std::streampos saved = f.tellg();
      f.seekg(tableOff);
      std::vector<char> buf(tableLen);
      f.read(buf.data(), tableLen);
      f.seekg(saved);
      if (tableLen < 6) return L"";
      uint16_t count = _byteswap_ushort(reinterpret_cast<uint16_t*>(buf.data())[1]);
      uint16_t strOff = _byteswap_ushort(reinterpret_cast<uint16_t*>(buf.data())[2]);
      for (uint16_t j = 0; j < count; ++j) {
        uint16_t rec[6];
        size_t off = 6 + j * 12;
        if (off + 12 > buf.size()) break;
        for (int k = 0; k < 6; ++k)
          rec[k] = _byteswap_ushort(reinterpret_cast<uint16_t*>(buf.data() + off)[k]);
        if (rec[0] == 3 && rec[1] == 1 && rec[3] == 4) {
          uint16_t nameLen = rec[4];
          uint16_t nameOff = rec[5];
          size_t start = strOff + nameOff;
          if (start + nameLen <= buf.size()) {
            std::wstring result(nameLen / 2, L'\0');
            for (uint16_t c = 0; c < nameLen / 2; ++c)
              result[c] = static_cast<wchar_t>(_byteswap_ushort(reinterpret_cast<uint16_t*>(buf.data() + start)[c]));
            return result;
          }
        }
      }
      for (uint16_t j = 0; j < count; ++j) {
        uint16_t rec[6];
        size_t off = 6 + j * 12;
        if (off + 12 > buf.size()) break;
        for (int k = 0; k < 6; ++k)
          rec[k] = _byteswap_ushort(reinterpret_cast<uint16_t*>(buf.data() + off)[k]);
        if (rec[0] == 1 && rec[1] == 0 && rec[3] == 1) {
          uint16_t nameLen = rec[4];
          uint16_t nameOff = rec[5];
          size_t start = strOff + nameOff;
          if (start + nameLen <= buf.size()) {
            std::wstring result(nameLen / 2, L'\0');
            for (uint16_t c = 0; c < nameLen / 2; ++c)
              result[c] = static_cast<wchar_t>(_byteswap_ushort(reinterpret_cast<uint16_t*>(buf.data() + start)[c]));
            return result;
          }
        }
      }
    }
  }
  return L"";
}

inline std::vector<FontEntry> ScanFontDirectory(const std::filesystem::path& fontsDir) {
  std::vector<FontEntry> result;
  if (!std::filesystem::exists(fontsDir)) return result;
  for (const auto& entry : std::filesystem::directory_iterator(fontsDir)) {
    if (!entry.is_directory()) continue;
    std::wstring dirName = entry.path().filename().wstring();
    for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
      if (!file.is_regular_file()) continue;
      auto ext = file.path().extension().wstring();
      for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
      if (ext != L".ttf" && ext != L".otf") continue;
      std::wstring faceName = ReadTtfFaceName(file.path());
      if (faceName.empty()) faceName = dirName;
      result.push_back({ dirName, file.path().wstring(), faceName });
      break;
    }
  }
  std::sort(result.begin(), result.end(), [](const FontEntry& a, const FontEntry& b) {
    return a.displayName < b.displayName;
  });
  return result;
}

struct AppPaths {
  std::filesystem::path rootDir;
  std::filesystem::path collectorScript;
  std::filesystem::path crtShaderFile;
  std::vector<std::filesystem::path> shaderFiles;
  std::filesystem::path fontsDir;
  std::filesystem::path fontFile;
  std::vector<FontEntry> fontList;
  std::filesystem::path soundDir;
  std::filesystem::path introWave;
  std::filesystem::path iconFile;
  std::filesystem::path configFile;
  std::filesystem::path logsDir;
  std::filesystem::path exportsDir;
  std::vector<std::filesystem::path> borderFiles;
};

struct CRTSettings {
  float curvature = 0.02f;
  float scanlineIntensity = 0.30f;
  float sharpness = 0.55f;
  float maskIntensity = 0.55f;
  float chromaticAberration = 0.03f;
  float rgbShift = 0.0f;
  float noise = 0.018f;
  float grain = 0.0f;
  float jitter = 0.0f;
  float flicker = 0.012f;
  float vignette = 0.22f;
  float subpixelMode = 0.0f;
  float bloom = 0.0f;
  float burnInIntensity = 0.0f;
  float burnInDecayRate = 1.0f;
};

inline CRTSettings BuildCrtSettings(const Config& cfg) {
  CRTSettings s;
  s.curvature = static_cast<float>(std::clamp(cfg.crtCurvatureStrength, 0.0, 0.6));
  s.scanlineIntensity = static_cast<float>(std::clamp(cfg.crtScanlineIntensity, 0.0, 0.6));
  s.sharpness = static_cast<float>(std::clamp(cfg.crtSharpness, 0.0, 1.0));
  s.maskIntensity = 0.55f;
  s.chromaticAberration = static_cast<float>(std::clamp(cfg.crtChromaticAberration, 0.0, 0.16));
  s.rgbShift = static_cast<float>(std::clamp(cfg.crtRgbShift, 0.0, 0.1));
  s.noise = static_cast<float>(std::clamp(cfg.crtNoiseAmount, 0.0, 0.1));
  s.grain = static_cast<float>(std::clamp(cfg.crtGrain, 0.0, 0.1));
  s.jitter = static_cast<float>(std::clamp(cfg.crtJitter, 0.0, 1.0));
  s.flicker = static_cast<float>(std::clamp(cfg.crtFlickerAmount, 0.0, 0.08));
  s.vignette = static_cast<float>(std::clamp(cfg.crtVignetteStrength, 0.0, 1.0));
  if (cfg.borderEnabled) {
    s.vignette *= 0.25f;
  }
  s.subpixelMode = static_cast<float>(std::clamp(cfg.crtSubpixelMode, 0.0, 1.0));
  s.bloom = static_cast<float>(std::clamp(cfg.crtBloom, 0.0, 0.5));
  s.burnInIntensity = static_cast<float>(std::clamp(cfg.crtBurnInIntensity, 0.0, 1.0));
  s.burnInDecayRate = static_cast<float>(std::clamp(cfg.crtBurnInDecayRate, 0.01, 10.0));
  return s;
}

inline bool HasRepoMarkers(const std::filesystem::path& candidate) {
  return std::filesystem::exists(candidate / "src" / "native" / "collect.ps1") &&
         std::filesystem::exists(candidate / "tools" / "vhs-gothic.ttf");
}

inline AppPaths ResolveAppPaths() {
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

inline Config LoadConfigFromFile(const std::filesystem::path& configFile) {
  Config cfg;
  std::ifstream input(configFile);
  if (!input) return cfg;

  std::string line;
  while (std::getline(input, line)) {
    line = TrimAscii(line);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
    const auto separator = line.find('=');
    if (separator == std::string::npos) continue;

    const std::string key = ToLowerAscii(TrimAscii(line.substr(0, separator)));
    const std::string value = TrimAscii(line.substr(separator + 1));

    if (key == "border_image") {
      cfg.borderImage = Utf8ToWide(value);
    } else if (key == "log_buffer_size") {
      int parsed = 0;
      if (TryParseInt(value, parsed)) {
        cfg.logBufferSize = std::clamp(parsed, 64, 800);
      }
    } else if (key == "log_max_file_bytes") {
      UINT parsed = 0;
      if (TryParseUInt(value, parsed)) {
        cfg.logMaxFileBytes = std::clamp<std::uint64_t>(parsed, 65536ull, 32ull * 1024ull * 1024ull);
      }
    } else if (key == "user_id") {
      cfg.userId = Utf8ToWide(value);
    } else {
      RegistryLoadSetting(key, value, cfg);
    }
  }
  return cfg;
}

inline void SaveConfigToFile(const std::filesystem::path& configFile, const Config& cfg) {
  std::vector<std::pair<std::string, std::string>> preservedExtras;
  if (std::filesystem::exists(configFile)) {
    std::ifstream input(configFile);
    std::string line;
    while (std::getline(input, line)) {
      if (line.empty() || line[0] == ';' || line[0] == '[') continue;
      auto eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string key = line.substr(0, eq);
      auto trim = [](std::string& s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
        size_t start = 0; while (start < s.size() && s[start] == ' ') ++start;
        s = s.substr(start);
      };
      trim(key);
      static const char* knownKeys[] = {
        "theme_mode","language","show_fps","vsync","crt_postprocess","crt_intensity",
        "log_level","always_on_top","minimize_to_tray","start_with_windows",
        "start_maximized","save_window_position","border","font_scale","window_opacity",
        "process_priority","sound_enabled","log_milliseconds","telemetry_interval_ms",
        "snapshot_interval_ms","log_flush_interval_ms","log_retention_days",
        "log_plain_enabled","log_json_enabled","show_network_panel","show_ai_panel",
        "show_scram_panel","show_kernel_panel","show_gpu_panel","show_memory_panel",
        "show_disk_panel","show_display_panel","show_audio_panel","show_process_panel",
        "show_security_panel","show_script_panel","show_device_panel","show_storage_panel",
        "show_session_panel","show_registry_panel","show_reliability_panel",
        "show_filesystem_panel","show_health_panel","show_power_panel","show_thermal_panel",
        "show_driver_panel","show_sensor_panel","show_service_panel","show_forensic_panel",
        "show_integrity_panel","show_quarantine_panel","show_validation_panel",
        "show_sandbox_panel","show_source_panel","show_event_bus_panel","show_rule_engine_panel",
        "show_threshold_panel","show_pattern_panel","show_anomaly_panel","show_baseline_panel",
        "show_dpi_panel","show_audio_multimedia_panel","show_performance_panel",
        "show_stability_panel","show_font_panel","show_notification_panel","show_export_panel",
        "show_intro_panel","show_authentication_panel","show_about_panel",
        "border_image","log_buffer_size","log_max_file_bytes","user_id","font_face_index"
      };
      bool isKnown = false;
      for (const auto* k : knownKeys) { if (key == k) { isKnown = true; break; } }
      if (!isKnown) {
        std::string val = line.substr(eq + 1);
        trim(val);
        preservedExtras.push_back({key, val});
      }
    }
  }

  std::ofstream output(configFile, std::ios::trunc);
  if (!output) return;

  output << "; Monix runtime configuration\n";
  output << "; Settings can be edited live from the Settings tab or reloaded with F5.\n\n";

  const auto& registry = GetSettingsRegistry();
  for (const auto& def : registry) {
    RegistrySaveSetting(def.id, cfg, output);
  }

  output << "\nborder_image=" << WideToUtf8(cfg.borderImage) << "\n";
  output << "log_buffer_size=" << cfg.logBufferSize << "\n";
  output << "log_max_file_bytes=" << cfg.logMaxFileBytes << "\n";
  output << "user_id=" << WideToUtf8(cfg.userId) << "\n";

  if (!preservedExtras.empty()) {
    output << "\n; Preserved keys (managed by other subsystems)\n";
    for (const auto& [k, v] : preservedExtras) {
      output << k << "=" << v << "\n";
    }
  }
}

inline void WriteDefaultConfigIfMissing(const std::filesystem::path& configFile) {
  if (std::filesystem::exists(configFile)) {
    return;
  }

  std::ofstream output(configFile);
  output << "; Monix runtime configuration\n";
  output << "; Settings can be edited live from the Settings tab or reloaded with F5.\n";
  output << "delay_ms=75\n";
  output << "frame_interval_ms=33\n";
  output << "font_scale=1.12\n";
  output << "log_buffer_size=480\n";
  output << "log_visible_lines=26\n";
  output << "log_level=0\n";
  output << "log_view=0\n";
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

inline std::wstring GenerateSessionId() {
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
    static_cast<unsigned long>(GetCurrentProcessId()));
  return buffer;
}

}
