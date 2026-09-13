#include "LogExport.hpp"

#include "../../MonixApp.hpp"
#include "../../core/TextUtils.hpp"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

using namespace monix;

void MonixApp::ExportLogsJson() const {
  std::filesystem::create_directories(paths_.exportsDir);
  const auto path = paths_.exportsDir / (state_.session.id + L"-logs.json");
  std::lock_guard<std::recursive_mutex> lock(stateMutex_);
  std::wofstream output(path);
  output << L"[\n";
  for (std::size_t i = 0; i < state_.logState.entries.size(); ++i) {
    const auto& entry = state_.logState.entries[i];
    output << L"  {\"timestamp\":\"" << EscapeJson(entry.fullTimestamp)
           << L"\",\"level\":\"" << EscapeJson(entry.severity)
           << L"\",\"domain\":\"" << EscapeJson(entry.domain)
           << L"\",\"module\":\"" << EscapeJson(entry.module)
           << L"\",\"service\":\"" << EscapeJson(entry.service)
           << L"\",\"message\":\"" << EscapeJson(entry.message)
           << L"\",\"metadata\":\"" << EscapeJson(entry.metadata)
           << L"\",\"session_id\":\"" << EscapeJson(entry.sessionId)
           << L"\",\"user_id\":\"" << EscapeJson(entry.userId)
           << L"\",\"event_id\":" << entry.eventId
           << L",\"process_id\":" << entry.processId
           << L",\"thread_id\":" << entry.threadId
           << L",\"repeat_count\":" << entry.repeatCount
           << L"}" << (i + 1 == state_.logState.entries.size() ? L"\n" : L",\n");
  }
  output << L"]\n";
}

void MonixApp::ExportLogsCsv() const {
  std::filesystem::create_directories(paths_.exportsDir);
  const auto path = paths_.exportsDir / (state_.session.id + L"-logs.csv");
  std::lock_guard<std::recursive_mutex> lock(stateMutex_);
  std::wofstream output(path);
  output << L"timestamp,level,domain,module,service,event_id,process_id,thread_id,repeat_count,message,metadata\n";
  for (const auto& entry : state_.logState.entries) {
    auto csvEscape = [](std::wstring s) -> std::wstring {
      std::wstring result = L"\"";
      for (wchar_t c : s) {
        if (c == L'"') result += L"\"\"";
        else if (c == L'\n') result += L' ';
        else if (c == L'\r') result += L' ';
        else result += c;
      }
      result += L"\"";
      return result;
    };
    output << csvEscape(entry.fullTimestamp) << L","
           << csvEscape(entry.severity) << L","
           << csvEscape(entry.domain) << L","
           << csvEscape(entry.module) << L","
           << csvEscape(entry.service) << L","
           << entry.eventId << L","
           << entry.processId << L","
           << entry.threadId << L","
           << entry.repeatCount << L","
           << csvEscape(entry.message) << L","
           << csvEscape(entry.metadata) << L"\n";
  }
}
