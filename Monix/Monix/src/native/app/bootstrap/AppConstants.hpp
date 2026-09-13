#pragma once

#include <windows.h>

namespace monix {

constexpr UINT_PTR kFrameTimerId = 1;
constexpr UINT WM_MONIX_UPDATE = WM_APP + 1;
constexpr int kMainIconResourceId = 101;

} // namespace monix

constexpr int kWindowMinWidth = 1320;
constexpr int kWindowMinHeight = 840;
constexpr int kTaskMenuWidth = 320;
constexpr int kTaskMenuItemHeight = 38;
constexpr int kNotificationWidth = 370;
constexpr int kNotificationHeight = 82;
