#include "LogSink.hpp"

#include "../MonixApp.hpp"
#include "../core/TextUtils.hpp"

#include <windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>

using namespace monix;

std::filesystem::path MonixApp::ResolveLogFilePathLocked(const std::wstring& extension, ULONGLONG maxFileBytes) const {
  std::filesystem::create_directories(paths_.logsDir);

  SYSTEMTIME time {};
  GetLocalTime(&time);
  wchar_t baseName[64];
  swprintf(baseName, 64, L"monix-%04d-%02d-%02d", time.wYear, time.wMonth, time.wDay);

  std::error_code ec;
  std::filesystem::path candidate = paths_.logsDir / (std::wstring(baseName) + L"." + extension);

  if (std::filesystem::exists(candidate) && std::filesystem::file_size(candidate, ec) >= maxFileBytes) {
    wchar_t stamp[32];
    swprintf(stamp, 32, L"-%02d%02d%02d", time.wHour, time.wMinute, time.wSecond);
    candidate = paths_.logsDir / (std::wstring(baseName) + std::wstring(stamp) + L"." + extension);
  }

  return candidate;
}

std::filesystem::path MonixApp::ResolveLogFilePath(const std::wstring& extension) const {
  std::shared_lock<std::shared_mutex> lock(configMutex_);
  return ResolveLogFilePathLocked(extension, config_.logMaxFileBytes);
}

void MonixApp::CleanupLogFiles() {
  if (!std::filesystem::exists(paths_.logsDir)) {
    return;
  }

  std::shared_lock<std::shared_mutex> lock(configMutex_);
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

void MonixApp::CleanupStaleLogFiles() {
  if (!std::filesystem::exists(paths_.logsDir)) {
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(paths_.logsDir)) {
    if (!entry.is_regular_file()) continue;
    const auto stem = entry.path().stem().wstring();
    const auto ext = entry.path().extension().wstring();
    if (ext != L".log" && ext != L".jsonl") continue;
    if (stem.length() <= 16) continue;
    if (stem.substr(0, 6) != L"monix-") continue;
    if (stem[16] != L'-') continue;
    bool allDigits = true;
    for (size_t k = 17; k < stem.length(); ++k) {
      if (stem[k] < L'0' || stem[k] > L'9') { allDigits = false; break; }
    }
    if (allDigits) {
      std::error_code ec;
      std::filesystem::remove(entry.path(), ec);
    }
  }
}

void MonixApp::FlushLogQueues(bool force) {
  const ULONGLONG now = GetTickCount64();

  bool doFlush = false;
  bool logPlain = false;
  bool logJson = false;
  {
    std::unique_lock<std::shared_mutex> lock(configMutex_);
    if (!force && now - lastLogFlushAtMs_ < config_.logFlushIntervalMs) {
      return;
    }
    if ((pendingPlainLogs_.empty() && pendingJsonLogs_.empty()) && !force) {
      lastLogFlushAtMs_ = now;
      if (now - lastRetentionSweepAtMs_ > 60000) {
        lastRetentionSweepAtMs_ = now;
      }
      return;
    }
    doFlush = true;
    logPlain = config_.logPlainEnabled;
    logJson = config_.logJsonEnabled;
    lastLogFlushAtMs_ = now;
    if (now - lastRetentionSweepAtMs_ > 60000 || force) {
      lastRetentionSweepAtMs_ = now;
    }
  }

  if (!doFlush) return;

  std::filesystem::create_directories(paths_.logsDir);

  std::vector<std::wstring> plainBatch;
  std::vector<std::wstring> jsonBatch;
  {
    std::unique_lock<std::shared_mutex> lock(configMutex_);
    if (logPlain && !pendingPlainLogs_.empty()) {
      plainBatch = std::move(pendingPlainLogs_);
      pendingPlainLogs_.clear();
    }
    if (logJson && !pendingJsonLogs_.empty()) {
      jsonBatch = std::move(pendingJsonLogs_);
      pendingJsonLogs_.clear();
    }
  }

  if (!plainBatch.empty()) {
    std::wofstream output(ResolveLogFilePath(L"log"), std::ios::app);
    if (output.is_open()) {
      for (const auto& line : plainBatch) {
        output << line << L"\n";
      }
    }
  }

  if (!jsonBatch.empty()) {
    std::wofstream output(ResolveLogFilePath(L"jsonl"), std::ios::app);
    if (output.is_open()) {
      for (const auto& line : jsonBatch) {
        output << line << L"\n";
      }
    }
  }

  CleanupLogFiles();
}

void MonixApp::PushLog(
  std::wstring domain,
  std::wstring severity,
  std::wstring message,
  ColorRole color,
  std::wstring module,
  std::wstring service,
  std::wstring metadata,
  monix::EventType eventType
) {
  const bool isNewEntry = logManager_->Push(std::move(domain), std::move(severity), std::move(message),
                     color, std::move(module), std::move(service), std::move(metadata),
                     eventType);

  {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    state_.logState.entries = logManager_->Entries();
    state_.logState.counters = logManager_->Counters();
    state_.logState.nextEventId = logManager_->NextEventId();
    state_.logState.warnHistory = logManager_->WarnHistory();
    state_.logState.errHistory = logManager_->ErrHistory();
    state_.logState.critHistory = logManager_->CritHistory();
    state_.logState.kernelHistory = logManager_->KernelHistory();
    state_.logState.netHistory = logManager_->NetHistory();
  }

  if (!isNewEntry) return;

  const auto& logEntries = state_.logState.entries;
  if (!logEntries.empty()) {
    const auto& entry = logEntries.front();
    std::unique_lock<std::shared_mutex> cfgLock(configMutex_);
    if (config_.logPlainEnabled) {
      std::wstring plain = entry.fullTimestamp +
        L" [" + entry.severity + L"]" +
        L" [" + entry.domain + L"]" +
        L" [module=" + entry.module + L"]" +
        L" [service=" + entry.service + L"]" +
        L" [session=" + entry.sessionId + L"]" +
        L" [event=" + std::to_wstring(entry.eventId) + L"]" +
        L" [seq=" + std::to_wstring(entry.sequenceId) + L"]" +
        L" [pid=" + std::to_wstring(entry.processId) + L"]" +
        L" [tid=" + std::to_wstring(entry.threadId) + L"] " +
        entry.message;
      if (entry.repeatCount > 1) {
        plain += L" (x" + std::to_wstring(entry.repeatCount) + L" since " + entry.firstTime + L")";
      }
      if (!entry.metadata.empty()) {
        plain += L" { " + entry.metadata + L" }";
      }
      pendingPlainLogs_.push_back(std::move(plain));
    }

    if (config_.logJsonEnabled) {
      std::wstring json =
        L"{\"timestamp\":\"" + EscapeJson(entry.fullTimestamp) +
        L"\",\"first_timestamp\":\"" + EscapeJson(entry.firstFullTimestamp) +
        L"\",\"level\":\"" + EscapeJson(entry.severity) +
        L"\",\"event_type\":\"" + EscapeJson(EventTypeName(entry.eventType)) +
        L"\",\"domain\":\"" + EscapeJson(entry.domain) +
        L"\",\"module\":\"" + EscapeJson(entry.module) +
        L"\",\"service\":\"" + EscapeJson(entry.service) +
        L"\",\"message\":\"" + EscapeJson(entry.message) +
        L"\",\"metadata\":\"" + EscapeJson(entry.metadata) +
        L"\",\"session_id\":\"" + EscapeJson(entry.sessionId) +
        L"\",\"user_id\":\"" + EscapeJson(entry.userId) +
        L"\",\"event_id\":" + std::to_wstring(entry.eventId) +
        L",\"sequence_id\":" + std::to_wstring(entry.sequenceId) +
        L",\"process_id\":" + std::to_wstring(entry.processId) +
        L",\"thread_id\":" + std::to_wstring(entry.threadId) +
        L",\"repeat_count\":" + std::to_wstring(entry.repeatCount) +
        L"}";
      pendingJsonLogs_.push_back(std::move(json));
    }
  }
}
