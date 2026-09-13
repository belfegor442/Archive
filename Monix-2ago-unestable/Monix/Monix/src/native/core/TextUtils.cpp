#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "TextUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cwctype>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace monix {

std::wstring Utf8ToWide(const std::string& text) {
  if (text.empty()) {
    return L"";
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  if (size <= 0) {
    return std::wstring(text.begin(), text.end());
  }

  std::wstring output(size, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), output.data(), size);
  return output;
}

std::string WideToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return "";
  }

  const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    std::string fallback;
    fallback.reserve(text.size());
    for (wchar_t ch : text) {
      fallback.push_back(static_cast<char>(ch & 0xFF));
    }
    return fallback;
  }

  std::string output(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), output.data(), size, nullptr, nullptr);
  return output;
}

std::wstring Trim(const std::wstring& value) {
  const auto begin = value.find_first_not_of(L" \t\r\n");
  if (begin == std::wstring::npos) {
    return L"";
  }
  const auto end = value.find_last_not_of(L" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string TrimAscii(const std::string& value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::wstring ToUpper(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towupper(ch));
  });
  return value;
}

std::vector<std::wstring> Split(const std::wstring& text, wchar_t delimiter) {
  std::vector<std::wstring> parts;
  std::wstring current;
  for (wchar_t ch : text) {
    if (ch == delimiter) {
      parts.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  parts.push_back(current);
  return parts;
}

std::wstring EscapeJson(std::wstring value) {
  std::wstring escaped;
  escaped.reserve(value.size() + 16);
  for (wchar_t ch : value) {
    switch (ch) {
      case L'\\': escaped += L"\\\\"; break;
      case L'"': escaped += L"\\\""; break;
      case L'\r': escaped += L"\\r"; break;
      case L'\n': escaped += L"\\n"; break;
      case L'\t': escaped += L"\\t"; break;
      default: escaped.push_back(ch); break;
    }
  }
  return escaped;
}

int LogLevelRank(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return 0;
    case LogLevel::Info: return 1;
    case LogLevel::Warn: return 2;
    case LogLevel::Error: return 3;
    case LogLevel::Critical: return 4;
    default: return 1;
  }
}

std::wstring LogLevelText(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return L"DEBUG";
    case LogLevel::Info: return L"INFO";
    case LogLevel::Warn: return L"WARN";
    case LogLevel::Error: return L"ERROR";
    case LogLevel::Critical: return L"CRITICAL";
    default: return L"INFO";
  }
}

LogLevel ParseLogLevel(const std::wstring& severity) {
  const std::wstring upper = ToUpper(severity);
  if (upper == L"DEBUG") return LogLevel::Debug;
  if (upper == L"WARN" || upper == L"WARNING") return LogLevel::Warn;
  if (upper == L"ERROR") return LogLevel::Error;
  if (upper == L"CRITICAL" || upper == L"FATAL") return LogLevel::Critical;
  return LogLevel::Info;
}

std::string WideToAsciiLower(std::wstring value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch & 0xFF))));
  }
  return out;
}

bool TryParseLogLevel(const std::string& value, LogLevel& out) {
  const std::string lowered = ToLowerAscii(TrimAscii(value));
  if (lowered == "debug") { out = LogLevel::Debug; return true; }
  if (lowered == "info") { out = LogLevel::Info; return true; }
  if (lowered == "warn" || lowered == "warning") { out = LogLevel::Warn; return true; }
  if (lowered == "error") { out = LogLevel::Error; return true; }
  if (lowered == "critical" || lowered == "fatal") { out = LogLevel::Critical; return true; }
  return false;
}

bool TryParseLogViewMode(const std::string& value, LogViewMode& out) {
  const std::string lowered = ToLowerAscii(TrimAscii(value));
  if (lowered == "timeline") { out = LogViewMode::Timeline; return true; }
  if (lowered == "structured") { out = LogViewMode::Structured; return true; }
  if (lowered == "compact") { out = LogViewMode::Compact; return true; }
  return false;
}

std::wstring LogViewModeText(LogViewMode mode) {
  switch (mode) {
    case LogViewMode::Timeline: return L"TIMELINE";
    case LogViewMode::Structured: return L"STRUCTURED";
    case LogViewMode::Compact: return L"COMPACT";
    default: return L"STRUCTURED";
  }
}

bool TryParseUInt(const std::string& value, UINT& out) {
  try {
    out = static_cast<UINT>(std::stoul(value));
    return true;
  } catch (...) {
    return false;
  }
}

bool TryParseInt(const std::string& value, int& out) {
  try {
    out = std::stoi(value);
    return true;
  } catch (...) {
    return false;
  }
}

bool TryParseDouble(const std::string& value, double& out) {
  try {
    out = std::stod(value);
    return true;
  } catch (...) {
    return false;
  }
}

bool TryParseBool(const std::string& value, bool& out) {
  const std::string lowered = ToLowerAscii(TrimAscii(value));
  if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
    out = true;
    return true;
  }
  if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
    out = false;
    return true;
  }
  return false;
}

std::uint64_t FileTimeToUInt64(const FILETIME& value) {
  ULARGE_INTEGER integer {};
  integer.LowPart = value.dwLowDateTime;
  integer.HighPart = value.dwHighDateTime;
  return integer.QuadPart;
}

std::wstring FormatIpv4(DWORD address) {
  IN_ADDR addr {};
  addr.S_un.S_addr = address;
  char buffer[INET_ADDRSTRLEN] {};
  if (!inet_ntop(AF_INET, &addr, buffer, static_cast<DWORD>(sizeof(buffer)))) {
    return L"0.0.0.0";
  }
  return Utf8ToWide(buffer);
}

std::wstring FormatProcessGuid(int pid, std::uint64_t createTime) {
  wchar_t buffer[96];
  swprintf(buffer, 96, L"P-%08X-%016llX", static_cast<unsigned int>(pid), static_cast<unsigned long long>(createTime));
  return buffer;
}

std::wstring FormatClockNow(bool includeMilliseconds) {
  SYSTEMTIME time {};
  GetLocalTime(&time);
  wchar_t buffer[48];
  if (includeMilliseconds) {
    swprintf(buffer, 48, L"%02d/%02d %02d:%02d:%02d.%03d", time.wDay, time.wMonth, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
  } else {
    swprintf(buffer, 48, L"%02d/%02d %02d:%02d:%02d", time.wDay, time.wMonth, time.wHour, time.wMinute, time.wSecond);
  }
  return buffer;
}

std::wstring FormatIsoTimestampNow(bool includeMilliseconds) {
  SYSTEMTIME time {};
  GetLocalTime(&time);
  wchar_t buffer[48];
  if (includeMilliseconds) {
    swprintf(buffer, 48, L"%04d-%02d-%02dT%02d:%02d:%02d.%03d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
  } else {
    swprintf(buffer, 48, L"%04d-%02d-%02dT%02d:%02d:%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
  }
  return buffer;
}

std::wstring FormatPercent(double value) {
  wchar_t buffer[32];
  swprintf(buffer, 32, L"%.0f%%", value);
  return buffer;
}

std::wstring FormatDecimal(double value) {
  wchar_t buffer[32];
  swprintf(buffer, 32, L"%.1f", value);
  return buffer;
}

std::wstring FormatBytes(std::uint64_t bytes) {
  const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
  double size = static_cast<double>(bytes);
  int unit = 0;
  while (size >= 1024.0 && unit < 4) {
    size /= 1024.0;
    ++unit;
  }

  wchar_t buffer[64];
  if (unit == 0) {
    swprintf(buffer, 64, L"%llu %ls", static_cast<unsigned long long>(bytes), units[unit]);
  } else if (size >= 100.0) {
    swprintf(buffer, 64, L"%.0f%ls", size, units[unit]);
  } else {
    swprintf(buffer, 64, L"%.1f%ls", size, units[unit]);
  }
  return buffer;
}

std::wstring FormatRate(std::uint64_t bytesPerSec) {
  return FormatBytes(bytesPerSec) + L"/s";
}

std::wstring FormatTemperature(double value, bool estimated) {
  if (value <= 0.0) {
    return L"N/A";
  }
  return FormatDecimal(value) + (estimated ? L"C EST" : L"C");
}

std::wstring FormatDuration(std::uint64_t uptimeSeconds) {
  const std::uint64_t days = uptimeSeconds / 86400ull;
  const std::uint64_t hours = (uptimeSeconds % 86400ull) / 3600ull;
  const std::uint64_t minutes = (uptimeSeconds % 3600ull) / 60ull;
  wchar_t buffer[64];
  if (days > 0) {
    swprintf(buffer, 64, L"%llud %02lluh %02llum", static_cast<unsigned long long>(days), static_cast<unsigned long long>(hours), static_cast<unsigned long long>(minutes));
  } else {
    swprintf(buffer, 64, L"%02lluh %02llum", static_cast<unsigned long long>(hours), static_cast<unsigned long long>(minutes));
  }
  return buffer;
}

std::wstring BuildBar(double value, int segments) {
  const int filled = std::clamp(static_cast<int>((value / 100.0) * segments + 0.5), 0, segments);
  std::wstring bar = L"[";
  for (int i = 0; i < segments; ++i) {
    bar += i < filled ? L'#' : L'-';
  }
  bar += L"]";
  return bar;
}

COLORREF ResolveColor(ColorRole role) {
  switch (role) {
    case ColorRole::Black: return RGB(5, 5, 5);
    case ColorRole::Primary: return RGB(0, 255, 102);
    case ColorRole::Success: return RGB(92, 248, 144);
    case ColorRole::Warning: return RGB(255, 176, 0);
    case ColorRole::Error: return RGB(255, 92, 74);
    case ColorRole::Network: return RGB(79, 213, 255);
    case ColorRole::Scram: return RGB(244, 234, 200);
    case ColorRole::Kernel: return RGB(255, 176, 0);
    case ColorRole::Dim: return RGB(118, 148, 118);
    case ColorRole::Accent: return RGB(28, 84, 42);
    case ColorRole::Inverse: return RGB(8, 12, 8);
    case ColorRole::Idle: return RGB(34, 40, 34);
    case ColorRole::Engine: return RGB(79, 213, 255);
    case ColorRole::Security: return RGB(220, 198, 146);
    case ColorRole::HealthScore: return RGB(154, 255, 120);
    case ColorRole::Storage: return RGB(214, 188, 140);
    case ColorRole::CriticalScram: return RGB(255, 196, 116);
    case ColorRole::UserInput: return RGB(182, 232, 255);
    case ColorRole::Fatal: return RGB(132, 34, 24);
    case ColorRole::Thermal: return RGB(255, 176, 0);
    case ColorRole::White:
    default:
      return RGB(244, 236, 216);
  }
}

COLORREF ResolveGlowColor(ColorRole role) {
  switch (role) {
    case ColorRole::Warning: return RGB(72, 44, 0);
    case ColorRole::Error: return RGB(74, 16, 12);
    case ColorRole::Network: return RGB(8, 34, 44);
    case ColorRole::Scram: return RGB(42, 34, 18);
    case ColorRole::Kernel: return RGB(62, 34, 0);
    case ColorRole::Dim: return RGB(12, 28, 14);
    case ColorRole::Idle: return RGB(10, 10, 10);
    case ColorRole::Engine: return RGB(8, 34, 44);
    case ColorRole::Security: return RGB(34, 28, 18);
    case ColorRole::HealthScore: return RGB(18, 42, 18);
    case ColorRole::Storage: return RGB(38, 30, 18);
    case ColorRole::CriticalScram: return RGB(64, 42, 12);
    case ColorRole::UserInput: return RGB(10, 34, 44);
    case ColorRole::Fatal: return RGB(38, 8, 6);
    case ColorRole::Thermal: return RGB(62, 34, 0);
    case ColorRole::Success:
    case ColorRole::Primary:
    case ColorRole::White:
    default:
      return RGB(8, 34, 14);
  }
}

void FillSolid(HDC dc, const RECT& rect, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

void DrawRectOutline(HDC dc, const RECT& rect, COLORREF color) {
  HPEN pen = CreatePen(PS_SOLID, 1, color);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(pen);
}

RECT ShrinkRect(RECT rect, int amount) {
  rect.left += amount;
  rect.top += amount;
  rect.right -= amount;
  rect.bottom -= amount;
  return rect;
}

}
