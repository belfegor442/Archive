#pragma once

#include "../ui/AppState.hpp"
#include "../logging/LogEntry.hpp"

#include <functional>
#include <mutex>
#include <vector>

namespace monix {

struct NotificationQueueConfig {
  bool enabled = true;
  int durationMs = 4200;
  int maxStack = 1;
};

class NotificationQueue {
 public:
  explicit NotificationQueue(const NotificationQueueConfig& config);

  void Queue(const LogEntry& entry);
  void Tick();  // Remove expired notifications
  void Clear();

  std::vector<NotificationItem> Items() const { std::lock_guard<std::mutex> lock(mutex_); return items_; }

  // Toast support
  void SetToast(const std::wstring& message);
  void TickToast();
  bool HasToast() const;
  std::wstring ToastMessage() const { std::lock_guard<std::mutex> lock(mutex_); return toastMessage_; }
  ULONGLONG ToastUntilMs() const { std::lock_guard<std::mutex> lock(mutex_); return toastUntilMs_; }

  // Callback for alert sounds
  std::function<void(int level)> onAlertSound;

 private:
  mutable std::mutex mutex_;
  NotificationQueueConfig config_;
  std::vector<NotificationItem> items_;
  std::wstring toastMessage_;
  ULONGLONG toastUntilMs_ = 0;
};

} // namespace monix
