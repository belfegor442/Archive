#pragma once

#include "UserSessionTypes.hpp"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace monix::collectors::usersession {

using UserSessionCallback = std::function<void(
  UserSessionEventKind, UserSessionOrigin, const SessionInfo&)>;

using UserActionCallback = std::function<void(
  UserActionKind, UserId, SessionId, const std::string&)>;

struct UserSessionCollectorConfig {
  std::uint32_t poll_interval_ms = 2000;
  bool track_user_actions = false;
  std::size_t max_sessions = 100;

  static UserSessionCollectorConfig defaults();
};

class UserSessionCollector {
public:
  UserSessionCollector();
  ~UserSessionCollector();

  UserSessionCollector(const UserSessionCollector&) = delete;
  UserSessionCollector& operator=(const UserSessionCollector&) = delete;

  bool start(UserSessionCollectorConfig config = UserSessionCollectorConfig::defaults());
  bool stop();
  bool isRunning() const;

  void setCallback(UserSessionCallback callback);
  void setUserActionCallback(UserActionCallback callback);

  std::vector<SessionInfo> currentSessions() const;
  std::size_t sessionCount() const;
  std::size_t eventsEmitted() const;
  std::size_t initialSnapshotCount() const;
  bool wasInInitialSnapshot(SessionId id) const;

  void reportUserAction(UserActionKind kind, const std::string& description);

  static SessionId currentSessionId();
  static UserId currentUserSid();
  static std::string currentUsername();

private:
  void pollLoop();
  void emitEvent(UserSessionEventKind kind, UserSessionOrigin origin, const SessionInfo& info);
  void reconcile(const std::vector<SessionInfo>& current, UserSessionOrigin origin);
  bool enumerateSessions(std::vector<SessionInfo>& out);

  UserSessionCollectorConfig config_;
  UserSessionCallback callback_;
  UserActionCallback action_callback_;
  std::unordered_map<SessionId, SessionInfo> tracked_;
  std::unordered_set<SessionId> initial_snapshot_ids_;

  mutable std::mutex mu_;
  mutable std::mutex callback_mu_;
  std::thread poll_thread_;
  std::atomic<bool> running_{false};
  bool stopping_{false};
  std::condition_variable stop_cv_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::usersession
