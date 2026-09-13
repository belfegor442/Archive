#pragma once

#include "TransitionType.hpp"

#include <cstdint>
#include <functional>

namespace monix {

struct TransitionConfig {
  bool enabled = true;
  double durationMs = 220.0;
  double scanlineWidth = 0.012;
  double intensity = 0.8;
  double distortion = 0.002;
  double noiseAmount = 0.015;
  double flickerAmount = 0.025;
};

struct TransitionState {
  float progress = 0.0f;
  float intensity = 0.8f;
  float scanlineWidth = 0.012f;
  float distortion = 0.002f;
  float noiseAmount = 0.015f;
  float flickerAmount = 0.025f;
  float time = 0.0f;
  float _pad = 0.0f;
};

class Transition {
public:
  virtual ~Transition() = default;

  virtual TransitionType type() const = 0;
  virtual const char* name() const = 0;

  virtual void start(double durationMs, const TransitionConfig& config) = 0;
  virtual bool update(double elapsedMs) = 0;
  virtual void reset() = 0;

  virtual bool isActive() const = 0;
  virtual float progress() const = 0;
  virtual TransitionState gpuState() const = 0;
};

}  // namespace monix
