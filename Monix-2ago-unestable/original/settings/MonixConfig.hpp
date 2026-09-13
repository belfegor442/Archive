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

}
