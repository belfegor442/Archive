#include "TransitionManager.hpp"
#include "ScanlineTransition.hpp"

#include <algorithm>

namespace monix {

TransitionManager::TransitionManager() {
  transitionPool_.emplace_back(std::make_unique<ScanlineTransition>());
}

TransitionManager::~TransitionManager() = default;

void TransitionManager::setEventCallback(EventCallback cb) {
  eventCallback_ = std::move(cb);
}

Transition* TransitionManager::createTransition(TransitionType type) {
  for (auto& t : transitionPool_) {
    if (t->type() == type && t.get() != activeTransition_) {
      return t.get();
    }
  }
  if (type == TransitionType::Scanline) {
    transitionPool_.emplace_back(std::make_unique<ScanlineTransition>());
    return transitionPool_.back().get();
  }
  return nullptr;
}

bool TransitionManager::requestTransition(Tab fromTab, Tab toTab,
                                          TransitionType type) {
  if (!config_.enabled || type == TransitionType::None) {
    return false;
  }
  if (fromTab == toTab) {
    return false;
  }

  if (activeTransition_ && activeTransition_->isActive()) {
    TransitionEvent cancelEvent{};
    cancelEvent.type = TransitionEvent::Type::Cancelled;
    cancelEvent.fromTab = fromTab_;
    cancelEvent.toTab = toTab_;
    cancelEvent.transitionType = activeTransition_->type();
    cancelEvent.durationMs = config_.durationMs;
    activeTransition_->reset();

    if (eventCallback_) {
      eventCallback_(cancelEvent);
    }
  }

  Transition* tr = createTransition(type);
  if (!tr) {
    return false;
  }

  fromTab_ = fromTab;
  toTab_ = toTab;
  activeTransition_ = tr;
  hasPendingTarget_ = true;

  activeTransition_->start(config_.durationMs, config_);

  TransitionEvent startEvent{};
  startEvent.type = TransitionEvent::Type::Started;
  startEvent.fromTab = fromTab;
  startEvent.toTab = toTab;
  startEvent.transitionType = type;
  startEvent.durationMs = config_.durationMs;

  if (eventCallback_) {
    eventCallback_(startEvent);
  }

  return true;
}

bool TransitionManager::update(double elapsedMs) {
  if (!activeTransition_ || !activeTransition_->isActive()) {
    return false;
  }

  bool completed = activeTransition_->update(elapsedMs);

  if (completed) {
    TransitionEvent completeEvent{};
    completeEvent.type = TransitionEvent::Type::Completed;
    completeEvent.fromTab = fromTab_;
    completeEvent.toTab = toTab_;
    completeEvent.transitionType = activeTransition_->type();
    completeEvent.durationMs = config_.durationMs;

    if (eventCallback_) {
      eventCallback_(completeEvent);
    }

    activeTransition_->reset();
    activeTransition_ = nullptr;
    hasPendingTarget_ = false;
    return true;
  }
  return false;
}

void TransitionManager::cancelCurrent() {
  if (activeTransition_ && activeTransition_->isActive()) {
    TransitionEvent cancelEvent{};
    cancelEvent.type = TransitionEvent::Type::Cancelled;
    cancelEvent.fromTab = fromTab_;
    cancelEvent.toTab = toTab_;
    cancelEvent.transitionType = activeTransition_->type();

    activeTransition_->reset();

    if (eventCallback_) {
      eventCallback_(cancelEvent);
    }

    activeTransition_ = nullptr;
    hasPendingTarget_ = false;
  }
}

bool TransitionManager::isTransitioning() const {
  return activeTransition_ && activeTransition_->isActive();
}

float TransitionManager::currentProgress() const {
  if (activeTransition_ && activeTransition_->isActive()) {
    return activeTransition_->progress();
  }
  return 1.0f;
}

Tab TransitionManager::pendingTargetTab() const {
  if (hasPendingTarget_) {
    return toTab_;
  }
  return Tab::Log;
}

Tab TransitionManager::sourceTab() const {
  return fromTab_;
}

TransitionType TransitionManager::currentType() const {
  if (activeTransition_) {
    return activeTransition_->type();
  }
  return TransitionType::None;
}

TransitionState TransitionManager::gpuState() const {
  if (activeTransition_ && activeTransition_->isActive()) {
    return activeTransition_->gpuState();
  }
  TransitionState s{};
  s.progress = 1.0f;
  return s;
}

void TransitionManager::setConfig(const TransitionConfig& config) {
  config_ = config;
}

}  // namespace monix
