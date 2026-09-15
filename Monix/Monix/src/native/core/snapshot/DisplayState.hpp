#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace monix {

struct DisplayState {
  int refreshRateHz = 0;
  int width = 0;
  int height = 0;
  int bitsPerPel = 0;
  int monitorCount = 0;
  int hdrEnabled = 0;
  int vsyncEnabled = 0;
  int desktopCompositionEnabled = 1;
  std::set<std::wstring> displayNames;
};

}
