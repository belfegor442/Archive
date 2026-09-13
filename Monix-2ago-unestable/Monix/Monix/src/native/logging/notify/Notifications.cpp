#include "Notifications.hpp"

#include "../../MonixApp.hpp"

#include <windows.h>

using namespace monix;

void MonixApp::QueueNotification(const LogEntry& entry) {
  std::shared_lock<std::shared_mutex> lock(configMutex_);
  if (!config_.notificationsEnabled) {
    return;
  }

  if (entry.level != LogLevel::Error && entry.level != LogLevel::Critical) {
    return;
  }

  NotificationItem item;
  item.title = entry.domain + L" :: " + entry.severity;
  item.message = entry.message;
  item.color = entry.color;
  item.level = entry.level;
  item.eventId = entry.eventId;
  item.expiresAtMs = GetTickCount64() + config_.notificationDurationMs;
  {
    std::lock_guard<std::recursive_mutex> stateLock(stateMutex_);
    state_.notifState.items.insert(state_.notifState.items.begin(), std::move(item));
    if (static_cast<int>(state_.notifState.items.size()) > config_.notificationMaxStack) {
      state_.notifState.items.resize(config_.notificationMaxStack);
    }
  }
  lock.unlock();
  PlayAlertSound(entry.level);
}

void MonixApp::SetToast(const std::wstring& message) {
  std::lock_guard<std::recursive_mutex> lock(stateMutex_);
  state_.notifState.toastMessage = message;
  state_.notifState.toastUntilMs = GetTickCount64() + 2400;
}
