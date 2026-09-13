#include "UserSessionCollector.hpp"

#include <chrono>
#include <algorithm>

#include <windows.h>
#include <wtsapi32.h>
#include <sddl.h>
#include <securitybaseapi.h>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace monix::collectors::usersession {

UserSessionCollectorConfig UserSessionCollectorConfig::defaults() {
  UserSessionCollectorConfig cfg;
  cfg.poll_interval_ms = 2000;
  cfg.track_user_actions = false;
  cfg.max_sessions = 100;
  return cfg;
}

UserSessionCollector::UserSessionCollector() = default;

UserSessionCollector::~UserSessionCollector() {
  stop();
}

bool UserSessionCollector::start(UserSessionCollectorConfig config) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) return false;
    config_ = std::move(config);
    running_ = true;
    stopping_ = false;
    events_emitted_ = 0;
    tracked_.clear();
    initial_snapshot_ids_.clear();
  }

  std::vector<SessionInfo> initial;
  if (enumerateSessions(initial)) {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& s : initial) {
      tracked_[s.session_id] = s;
      initial_snapshot_ids_.insert(s.session_id);
    }
  }

  poll_thread_ = std::thread(&UserSessionCollector::pollLoop, this);
  return true;
}

bool UserSessionCollector::stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (!running_) return false;
    stopping_ = true;
  }
  running_ = false;
  stop_cv_.notify_all();

  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_.clear();
    initial_snapshot_ids_.clear();
  }
  return true;
}

bool UserSessionCollector::isRunning() const {
  return running_;
}

void UserSessionCollector::setCallback(UserSessionCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mu_);
  callback_ = std::move(callback);
}

void UserSessionCollector::setUserActionCallback(UserActionCallback callback) {
  std::lock_guard<std::mutex> lock(callback_mu_);
  action_callback_ = std::move(callback);
}

std::vector<SessionInfo> UserSessionCollector::currentSessions() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<SessionInfo> result;
  for (const auto& [id, info] : tracked_) {
    result.push_back(info);
  }
  return result;
}

std::size_t UserSessionCollector::sessionCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tracked_.size();
}

std::size_t UserSessionCollector::eventsEmitted() const {
  return events_emitted_.load(std::memory_order_relaxed);
}

std::size_t UserSessionCollector::initialSnapshotCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return initial_snapshot_ids_.size();
}

bool UserSessionCollector::wasInInitialSnapshot(SessionId id) const {
  std::lock_guard<std::mutex> lock(mu_);
  return initial_snapshot_ids_.find(id) != initial_snapshot_ids_.end();
}

void UserSessionCollector::reportUserAction(UserActionKind kind, const std::string& description) {
  UserActionCallback callback;
  {
    std::lock_guard<std::mutex> lock(callback_mu_);
    callback = action_callback_;
  }
  if (!callback) return;

  SessionId sid = currentSessionId();
  UserId uid = currentUserSid();

  callback(kind, uid, sid, description);
}

SessionId UserSessionCollector::currentSessionId() {
  return static_cast<SessionId>(WTSGetActiveConsoleSessionId());
}

UserId UserSessionCollector::currentUserSid() {
  HANDLE hToken = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return kUnknownUser;

  DWORD len = 0;
  GetTokenInformation(hToken, TokenUser, nullptr, 0, &len);
  if (len == 0) { CloseHandle(hToken); return kUnknownUser; }

  std::vector<BYTE> buf(len);
  if (!GetTokenInformation(hToken, TokenUser, buf.data(), len, &len)) {
    CloseHandle(hToken);
    return kUnknownUser;
  }

  CloseHandle(hToken);
  auto* tu = reinterpret_cast<TOKEN_USER*>(buf.data());
  PSID sid = tu->User.Sid;
  DWORD sidLen = GetLengthSid(sid);

  UserId result = 0;
  if (sidLen >= 8) {
    auto* countByte = reinterpret_cast<BYTE*>(sid) + 1;
    DWORD count = *countByte;
    if (count > 0) {
      auto* subAuth = reinterpret_cast<DWORD*>(
        reinterpret_cast<BYTE*>(sid) + 8 + (count - 1) * sizeof(DWORD));
      result = *subAuth;
    }
  }
  return result;
}

std::string UserSessionCollector::currentUsername() {
  wchar_t name[256]{};
  DWORD size = 256;
  if (GetUserNameW(name, &size)) {
    std::string result;
    for (wchar_t c : name) {
      if (c == L'\0') break;
      result += static_cast<char>(c & 0xFF);
    }
    return result;
  }
  return "unknown";
}

void UserSessionCollector::pollLoop() {
  bool firstPoll = true;
  while (running_) {
    std::vector<SessionInfo> current;
    bool ok = false;

    ok = enumerateSessions(current);

    if (ok) {
      std::lock_guard<std::mutex> lock(mu_);
      UserSessionOrigin origin = firstPoll
        ? UserSessionOrigin::InitialSnapshot
        : UserSessionOrigin::Polling;
      reconcile(current, origin);
      firstPoll = false;
    }

    std::unique_lock<std::mutex> lock(mu_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(config_.poll_interval_ms),
                      [this] { return stopping_; });
  }
}

void UserSessionCollector::emitEvent(UserSessionEventKind kind, UserSessionOrigin origin, const SessionInfo& info) {
  events_emitted_.fetch_add(1, std::memory_order_relaxed);
  UserSessionCallback callback;
  {
    std::lock_guard<std::mutex> lock(callback_mu_);
    callback = callback_;
  }
  if (callback) callback(kind, origin, info);
}

void UserSessionCollector::reconcile(const std::vector<SessionInfo>& current, UserSessionOrigin origin) {
  std::unordered_set<SessionId> currentIds;

  for (const auto& s : current) {
    currentIds.insert(s.session_id);
    auto it = tracked_.find(s.session_id);

    if (it == tracked_.end()) {
      if (tracked_.size() < config_.max_sessions) {
        tracked_[s.session_id] = s;
        if (origin == UserSessionOrigin::InitialSnapshot) {
          initial_snapshot_ids_.insert(s.session_id);
        } else {
          emitEvent(UserSessionEventKind::SessionCreated, origin, s);
          if (s.isActive()) {
            emitEvent(UserSessionEventKind::Login, origin, s);
          }
        }
      }
    } else {
      SessionState oldState = it->second.state;
      SessionState newState = s.state;

      if (oldState != newState) {
        it->second.state = newState;
        it->second.client_name = s.client_name;
        it->second.client_address = s.client_address;

        if (oldState != SessionState::Active && oldState != SessionState::Connected &&
            (newState == SessionState::Active || newState == SessionState::Connected)) {
          emitEvent(UserSessionEventKind::Login, origin, s);
        } else if ((oldState == SessionState::Active || oldState == SessionState::Connected) &&
                   newState == SessionState::Disconnected) {
          emitEvent(UserSessionEventKind::Logout, origin, s);
        }
      }
    }
  }

  auto it = tracked_.begin();
  while (it != tracked_.end()) {
    if (currentIds.find(it->first) == currentIds.end()) {
      SessionInfo closed = it->second;
      closed.state = SessionState::Disconnected;
      emitEvent(UserSessionEventKind::SessionClosed, origin, closed);
      initial_snapshot_ids_.erase(it->first);
      it = tracked_.erase(it);
    } else {
      ++it;
    }
  }
}

bool UserSessionCollector::enumerateSessions(std::vector<SessionInfo>& out) {
  PWTS_SESSION_INFOW sessionInfo = nullptr;
  DWORD count = 0;

  if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessionInfo, &count)) {
    return false;
  }

  for (DWORD i = 0; i < count; i++) {
    SessionInfo info;
    info.session_id = static_cast<SessionId>(sessionInfo[i].SessionId);
    info.state = SessionStateFromWts(sessionInfo[i].State);

    if (sessionInfo[i].pWinStationName) {
      int sz = WideCharToMultiByte(CP_UTF8, 0, sessionInfo[i].pWinStationName, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) {
        info.client_name.resize(static_cast<std::size_t>(sz) - 1);
        WideCharToMultiByte(CP_UTF8, 0, sessionInfo[i].pWinStationName, -1, info.client_name.data(), sz, nullptr, nullptr);
      }
    }

    PWTS_SESSION_INFOW si = &sessionInfo[i];
    DWORD userNameLen = 0;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, si->SessionId,
                                     WTSUserName, nullptr, &userNameLen) && userNameLen > 0) {
      wchar_t* userName = nullptr;
      if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, si->SessionId,
                                       WTSUserName, &userName, &userNameLen)) {
        if (userName) {
          int sz = WideCharToMultiByte(CP_UTF8, 0, userName, -1, nullptr, 0, nullptr, nullptr);
          if (sz > 0) {
            info.username.resize(static_cast<std::size_t>(sz) - 1);
            WideCharToMultiByte(CP_UTF8, 0, userName, -1, info.username.data(), sz, nullptr, nullptr);
          }
        }
        WTSFreeMemory(userName);
      }
    }

    DWORD domainNameLen = 0;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, si->SessionId,
                                     WTSDomainName, nullptr, &domainNameLen) && domainNameLen > 0) {
      wchar_t* domainName = nullptr;
      if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, si->SessionId,
                                       WTSDomainName, &domainName, &domainNameLen)) {
        if (domainName) {
          int sz = WideCharToMultiByte(CP_UTF8, 0, domainName, -1, nullptr, 0, nullptr, nullptr);
          if (sz > 0) {
            info.domain.resize(static_cast<std::size_t>(sz) - 1);
            WideCharToMultiByte(CP_UTF8, 0, domainName, -1, info.domain.data(), sz, nullptr, nullptr);
          }
        }
        WTSFreeMemory(domainName);
      }
    }

    DWORD clientAddrLen = 0;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, si->SessionId,
                                     WTSClientAddress, nullptr, &clientAddrLen) && clientAddrLen > 0) {
      LPWSTR clientAddrRaw = nullptr;
      if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, si->SessionId,
                                       WTSClientAddress, &clientAddrRaw, &clientAddrLen)) {
        auto* clientAddr = reinterpret_cast<BYTE*>(clientAddrRaw);
        auto* addr = reinterpret_cast<WTS_CLIENT_ADDRESS*>(clientAddr);
        if (addr->AddressFamily == 2) {
          char ip[32]{};
          snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
            static_cast<int>(addr->Address[2]), static_cast<int>(addr->Address[3]),
            static_cast<int>(addr->Address[4]), static_cast<int>(addr->Address[5]));
          info.client_address = ip;
        }
        WTSFreeMemory(clientAddrRaw);
      }
    }

    out.push_back(std::move(info));
  }

  if (sessionInfo) {
    WTSFreeMemory(sessionInfo);
  }

  return true;
}

}  // namespace monix::collectors::usersession
