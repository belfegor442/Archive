#pragma once

#include "Transition.hpp"
#include "TransitionType.hpp"
#include "../core/Types.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace monix {

struct TransitionEvent {
  enum class Type { Started, Completed, Cancelled, Replaced };
  Type type;
  Tab fromTab;
  Tab toTab;
  TransitionType transitionType;
  double durationMs = 0.0;
};

class TransitionManager {
public:
  using EventCallback = std::function<void(const TransitionEvent&)>;

  TransitionManager();
  ~TransitionManager();

  void setEventCallback(EventCallback cb);

  bool requestTransition(Tab fromTab, Tab toTab,
                         TransitionType type = TransitionType::Scanline);

  bool update(double elapsedMs);

  void cancelCurrent();

  bool isTransitioning() const;
  float currentProgress() const;
  Tab pendingTargetTab() const;
  Tab sourceTab() const;
  TransitionType currentType() const;

  TransitionState gpuState() const;

  void setConfig(const TransitionConfig& config);
  const TransitionConfig& config() const { return config_; }

  bool enabled() const { return config_.enabled; }

private:
  Transition* createTransition(TransitionType type);

  TransitionConfig config_;
  std::vector<std::unique_ptr<Transition>> transitionPool_;

  Transition* activeTransition_ = nullptr;
  Tab fromTab_ = Tab::Log;
  Tab toTab_ = Tab::Log;
  bool hasPendingTarget_ = false;

  EventCallback eventCallback_;
};

}  // namespace monix
