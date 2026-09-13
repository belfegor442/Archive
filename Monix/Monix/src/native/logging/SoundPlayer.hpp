#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>

namespace monix {

class SoundPlayer {
 public:
  SoundPlayer(std::filesystem::path soundDir, bool enabled, bool loggedIn);

  void SetLoggedIn(bool loggedIn) { loggedIn_.store(loggedIn); }
  void SetEnabled(bool enabled) { enabled_.store(enabled); }

  void PlayLogSound();
  void PlayAlertSound(int logLevel);
  void PlayClickSound();

 private:
  std::filesystem::path soundDir_;
  std::atomic<bool> enabled_{true};
  std::atomic<bool> loggedIn_{false};
  std::atomic<unsigned int> logAliasIndex_{0};
  std::mutex mciMutex_;
};

} // namespace monix
