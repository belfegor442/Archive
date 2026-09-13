#pragma once

#include "../LogEntry.hpp"
#include <string>

class MonixApp;

namespace monix::notify {

void QueueNotification(MonixApp* app, const LogEntry& entry);
void SetToast(MonixApp* app, const std::wstring& message);

}  // namespace monix::notify
