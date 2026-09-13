#pragma once

#include "Transition.hpp"

#include <cstdint>

namespace monix {

class ScanlineBlender {
public:
  static void blend(void* outputPixels,
                    const void* oldFramePixels,
                    const void* newFramePixels,
                    uint32_t width, uint32_t height,
                    uint32_t stride,
                    const TransitionState& state);

private:
  static float smoothstep(float edge0, float edge1, float x);
  static float pseudoRandom(int32_t seed);
};

}  // namespace monix
