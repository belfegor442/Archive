#include "MonixApp.hpp"

#include "../../core/TextUtils.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace monix;

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

  std::deque<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    line = TrimAscii(line);
    if (line.empty()) {
      continue;
    }
    if (static_cast<int>(lines.size()) == limit) {
      lines.pop_front();  // O(1) instead of O(n) with vector
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
      entry.metadata = payload.substr(metadataStart + 3, payload.size() - metadataStart - 5);
    } else {
      entry.message = payload;
    }

    entry.level = ParseLogLevel(entry.severity);
    entry.severity = LogLevelText(entry.level);
    entry.color = resolveColor(entry.domain, entry.level);
    highestEventId = std::max(highestEventId, entry.eventId);
    state_.logState.entries.push_back(std::move(entry));
  }

  state_.logState.nextEventId = std::max(state_.logState.nextEventId, highestEventId + 1);
}
