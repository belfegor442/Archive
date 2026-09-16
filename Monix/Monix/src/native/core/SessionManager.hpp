#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../events/SystemEvent.hpp"
#include "../core/snapshot/SystemSnapshot.hpp"
#include "../scram/ScramEngine.hpp"
#include "Timeline.hpp"

namespace monix {

struct SessionMetadata {
  std::wstring id;
  std::wstring name;
  uint64_t startTimeNs = 0;
  uint64_t endTimeNs = 0;
  std::wstring machineName;
  std::wstring osVersion;
  std::wstring monixVersion;
  int totalSnapshots = 0;
  int totalEvents = 0;
  int totalFindings = 0;
  int peakRiskScore = 0;
  std::wstring notes;
};

struct Session {
  SessionMetadata metadata;
  std::vector<SystemSnapshot> snapshots;
  std::vector<SystemEvent> events;
  std::vector<ScramResult> findings;
  std::vector<TimelineEntry> timeline;

  double avgCpu = 0.0;
  double avgRamPct = 0.0;
  double avgGpu = 0.0;
  int errorCount = 0;
  int warningCount = 0;
  int criticalCount = 0;
};

class SessionManager {
public:
  Session& StartSession(const std::wstring& name = L"") {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = std::make_unique<Session>();
    current_->metadata.id = GenerateSessionId();
    current_->metadata.name = name.empty() ? current_->metadata.id : name;
    current_->metadata.startTimeNs = MonotonicNs();
    paused_ = false;
    return *current_;
  }

  void StopSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_) {
      current_->metadata.endTimeNs = MonotonicNs();
      ComputeSummary();
      history_.push_back(current_->metadata);
      current_.reset();
      paused_ = false;
    }
  }

  void PauseSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
  }

  void ResumeSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = false;
  }

  Session* CurrentSession() {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_.get();
  }

  const Session* CurrentSession() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_.get();
  }

  bool HasActiveSession() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_ != nullptr && !paused_;
  }

  bool IsPaused() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return paused_;
  }

  void RecordSnapshot(const SystemSnapshot& snap) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_ || paused_) return;
    current_->snapshots.push_back(snap);
    current_->metadata.totalSnapshots = static_cast<int>(current_->snapshots.size());
  }

  void RecordEvent(const SystemEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_ || paused_) return;
    current_->events.push_back(event);
    current_->metadata.totalEvents = static_cast<int>(current_->events.size());
    if (event.severity == EventSeverity::Error) current_->errorCount++;
    if (event.severity == EventSeverity::Warning) current_->warningCount++;
    if (event.severity == EventSeverity::Critical) current_->criticalCount++;
  }

  void RecordFinding(const ScramResult& finding) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_ || paused_) return;
    current_->findings.push_back(finding);
    current_->metadata.totalFindings = static_cast<int>(current_->findings.size());
    if (finding.riskScore > current_->metadata.peakRiskScore) {
      current_->metadata.peakRiskScore = finding.riskScore;
    }
  }

  void RecordTimeline(const TimelineEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!current_ || paused_) return;
    current_->timeline.push_back(entry);
  }

  const std::vector<SessionMetadata>& History() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
  }

  struct SessionComparison {
    int snapshotCountDiff = 0;
    int eventCountDiff = 0;
    int findingCountDiff = 0;
    double avgCpuDiff = 0.0;
    double avgRamDiff = 0.0;
    double avgGpuDiff = 0.0;
    int errorDiff = 0;
    int warningDiff = 0;
    std::wstring summary;
  };

  static SessionComparison CompareSessions(const Session& a, const Session& b) {
    SessionComparison c;
    c.snapshotCountDiff = a.metadata.totalSnapshots - b.metadata.totalSnapshots;
    c.eventCountDiff = a.metadata.totalEvents - b.metadata.totalEvents;
    c.findingCountDiff = a.metadata.totalFindings - b.metadata.totalFindings;
    c.avgCpuDiff = a.avgCpu - b.avgCpu;
    c.avgRamDiff = a.avgRamPct - b.avgRamPct;
    c.avgGpuDiff = a.avgGpu - b.avgGpu;
    c.errorDiff = a.errorCount - b.errorCount;
    c.warningDiff = a.warningCount - b.warningCount;

    c.summary = L"Session " + a.metadata.id + L" vs " + b.metadata.id + L": ";
    c.summary += L"Snapshots " + SignStr(c.snapshotCountDiff) + L", ";
    c.summary += L"Events " + SignStr(c.eventCountDiff) + L", ";
    c.summary += L"Findings " + SignStr(c.findingCountDiff) + L", ";
    c.summary += L"CPU " + SignStrF(c.avgCpuDiff) + L"%, ";
    c.summary += L"RAM " + SignStrF(c.avgRamDiff) + L"%, ";
    c.summary += L"Errors " + SignStr(c.errorDiff);
    return c;
  }

private:
  static uint64_t MonotonicNs() {
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart) * 1000000000ULL /
      static_cast<uint64_t>(freq.QuadPart);
  }

  static std::wstring GenerateSessionId() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return L"SESSION-" +
      std::to_wstring(st.wYear) +
      (st.wMonth < 10 ? L"0" : L"") + std::to_wstring(st.wMonth) +
      (st.wDay < 10 ? L"0" : L"") + std::to_wstring(st.wDay) + L"-" +
      (st.wHour < 10 ? L"0" : L"") + std::to_wstring(st.wHour) +
      (st.wMinute < 10 ? L"0" : L"") + std::to_wstring(st.wMinute) +
      (st.wSecond < 10 ? L"0" : L"") + std::to_wstring(st.wSecond);
  }

  static std::wstring SignStr(int v) {
    return (v >= 0 ? L"+" : L"") + std::to_wstring(v);
  }

  static std::wstring SignStrF(double v) {
    wchar_t buf[32];
    swprintf(buf, 32, L"%+.1f", v);
    return buf;
  }

  void ComputeSummary() {
    if (!current_ || current_->snapshots.empty()) return;
    double cpuSum = 0, gpuSum = 0;
    uint64_t ramSum = 0, ramTotal = 0;
    for (const auto& s : current_->snapshots) {
      cpuSum += s.cpu.pct;
      if (s.gpu.pctValid) gpuSum += s.gpu.pct;
      ramSum += s.memory.usedBytes;
      ramTotal += s.memory.totalBytes;
    }
    int n = static_cast<int>(current_->snapshots.size());
    current_->avgCpu = cpuSum / n;
    current_->avgGpu = gpuSum / n;
    current_->avgRamPct = (ramTotal > 0) ?
      (static_cast<double>(ramSum) / static_cast<double>(ramTotal)) * 100.0 : 0.0;
  }

  std::unique_ptr<Session> current_;
  std::vector<SessionMetadata> history_;
  bool paused_ = false;
  mutable std::mutex mutex_;
};

}
