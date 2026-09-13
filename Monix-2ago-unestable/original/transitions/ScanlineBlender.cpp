#include "ScanlineBlender.hpp"

#include <algorithm>
#include <cmath>

namespace monix {

void ScanlineBlender::blend(void* outputPixels,
                            const void* oldFramePixels,
                            const void* newFramePixels,
                            uint32_t width, uint32_t height,
                            uint32_t stride,
                            const TransitionState& state) {
  if (!outputPixels || !oldFramePixels || !newFramePixels) return;
  if (width == 0 || height == 0) return;

  const float progress = std::clamp(state.progress, 0.0f, 1.0f);
  const float scanY = progress;
  const float scanW = state.scanlineWidth;
  const float intensity = state.intensity;
  const float distortion = state.distortion;
  const float noiseAmt = state.noiseAmount;
  const float flickerAmt = state.flickerAmount;
  const float time = state.time;

  const float flicker = 1.0f + std::sin(time * 120.0f) * flickerAmt;

  auto* out = static_cast<uint8_t*>(outputPixels);
  const auto* oldF = static_cast<const uint8_t*>(oldFramePixels);
  const auto* newF = static_cast<const uint8_t*>(newFramePixels);

  for (uint32_t y = 0; y < height; ++y) {
    const float uvY = static_cast<float>(y) / static_cast<float>(height);
    const float distToScan = std::abs(uvY - scanY);

    const float edge = smoothstep(scanY - scanW, scanY + scanW, uvY);

    const float distortStrength = smoothstep(scanW * 3.0f, 0.0f, distToScan);
    const int32_t distortOffset = static_cast<int32_t>(
      distortStrength * distortion * width * std::sin(uvY * 80.0f + time * 10.0f));

    const float scanBright = std::exp(-distToScan * 80.0f) * intensity * 0.5f * flicker;
    const float edgeGlow = std::exp(-distToScan * 40.0f) * intensity * 0.3f * flicker;

    const uint32_t rowOffset = y * stride;

    for (uint32_t x = 0; x < width; ++x) {
      const uint32_t px = rowOffset + x * 4;

      const int32_t sampleX = std::clamp(static_cast<int32_t>(x) + distortOffset,
                                         0, static_cast<int32_t>(width) - 1);
      const uint32_t samplePx = rowOffset + sampleX * 4;

      const float noise = (pseudoRandom(static_cast<int32_t>(x * 1000 + y + time * 1000)) - 0.5f) * noiseAmt;

      for (int c = 0; c < 3; ++c) {
        float o = oldF[samplePx + c] / 255.0f;
        float n = newF[samplePx + c] / 255.0f;
        float val = n + (o - n) * edge;
        val += scanBright * 0.6f;
        val += edgeGlow * 0.4f;
        val += noise;
        val *= flicker;
        out[px + c] = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
      }
      out[px + 3] = 255;
    }
  }
}

float ScanlineBlender::smoothstep(float edge0, float edge1, float x) {
  float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float ScanlineBlender::pseudoRandom(int32_t seed) {
  uint32_t s = static_cast<uint32_t>(seed);
  s = (s * 1664525u + 1013904223u);
  return static_cast<float>(s & 0x00FFFFFF) / static_cast<float>(0x01000000);
}

}  // namespace monix
