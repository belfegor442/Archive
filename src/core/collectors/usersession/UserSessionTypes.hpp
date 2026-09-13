#pragma once

#include <cstdint>
#include <string>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace monix::collectors::usersession {

using SessionId = std::uint32_t;
using UserId = std::uint32_t;

constexpr SessionId kInvalidSession = 0xFFFFFFFF;
constexpr UserId kUnknownUser = 0;

enum class SessionState : std::uint8_t {
  Active,
  Connected,
  Shadow,
  Disconnected,
  Unknown
};

const char* SessionStateName(SessionState s);
SessionState SessionStateFromWts(DWORD state);

enum class UserSessionEventKind : std::uint8_t {
  Login,
  Logout,
  SessionCreated,
  SessionClosed,
  UserAction
};

const char* UserSessionEventKindName(UserSessionEventKind k);
std::string UserSessionEventKindAction(UserSessionEventKind k);

enum class UserSessionOrigin : std::uint8_t {
  InitialSnapshot,
  Polling,
  Manual
};

const char* UserSessionOriginName(UserSessionOrigin o);

enum class UserActionKind : std::uint8_t {
  ProgramStarted,
  DeviceConnected,
  ScriptExecuted,
  ConfigChanged,
  Unknown
};

const char* UserActionKindName(UserActionKind k);

struct SessionInfo {
  SessionId session_id = kInvalidSession;
  UserId user_id = kUnknownUser;
  std::string username;
  std::string domain;
  SessionState state = SessionState::Unknown;
  std::string client_name;
  std::string client_address;
  std::int64_t connect_time_ms = 0;
  std::int64_t disconnect_time_ms = 0;

  bool isActive() const;
  bool isValid() const;
  std::string qualifiedUser() const;
};

struct UserAction {
  UserActionKind kind = UserActionKind::Unknown;
  UserId user_id = kUnknownUser;
  SessionId session_id = kInvalidSession;
  std::string description;
  std::int64_t timestamp_ms = 0;
};

}  // namespace monix::collectors::usersession
