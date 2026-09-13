#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace monix::updater {

struct UpdateInfo {
  bool available = false;
  std::wstring latestVersion;
  std::wstring currentVersion;
  std::wstring tagName;
  std::wstring body;
  std::wstring htmlUrl;
  std::wstring exeDownloadUrl;
};

class AutoUpdater {
public:
  using Callback = std::function<void(const UpdateInfo&)>;

  AutoUpdater();
  ~AutoUpdater();

  void CheckForUpdateAsync(Callback callback);
  bool CheckForUpdateSync(UpdateInfo& info);

  bool IsChecking() const { return checking_.load(); }
  void CancelPendingCheck();

private:
  bool HttpsGet(const std::wstring& url, std::string& responseBody);
  bool ParseJsonString(const std::string& json, const std::string& key, std::string& value);
  bool CompareVersions(const std::wstring& remote, const std::wstring& local);
  void StripQuotes(std::string& s);

  std::atomic<bool> checking_{false};
  std::thread worker_;
  HINTERNET hSession_ = nullptr;
};

} // namespace monix::updater
