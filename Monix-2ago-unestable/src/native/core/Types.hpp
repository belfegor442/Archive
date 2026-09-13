#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace monix {

enum class Tab {
  Log,
  Tasks,
  Hardware,
  Network,
  Scram,
  Settings
};

enum class ColorRole {
  Black,
  Primary,
  White,
  Success,
  Warning,
  Error,
  Network,
  Scram,
  Kernel,
  Dim,
  Accent,
  Inverse,
  Idle,
  Engine,
  Security,
  HealthScore,
  Storage,
  CriticalScram,
  UserInput,
  Fatal,
  Thermal
};

enum class LogFilter {
  All,
  Warn,
  Net,
  Crit,
  Err,
  Kernel
};

struct LogButtonRect {
  LogFilter filter = LogFilter::All;
  RECT rect { 0, 0, 0, 0 };
};

struct LogToolbarRect {
  RECT clear { 0, 0, 0, 0 };
  RECT pause { 0, 0, 0, 0 };
  RECT search { 0, 0, 0, 0 };
  RECT json { 0, 0, 0, 0 };
  RECT csv { 0, 0, 0, 0 };
  RECT copy { 0, 0, 0, 0 };
};

COLORREF ResolveColor(ColorRole role);
COLORREF ResolveGlowColor(ColorRole role);
void FillSolid(HDC dc, const RECT& rect, COLORREF color);
void DrawRectOutline(HDC dc, const RECT& rect, COLORREF color);
RECT ShrinkRect(RECT rect, int amount);

}
