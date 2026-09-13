#include "SoundEffects.hpp"

#include "../../MonixApp.hpp"
#include "../../logging/LogEntry.hpp"

#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <filesystem>

using namespace monix;

void MonixApp::PlayAlertSound(LogLevel level) {
  std::shared_lock<std::shared_mutex> lock(configMutex_);
  if (!config_.soundEnabled || !state_.loggedIn) {
    return;
  }

  std::filesystem::path soundFile;
  switch (level) {
    case LogLevel::Critical:
    case LogLevel::Error:
      soundFile = paths_.soundDir / L"Log" / L"Error.mp3";
      break;
    case LogLevel::Warn:
      soundFile = paths_.soundDir / L"Log" / L"Warning.mp3";
      break;
    default:
      return;
  }

  if (!std::filesystem::exists(soundFile)) {
    return;
  }

  mciSendStringW(L"close monix_alert", nullptr, 0, nullptr);
  const std::wstring cmd = L"open \"" + soundFile.wstring() + L"\" type mpegvideo alias monix_alert";
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW(L"play monix_alert notify", nullptr, 0, nullptr);
  }
}

void MonixApp::PlayLogSound() {
  if (!config_.soundEnabled || !state_.loggedIn) {
    return;
  }
  static std::atomic<int> logAliasIndex{0};
  static const wchar_t* logAliases[] = {
    L"monix_log0", L"monix_log1", L"monix_log2",
    L"monix_log3", L"monix_log4", L"monix_log5",
    L"monix_log6", L"monix_log7", L"monix_log8",
    L"monix_log9", L"monix_logA", L"monix_logB"
  };
  constexpr int kMaxLogAliases = 12;
  const std::wstring alias = logAliases[logAliasIndex % kMaxLogAliases];
  logAliasIndex++;
  mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
  const std::wstring soundFile = (paths_.soundDir / L"Log" / L"log.wav").wstring();
  const std::wstring cmd = L"open \"" + soundFile + L"\" type waveaudio alias " + alias;
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW((L"play " + alias + L" from 0").c_str(), nullptr, 0, nullptr);
  }
}

void MonixApp::PlayClickSound() {
  if (soundPlayer_) {
    soundPlayer_->PlayClickSound();
  }
}
