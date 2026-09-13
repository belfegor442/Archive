#include "SoundPlayer.hpp"

#include "../logging/LogEntry.hpp"

namespace monix {

SoundPlayer::SoundPlayer(std::filesystem::path soundDir, bool enabled, bool loggedIn)
    : soundDir_(std::move(soundDir)), enabled_(enabled), loggedIn_(loggedIn) {}

void SoundPlayer::PlayLogSound() {
  if (!enabled_ || !loggedIn_) return;

  static const wchar_t* logAliases[] = {
    L"monix_log0", L"monix_log1", L"monix_log2",
    L"monix_log3", L"monix_log4", L"monix_log5",
    L"monix_log6", L"monix_log7", L"monix_log8",
    L"monix_log9", L"monix_logA", L"monix_logB"
  };
  constexpr int kMaxLogAliases = 12;
  const unsigned int idx = logAliasIndex_.fetch_add(1);
  const std::wstring alias = logAliases[idx % kMaxLogAliases];
  std::lock_guard<std::mutex> lock(mciMutex_);
  mciSendStringW((L"close " + alias).c_str(), nullptr, 0, nullptr);
  const std::wstring soundFile = (soundDir_ / L"Log" / L"log.wav").wstring();
  const std::wstring cmd = L"open \"" + soundFile + L"\" type waveaudio alias " + alias;
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW((L"play " + alias + L" from 0").c_str(), nullptr, 0, nullptr);
  }
}

void SoundPlayer::PlayAlertSound(int logLevel) {
  if (!enabled_ || !loggedIn_) return;

  std::filesystem::path soundFile;
  switch (static_cast<LogLevel>(logLevel)) {
    case LogLevel::Critical:
    case LogLevel::Error:
      soundFile = soundDir_ / L"Log" / L"Error.mp3";
      break;
    case LogLevel::Warn:
      soundFile = soundDir_ / L"Log" / L"Warning.mp3";
      break;
    default:
      return;
  }

  if (!std::filesystem::exists(soundFile)) return;

  std::lock_guard<std::mutex> lock(mciMutex_);
  mciSendStringW(L"close monix_alert", nullptr, 0, nullptr);
  const std::wstring cmd = L"open \"" + soundFile.wstring() + L"\" type mpegvideo alias monix_alert";
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW(L"play monix_alert notify", nullptr, 0, nullptr);
  }
}

void SoundPlayer::PlayClickSound() {
  if (!enabled_ || !loggedIn_) return;

  const std::filesystem::path soundFile = soundDir_ / L"App" / L"click.mp3";
  if (!std::filesystem::exists(soundFile)) return;

  std::lock_guard<std::mutex> lock(mciMutex_);
  mciSendStringW(L"close monix_click", nullptr, 0, nullptr);
  const std::wstring cmd = L"open \"" + soundFile.wstring() + L"\" type mpegvideo alias monix_click";
  if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
    mciSendStringW(L"play monix_click notify", nullptr, 0, nullptr);
  }
}

} // namespace monix
