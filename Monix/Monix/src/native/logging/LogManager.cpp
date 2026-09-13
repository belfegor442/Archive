#include "LogManager.hpp"

#include <algorithm>

namespace monix {

static std::uint64_t MonotonicNs() {
  static LARGE_INTEGER freq = {};
  if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<std::uint64_t>(now.QuadPart) * 1000000000ULL /
    static_cast<std::uint64_t>(freq.QuadPart);
}

LogManager::LogManager(const LogManagerConfig& config)
    : config_(config) {}

bool LogManager::Push(std::wstring domain,
                      std::wstring severity,
                      std::wstring message,
                      ColorRole color,
                      std::wstring module,
                      std::wstring service,
                      std::wstring metadata,
                      EventType eventType) {
  const LogLevel level = ParseLogLevel(severity);
  const std::wstring normalizedSeverity = LogLevelText(level);

  LogEntry entry;
  bool isNewEntry = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (LogLevelRank(level) < LogLevelRank(config_.minLevel)) {
      return false;
    }

    auto& rateLimiter = healthManager_.GetRateLimiter(domain);
    if (!rateLimiter.Allow()) {
      return false;
    }

    if (!globalRateLimiter_.Allow(level)) {
      return false;
    }

    if (config_.deduplicate && domain != L"SCRAM" && !entries_.empty()) {
      const int scanLimit = (std::min)(static_cast<int>(entries_.size()), 100);
      for (int i = 0; i < scanLimit; ++i) {
        auto& existing = entries_[i];
        if (existing.domain == domain &&
            existing.severity == normalizedSeverity &&
            existing.message == message &&
            existing.module == module &&
            existing.service == service &&
            existing.eventType == eventType) {
          existing.repeatCount += 1;
          existing.sequenceId = nextSequenceId_++;
          return false;
        }
      }
    }

    isNewEntry = true;
    const std::uint64_t monoNs = MonotonicNs();
    const std::wstring now = FormatClockNow(config_.logMilliseconds);
    const std::wstring nowIso = FormatIsoTimestampNow(config_.logMilliseconds);
    entry.time = now;
    entry.fullTimestamp = nowIso;
    entry.firstTime = now;
    entry.firstFullTimestamp = nowIso;
    entry.domain = std::move(domain);
    entry.severity = normalizedSeverity;
    entry.module = std::move(module);
    entry.service = std::move(service);
    entry.message = std::move(message);
    entry.metadata = std::move(metadata);
    entry.sessionId = config_.sessionId;
    entry.userId = config_.userId;
    entry.eventId = nextEventId_++;
    entry.sequenceId = nextSequenceId_++;
    entry.createdMonotonicNs = monoNs;
    entry.processId = GetCurrentProcessId();
    entry.threadId = GetCurrentThreadId();
    entry.level = level;
    entry.color = color;
    entry.eventType = eventType;

    entries_.insert(entries_.begin(), entry);
    if (static_cast<int>(entries_.size()) > config_.bufferSize) {
      entries_.resize(config_.bufferSize);
    }

    UpdateCounters(level, color);
    UpdateHistory(level, color);
  }

  if (level == LogLevel::Warn || level == LogLevel::Error || level == LogLevel::Critical) {
    if (onNotification) onNotification(entry);
  }
  return isNewEntry;
}

void LogManager::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_.clear();
  counters_ = {};
  nextEventId_ = 1;
  nextSequenceId_ = 1;
  warnHistory_.clear();
  errHistory_.clear();
  critHistory_.clear();
  kernelHistory_.clear();
  netHistory_.clear();
}

void LogManager::UpdateCounters(LogLevel level, ColorRole /*color*/) {
  switch (level) {
    case LogLevel::Debug: ++counters_.debug; break;
    case LogLevel::Info: ++counters_.info; break;
    case LogLevel::Warn: ++counters_.warnings; break;
    case LogLevel::Error: ++counters_.errors; break;
    case LogLevel::Critical: ++counters_.critical; break;
  }
}

void LogManager::UpdateHistory(LogLevel level, ColorRole color) {
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
  ema(warnHistory_,   isWarn);
  ema(errHistory_,    isErr);
  ema(critHistory_,   isCrit);
  ema(kernelHistory_, isKernel);
  ema(netHistory_,    isNet);
}

} // namespace monix
