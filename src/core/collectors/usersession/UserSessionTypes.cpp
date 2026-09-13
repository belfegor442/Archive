#include "UserSessionTypes.hpp"

#include <windows.h>
#include <wtsapi32.h>

namespace monix::collectors::usersession {

const char* SessionStateName(SessionState s) {
  switch (s) {
    case SessionState::Active:       return "Active";
    case SessionState::Connected:    return "Connected";
    case SessionState::Shadow:       return "Shadow";
    case SessionState::Disconnected: return "Disconnected";
    case SessionState::Unknown:      return "Unknown";
  }
  return "Unknown";
}

SessionState SessionStateFromWts(DWORD state) {
  switch (state) {
    case WTSActive:       return SessionState::Active;
    case WTSConnected:    return SessionState::Connected;
    case WTSShadow:       return SessionState::Shadow;
    case WTSDisconnected: return SessionState::Disconnected;
    default:              return SessionState::Unknown;
  }
}

const char* UserSessionEventKindName(UserSessionEventKind k) {
  switch (k) {
    case UserSessionEventKind::Login:          return "Login";
    case UserSessionEventKind::Logout:         return "Logout";
    case UserSessionEventKind::SessionCreated: return "SessionCreated";
    case UserSessionEventKind::SessionClosed:  return "SessionClosed";
    case UserSessionEventKind::UserAction:     return "UserAction";
  }
  return "Unknown";
}

std::string UserSessionEventKindAction(UserSessionEventKind k) {
  switch (k) {
    case UserSessionEventKind::Login:          return "login";
    case UserSessionEventKind::Logout:         return "logout";
    case UserSessionEventKind::SessionCreated: return "session_created";
    case UserSessionEventKind::SessionClosed:  return "session_closed";
    case UserSessionEventKind::UserAction:     return "user_action";
  }
  return "unknown";
}

const char* UserSessionOriginName(UserSessionOrigin o) {
  switch (o) {
    case UserSessionOrigin::InitialSnapshot: return "InitialSnapshot";
    case UserSessionOrigin::Polling:         return "Polling";
    case UserSessionOrigin::Manual:          return "Manual";
  }
  return "Unknown";
}

const char* UserActionKindName(UserActionKind k) {
  switch (k) {
    case UserActionKind::ProgramStarted:  return "ProgramStarted";
    case UserActionKind::DeviceConnected: return "DeviceConnected";
    case UserActionKind::ScriptExecuted:  return "ScriptExecuted";
    case UserActionKind::ConfigChanged:   return "ConfigChanged";
    case UserActionKind::Unknown:         return "Unknown";
  }
  return "Unknown";
}

bool SessionInfo::isActive() const {
  return state == SessionState::Active || state == SessionState::Connected;
}

bool SessionInfo::isValid() const {
  return session_id != kInvalidSession;
}

std::string SessionInfo::qualifiedUser() const {
  if (domain.empty()) return username;
  return domain + "\\" + username;
}

}  // namespace monix::collectors::usersession
