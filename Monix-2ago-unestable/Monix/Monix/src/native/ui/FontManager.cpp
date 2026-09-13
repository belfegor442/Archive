#include "MonixApp.hpp"

#include "CoreMonitorTheme.hpp"
#include "../core/TextUtils.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

using namespace monix;

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
  state_.currentFontIndex = state_.currentFontIndex % static_cast<int>(paths_.fontList.size());
  if (state_.currentFontIndex < 0) state_.currentFontIndex = 0;
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

void MonixApp::DestroyUiFonts() {
  if (tabFont_) DeleteObject(tabFont_);
  if (bodyFont_) DeleteObject(bodyFont_);
  if (smallFont_) DeleteObject(smallFont_);
  if (titleFont_) DeleteObject(titleFont_);
  if (logFont_) DeleteObject(logFont_);
  if (logoFont_) DeleteObject(logoFont_);
  tabFont_ = nullptr;
  bodyFont_ = nullptr;
  smallFont_ = nullptr;
  titleFont_ = nullptr;
  logFont_ = nullptr;
  logoFont_ = nullptr;
}

void MonixApp::MeasureFontMetrics() {
  HDC screen = GetDC(nullptr);
  if (!screen) return;
  const auto measure = [&](HFONT font) {
    TEXTMETRICW metrics {};
    HGDIOBJ old = SelectObject(screen, font);
    GetTextMetricsW(screen, &metrics);
    SelectObject(screen, old);
    return metrics.tmHeight + metrics.tmExternalLeading;
  };

  if (bodyFont_) bodyLineHeight_ = measure(bodyFont_);
  if (smallFont_) smallLineHeight_ = measure(smallFont_);
  if (logFont_) logLineHeight_ = measure(logFont_);
  if (logoFont_) logoLineHeight_ = measure(logoFont_);
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
  tabFont_ = CreateFontW(-scale(30), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str());
  bodyFont_ = CreateFontW(-scale(22), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str());
  smallFont_ = CreateFontW(-scale(17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str());
  titleFont_ = CreateFontW(-scale(34), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str());
  logFont_ = CreateFontW(-scale(18), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, fontCharset, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, quality, FIXED_PITCH | FF_MODERN, fontFace_.c_str());
  logoFont_ = CreateFontW(-scale(20), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, OEM_CHARSET, OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Terminal");

  MeasureFontMetrics();
}
