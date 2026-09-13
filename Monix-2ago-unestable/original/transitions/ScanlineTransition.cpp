#include "ScanlineTransition.hpp"

#include <algorithm>
#include <cmath>

namespace monix {

void ScanlineTransition::start(double durationMs, const TransitionConfig& config) {
  durationMs_ = std::max(1.0, durationMs);
  elapsedMs_ = 0.0;
  progress_ = 0.0f;
  time_ = 0.0f;
  active_ = true;

  intensity_ = static_cast<float>(config.intensity);
  scanlineWidth_ = static_cast<float>(config.scanlineWidth);
  distortion_ = static_cast<float>(config.distortion);
  noiseAmount_ = static_cast<float>(config.noiseAmount);
  flickerAmount_ = static_cast<float>(config.flickerAmount);
}

bool ScanlineTransition::update(double elapsedMs) {
  if (!active_) return false;

  elapsedMs_ += elapsedMs;
  time_ += static_cast<float>(elapsedMs) * 0.001f;

  double rawProgress = std::clamp(elapsedMs_ / durationMs_, 0.0, 1.0);
  progress_ = easeOutCubic(static_cast<float>(rawProgress));

  if (rawProgress >= 1.0) {
    progress_ = 1.0f;
    active_ = false;
    return true;
  }
  return false;
}

void ScanlineTransition::reset() {
  active_ = false;
  progress_ = 0.0f;
  elapsedMs_ = 0.0;
  time_ = 0.0f;
}

TransitionState ScanlineTransition::gpuState() const {
  TransitionState s{};
  s.progress = progress_;
  s.intensity = intensity_;
  s.scanlineWidth = scanlineWidth_;
  s.distortion = distortion_;
  s.noiseAmount = noiseAmount_;
  s.flickerAmount = flickerAmount_;
  s.time = time_;
  return s;
}

float ScanlineTransition::easeOutCubic(float t) {
  float inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}

float ScanlineTransition::smoothstep(float edge0, float edge1, float x) {
  float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace monix
