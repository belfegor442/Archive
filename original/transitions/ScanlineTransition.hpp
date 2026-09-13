#pragma once

#include "Transition.hpp"

namespace monix {

class ScanlineTransition : public Transition {
public:
  ScanlineTransition() = default;
  ~ScanlineTransition() override = default;

  TransitionType type() const override { return TransitionType::Scanline; }
  const char* name() const override { return "Scanline"; }

  void start(double durationMs, const TransitionConfig& config) override;
  bool update(double elapsedMs) override;
  void reset() override;

  bool isActive() const override { return active_; }
  float progress() const override { return progress_; }
  TransitionState gpuState() const override;

private:
  static float easeOutCubic(float t);
  static float smoothstep(float edge0, float edge1, float x);

  bool active_ = false;
  double durationMs_ = 220.0;
  double elapsedMs_ = 0.0;
  float progress_ = 0.0f;
  float time_ = 0.0f;

  float intensity_ = 0.8f;
  float scanlineWidth_ = 0.012f;
  float distortion_ = 0.002f;
  float noiseAmount_ = 0.015f;
  float flickerAmount_ = 0.025f;
};

}  // namespace monix
