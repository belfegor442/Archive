#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>

#include "../core/Types.hpp"
#include "../settings/MonixConfigTypes.hpp"

namespace monix {

struct IntroState {
  bool active = true;
  bool soundPlayed = false;
  int logoY = -9999;
  int creditOffset = 80;
  ULONGLONG settledAtMs = 0;
};

struct ContextMenuState {
  bool visible = false;
  int processIndex = -1;
  int hoverIndex = -1;
  RECT rect { 0, 0, 0, 0 };
};

struct NotificationItem {
  std::wstring title;
  std::wstring message;
  ColorRole color = ColorRole::Primary;
  LogLevel level = LogLevel::Info;
  std::uint64_t eventId = 0;
  ULONGLONG expiresAtMs = 0;
};

struct NativeProcessSample {
  std::uint64_t cpuTime = 0;
  std::uint64_t createTime = 0;
  std::uint64_t readBytes = 0;
  std::uint64_t writeBytes = 0;
};

}
