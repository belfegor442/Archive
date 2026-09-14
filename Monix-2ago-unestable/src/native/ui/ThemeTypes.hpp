#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "../core/TextUtils.hpp"
#include "../logging/LogEntry.hpp"
#include "../settings/MonixConfigTypes.hpp"
#include "../telemetry/Snapshot.hpp"
#include "AppState.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace monix::ui {

constexpr int kCoreMonitorThemeMode = 3;
constexpr int kWin98ThemeMode = 4;

struct CoreMonitorThemeFonts {
  HFONT title = nullptr;
  HFONT body = nullptr;
  HFONT smallText = nullptr;
  HFONT logText = nullptr;
  int bodyLineHeight = 28;
  int smallLineHeight = 20;
  int logLineHeight = 20;
};

struct CoreMonitorThemeContext {
  const Snapshot* snapshot = nullptr;
  const std::vector<LogEntry>* logs = nullptr;
  const SessionCounters* counters = nullptr;
  const std::vector<double>* cpuHistory = nullptr;
  const std::vector<double>* ramHistory = nullptr;
  const std::vector<double>* gpuHistory = nullptr;
  const std::vector<double>* netHistory = nullptr;
  const std::vector<double>* netUploadHistory = nullptr;
  Config config;
  CoreMonitorThemeFonts fonts;
  std::wstring rendererName;
  std::wstring shaderName;
  std::wstring fontName;
  int menuIndex = 0;
  bool livePaused = false;
  std::uint64_t frameCount = 0;
  const UpdateState* updateState = nullptr;
  int settingsCategory = 0;
  int pressedButton = -1;
  int hoveredMenuIndex = -1;
  int activeFilter = 0;
  int taskScroll = 0;
  int selectedTaskPid = 0;
};

inline const Snapshot& SnapshotOrDefault(const CoreMonitorThemeContext& ctx) {
  static const Snapshot fallback {};
  return ctx.snapshot ? *ctx.snapshot : fallback;
}

inline std::wstring ShortText(const std::wstring& value, std::size_t limit) {
  if (value.size() <= limit) return value;
  if (limit <= 3) return value.substr(0, limit);
  return value.substr(0, limit - 3) + L"...";
}

inline std::wstring Fixed(double v, int decimals) {
  std::wostringstream out;
  out << std::fixed << std::setprecision(decimals) << v;
  return out.str();
}

inline std::wstring Hms(std::uint64_t seconds) {
  const auto h = static_cast<int>(seconds / 3600);
  const auto m = static_cast<int>((seconds % 3600) / 60);
  const auto s = static_cast<int>(seconds % 60);
  std::wostringstream out;
  out << std::setw(2) << std::setfill(L'0') << h << L":"
      << std::setw(2) << std::setfill(L'0') << m << L":"
      << std::setw(2) << std::setfill(L'0') << s;
  return out.str();
}

inline std::wstring FormatBytes(std::uint64_t bytes) {
  if (bytes == 0) return L"0 B";
  const char* units[] = { "B", "KB", "MB", "GB", "TB" };
  int unit = 0;
  double size = static_cast<double>(bytes);
  while (size >= 1024.0 && unit < 4) { size /= 1024.0; ++unit; }
  std::wostringstream out;
  out << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << size << L" " << units[unit];
  return out.str();
}

inline std::wstring RateText(std::uint64_t bytesPerSec, double fallbackMb) {
  if (bytesPerSec == 0) return Fixed(fallbackMb, 1) + L" MB/s";
  return FormatBytes(bytesPerSec) + L"/s";
}

inline std::wstring DateTimeText() {
  SYSTEMTIME st {};
  GetLocalTime(&st);
  std::wostringstream out;
  out << std::setw(2) << std::setfill(L'0') << st.wMonth << L"/"
      << std::setw(2) << std::setfill(L'0') << st.wDay << L"/"
      << st.wYear << L" "
      << std::setw(2) << std::setfill(L'0') << st.wHour << L":"
      << std::setw(2) << std::setfill(L'0') << st.wMinute;
  return out.str();
}

}  // namespace monix::ui
