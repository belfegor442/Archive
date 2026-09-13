#pragma once

#include "Types.hpp"
#include "../settings/MonixConfigTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace monix {

std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
std::wstring Trim(const std::wstring& value);
std::string TrimAscii(const std::string& value);
std::string ToLowerAscii(std::string value);
std::wstring ToUpper(std::wstring value);
std::vector<std::wstring> Split(const std::wstring& text, wchar_t delimiter);
std::wstring EscapeJson(std::wstring value);

int LogLevelRank(LogLevel level);
std::wstring LogLevelText(LogLevel level);
LogLevel ParseLogLevel(const std::wstring& severity);
std::string WideToAsciiLower(std::wstring value);
bool TryParseLogLevel(const std::string& value, LogLevel& out);
bool TryParseLogViewMode(const std::string& value, LogViewMode& out);
std::wstring LogViewModeText(LogViewMode mode);

template <typename T>
void AppendBounded(std::vector<T>& values, T value, int capacity) {
  values.push_back(value);
  if (capacity > 0 && static_cast<int>(values.size()) > capacity) {
    values.erase(values.begin(), values.begin() + (values.size() - capacity));
  }
}

bool TryParseUInt(const std::string& value, UINT& out);
bool TryParseInt(const std::string& value, int& out);
bool TryParseDouble(const std::string& value, double& out);
bool TryParseBool(const std::string& value, bool& out);
std::uint64_t FileTimeToUInt64(const FILETIME& value);
std::wstring FormatIpv4(DWORD address);
std::wstring FormatProcessGuid(int pid, std::uint64_t createTime);
std::wstring FormatClockNow(bool includeMilliseconds = false);
std::wstring FormatIsoTimestampNow(bool includeMilliseconds = true);
std::wstring FormatPercent(double value);
std::wstring FormatDecimal(double value);
std::wstring FormatBytes(std::uint64_t bytes);
std::wstring FormatRate(std::uint64_t bytesPerSec);
std::wstring FormatTemperature(double value, bool estimated);
std::wstring FormatDuration(std::uint64_t uptimeSeconds);
std::wstring BuildBar(double value, int segments = 16);

}
