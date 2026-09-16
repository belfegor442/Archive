#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "../events/EventBus.hpp"
#include "../events/SystemEvent.hpp"
#include "Timeline.hpp"
#include "SessionManager.hpp"
#include "../storage/SessionPersistence.hpp"

namespace monix {

class TimelineBridge {
public:
  TimelineBridge(EventBus& bus, Timeline& timeline, SessionManager& sessions)
    : bus_(bus), timeline_(timeline), sessions_(sessions) {
    SubscribeAll();
  }

  void ConnectPersistence(std::unique_ptr<SessionPersistence> persist) {
    persist_ = std::move(persist);
  }

  void StartRecording() {
    recording_ = true;
  }

  void StopRecording() {
    recording_ = false;
  }

  bool IsRecording() const { return recording_; }

  Timeline& GetTimeline() { return timeline_; }
  const Timeline& GetTimeline() const { return timeline_; }

  SessionManager& GetSessionManager() { return sessions_; }
  const SessionManager& GetSessionManager() const { return sessions_; }

  void OnSnapshot(uint64_t tsNs, uint64_t snapshotId) {
    std::wstring label = L"Snapshot #" + std::to_wstring(snapshotId);
    timeline_.AddSnapshot(tsNs, snapshotId, label);
    if (sessions_.HasActiveSession()) {
      sessions_.RecordTimeline(Timeline::Entry(timeline_.TotalEntries() - 1));
    }
  }

  void OnFinding(uint64_t findingId, const std::wstring& headline,
                  int riskScore, uint64_t tsNs) {
    timeline_.AddFinding(findingId, headline, riskScore, tsNs);
    if (sessions_.HasActiveSession()) {
      sessions_.RecordTimeline(Timeline::Entry(timeline_.TotalEntries() - 1));
    }
  }

  void OnUserAction(const std::wstring& action, uint64_t tsNs) {
    timeline_.AddUserAction(action, tsNs);
    if (sessions_.HasActiveSession()) {
      sessions_.RecordTimeline(Timeline::Entry(timeline_.TotalEntries() - 1));
    }
  }

  void OnDiagnostic(const std::wstring& category,
                     const std::wstring& summary,
                     int severity, uint64_t tsNs) {
    timeline_.AddDiagnostic(category, summary, severity, tsNs);
    if (sessions_.HasActiveSession()) {
      sessions_.RecordTimeline(Timeline::Entry(timeline_.TotalEntries() - 1));
    }
  }

  Session& StartSession(const std::wstring& name = L"") {
    return sessions_.StartSession(name);
  }

  void StopSession() {
    if (persist_ && sessions_.CurrentSession()) {
      persist_->SaveSession(*sessions_.CurrentSession());
    }
    sessions_.StopSession();
  }

  void ExportSession(const Session& session, const std::wstring& path) {
    if (persist_) {
      persist_->ExportSession(session, path);
    }
  }

  std::vector<SessionMetadata> ListSessions() {
    return persist_ ? persist_->ListSessions() : std::vector<SessionMetadata>{};
  }

  SessionSaveData LoadSession(const std::wstring& sessionId) {
    return persist_ ? persist_->LoadSession(sessionId) : SessionSaveData{};
  }

private:
  void SubscribeAll() {
    bus_.SubscribeAll([this](const SystemEvent& e) {
      if (!recording_) return;

      timeline_.AddEvent(e);

      if (sessions_.HasActiveSession()) {
        sessions_.RecordEvent(e);

        SystemEvent eCopy = e;
        eCopy.sessionEventIndex = sessions_.CurrentSession()->metadata.totalEvents;
        sessions_.RecordTimeline(Timeline::Entry(timeline_.TotalEntries() - 1));
      }
    });
  }

  EventBus& bus_;
  Timeline& timeline_;
  SessionManager& sessions_;
  std::unique_ptr<SessionPersistence> persist_;
  bool recording_ = false;
};

}
