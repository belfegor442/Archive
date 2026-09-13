#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <fstream>
#include <filesystem>
#include <bcrypt.h>

#pragma comment(lib, "Bcrypt.lib")

namespace Monix {
namespace Security {

enum class AuthState {
  Idle,
  LoginOverlay,
  LockedOut,
  Authenticated
};

struct AuthConfig {
  std::wstring configPath;
  std::wstring storedHash;
  int maxAttempts = 5;
  int lockoutDurationMs = 30000;
};

class AuthManager {
public:
  AuthManager() = default;

  void Init(const std::wstring& configPath) {
    config_.configPath = configPath;
    LoadConfig();
  }

  AuthState GetState() const { return state_; }
  const AuthConfig& GetConfig() const { return config_; }
  bool HasPassword() const { return !config_.storedHash.empty(); }
  bool IsLockedOut() const {
    if (state_ != AuthState::LockedOut) return false;
    ULONGLONG elapsed = GetTickCount64() - lockoutStartMs_;
    return elapsed < static_cast<ULONGLONG>(config_.lockoutDurationMs);
  }

  bool HandlePasswordEntry(const std::wstring& input, std::wstring& outMsg) {
    if (input.empty()) {
      if (config_.storedHash.empty()) {
        state_ = AuthState::Authenticated;
        outMsg = L"Access granted.";
        return true;
      }
      outMsg = L"Password cannot be empty.";
      return false;
    }

    if (config_.storedHash.empty()) {
      // First boot: store new password
      config_.storedHash = HashPassword(input);
      SaveConfig();
      state_ = AuthState::Authenticated;
      outMsg = L"Password set. Access granted.";
      return true;
    }

    if (IsLockedOut()) {
      ULONGLONG remaining = config_.lockoutDurationMs - (GetTickCount64() - lockoutStartMs_);
      wchar_t buf[128];
      _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"Account locked. Try again in %llu seconds.", remaining / 1000);
      outMsg = buf;
      return false;
    }

    if (VerifyPassword(input, config_.storedHash)) {
      attemptCount_ = 0;
      state_ = AuthState::Authenticated;
      outMsg = L"Access granted.";
      return true;
    }

    attemptCount_++;
    if (attemptCount_ >= config_.maxAttempts) {
      state_ = AuthState::LockedOut;
      lockoutStartMs_ = GetTickCount64();
      outMsg = L"Too many failed attempts. Account locked.";
    } else {
      wchar_t buf[128];
      _snwprintf_s(buf, _countof(buf), _TRUNCATE,
        L"Invalid password. %d attempts remaining.", config_.maxAttempts - attemptCount_);
      outMsg = buf;
    }
    return false;
  }

  void ResetPassword() {
    config_.storedHash.clear();
    attemptCount_ = 0;
    state_ = AuthState::LoginOverlay;
    SaveConfig();
  }

  void ShowLogin() { state_ = AuthState::LoginOverlay; }

private:
  void LoadConfig() {
    if (!std::filesystem::exists(config_.configPath)) return;
    std::wifstream f(config_.configPath.c_str());
    if (!f.is_open()) return;
    std::wstring line;
    while (std::getline(f, line)) {
      auto pos = line.find(L'=');
      if (pos == std::wstring::npos) continue;
      std::wstring key = line.substr(0, pos);
      std::wstring val = line.substr(pos + 1);
      if (key == L"auth_hash") {
        config_.storedHash = val;
      }
    }
  }

  void SaveConfig() {
    std::wstring content;
    if (std::filesystem::exists(config_.configPath)) {
      std::wifstream fin(config_.configPath.c_str());
      std::wstring line;
      while (std::getline(fin, line)) {
        auto pos = line.find(L'=');
        if (pos != std::wstring::npos) {
          std::wstring key = line.substr(0, pos);
          if (key != L"auth_hash") {
            content += line + L"\n";
          }
        }
      }
    }
    content += L"auth_hash=" + config_.storedHash + L"\n";
    std::wofstream fout(config_.configPath.c_str());
    fout << content;
  }

  static std::wstring HashPassword(const std::wstring& password) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::wstring result;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return password;
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) < 0) {
      BCryptCloseAlgorithmProvider(hAlg, 0);
      return password;
    }

    BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(password.c_str())),
      static_cast<ULONG>(password.size() * sizeof(wchar_t)), 0);

    UCHAR hash[32]{};
    BCryptFinishHash(hHash, hash, sizeof(hash), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    wchar_t hex[65]{};
    for (int i = 0; i < 32; i++) {
      _snwprintf_s(hex + i * 2, 3, _TRUNCATE, L"%02x", hash[i]);
    }
    return hex;
  }

  static bool VerifyPassword(const std::wstring& input, const std::wstring& storedHash) {
    return HashPassword(input) == storedHash;
  }

  AuthConfig config_;
  AuthState state_ = AuthState::LoginOverlay;
  int attemptCount_ = 0;
  ULONGLONG lockoutStartMs_ = 0;
};

} // namespace Security
} // namespace Monix
