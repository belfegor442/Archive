#pragma once

#include <cstdint>

namespace monix {

enum class TransitionType : uint8_t {
  None = 0,
  Scanline = 1,
  Glitch = 2,
  CRTPowerOn = 3,
  CRTPowerOff = 4,
  Fade = 5
};

inline const char* TransitionTypeName(TransitionType t) {
  switch (t) {
    case TransitionType::None:         return "None";
    case TransitionType::Scanline:     return "Scanline";
    case TransitionType::Glitch:       return "Glitch";
    case TransitionType::CRTPowerOn:   return "CRTPowerOn";
    case TransitionType::CRTPowerOff:  return "CRTPowerOff";
    case TransitionType::Fade:         return "Fade";
    default:                           return "Unknown";
  }
}

}  // namespace monix
