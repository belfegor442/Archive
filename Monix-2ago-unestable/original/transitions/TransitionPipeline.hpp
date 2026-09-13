#pragma once

#include "Transition.hpp"

#include <cstdint>

namespace monix {

class TransitionPipeline {
public:
  TransitionPipeline() = default;
  ~TransitionPipeline() = default;

  TransitionPipeline(const TransitionPipeline&) = delete;
  TransitionPipeline& operator=(const TransitionPipeline&) = delete;

  bool isInitialized() const { return initialized_; }
  void markInitialized() { initialized_ = true; }

private:
  bool initialized_ = false;
};

}  // namespace monix
