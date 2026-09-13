#pragma once

#include "LogEntry.hpp"
#include <filesystem>
#include <string>

class MonixApp;

namespace monix::logging {

std::filesystem::path ResolveLogFilePath(const MonixApp* app, const std::wstring& extension);
void CleanupLogFiles(MonixApp* app);
void FlushLogQueues(MonixApp* app, bool force);
void PushLog(MonixApp* app, std::wstring domain, std::wstring severity, std::wstring message,
             ColorRole color, std::wstring module, std::wstring service, std::wstring metadata);

}  // namespace monix::logging
