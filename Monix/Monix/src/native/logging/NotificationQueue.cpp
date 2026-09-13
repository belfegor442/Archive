#include "NotificationQueue.hpp"

#include <algorithm>

namespace monix {

NotificationQueue::NotificationQueue(const NotificationQueueConfig& config)
    : config_(config) {}

void NotificationQueue::Queue(const LogEntry& entry) {
  bool shouldPlaySound = false;
  int soundLevel = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!config_.enabled) return;
    if (entry.level != LogLevel::Error && entry.level != LogLevel::Critical) return;

    NotificationItem item;
    item.title = entry.domain + L" :: " + entry.severity;
    item.message = entry.message;
    item.color = entry.color;
    item.level = entry.level;
    item.eventId = entry.eventId;
    item.expiresAtMs = GetTickCount64() + config_.durationMs;
    items_.insert(items_.begin(), std::move(item));
    if (static_cast<int>(items_.size()) > config_.maxStack) {
      items_.resize(config_.maxStack);
    }
    if (onAlertSound) {
      shouldPlaySound = true;
      soundLevel = static_cast<int>(entry.level);
    }
  }
  if (shouldPlaySound && onAlertSound) {
    onAlertSound(soundLevel);
  }
}

void NotificationQueue::Tick() {
  std::lock_guard<std::mutex> lock(mutex_);
  const ULONGLONG now = GetTickCount64();
  items_.erase(
    std::remove_if(items_.begin(), items_.end(),
                   [now](const NotificationItem& item) {
                     return now >= item.expiresAtMs;
                   }),
    items_.end());
}

void NotificationQueue::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  items_.clear();
}

void NotificationQueue::SetToast(const std::wstring& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  toastMessage_ = message;
  toastUntilMs_ = GetTickCount64() + 2400;
}

void NotificationQueue::TickToast() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!toastMessage_.empty() && GetTickCount64() >= toastUntilMs_) {
    toastMessage_.clear();
    toastUntilMs_ = 0;
  }
}

bool NotificationQueue::HasToast() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !toastMessage_.empty() && GetTickCount64() < toastUntilMs_;
}

} // namespace monix
