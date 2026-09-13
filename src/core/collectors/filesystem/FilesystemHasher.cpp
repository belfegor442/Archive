#include "FilesystemHasher.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace monix::collectors::fs {

std::string FilesystemHasher::hashBytes(const void* data, std::size_t size) {
  BCRYPT_ALG_HANDLE algHandle = nullptr;
  BCRYPT_HASH_HANDLE hashHandle = nullptr;

  NTSTATUS status = BCryptOpenAlgorithmProvider(&algHandle, BCRYPT_SHA256_ALGORITHM,
                                                  nullptr, 0);
  if (!BCRYPT_SUCCESS(status)) return "";

  status = BCryptCreateHash(algHandle, &hashHandle, nullptr, 0, nullptr, 0, 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return "";
  }

  status = BCryptHashData(hashHandle,
                          reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
                          static_cast<ULONG>(size), 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptDestroyHash(hashHandle);
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return "";
  }

  std::vector<std::uint8_t> digest(32);
  status = BCryptFinishHash(hashHandle, digest.data(), 32, 0);

  BCryptDestroyHash(hashHandle);
  BCryptCloseAlgorithmProvider(algHandle, 0);

  if (!BCRYPT_SUCCESS(status)) return "";

  std::ostringstream oss;
  for (auto b : digest) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
  }
  return oss.str();
}

std::string FilesystemHasher::hashFileSHA256(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return "";

  BCRYPT_ALG_HANDLE algHandle = nullptr;
  BCRYPT_HASH_HANDLE hashHandle = nullptr;

  NTSTATUS status = BCryptOpenAlgorithmProvider(&algHandle, BCRYPT_SHA256_ALGORITHM,
                                                  nullptr, 0);
  if (!BCRYPT_SUCCESS(status)) return "";

  status = BCryptCreateHash(algHandle, &hashHandle, nullptr, 0, nullptr, 0, 0);
  if (!BCRYPT_SUCCESS(status)) {
    BCryptCloseAlgorithmProvider(algHandle, 0);
    return "";
  }

  constexpr std::size_t kBufferSize = 65536;
  char buffer[kBufferSize];

  while (file.read(buffer, kBufferSize) || file.gcount() > 0) {
    status = BCryptHashData(hashHandle,
                            reinterpret_cast<PUCHAR>(buffer),
                            static_cast<ULONG>(file.gcount()), 0);
    if (!BCRYPT_SUCCESS(status)) {
      BCryptDestroyHash(hashHandle);
      BCryptCloseAlgorithmProvider(algHandle, 0);
      return "";
    }
    if (!file) break;
  }

  std::vector<std::uint8_t> digest(32);
  status = BCryptFinishHash(hashHandle, digest.data(), 32, 0);

  BCryptDestroyHash(hashHandle);
  BCryptCloseAlgorithmProvider(algHandle, 0);

  if (!BCRYPT_SUCCESS(status)) return "";

  std::ostringstream oss;
  for (auto b : digest) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b);
  }
  return oss.str();
}

std::string FilesystemHasher::hashFile(const std::filesystem::path& path, FileHashMode mode) {
  switch (mode) {
    case FileHashMode::Disabled:
      return "";
    case FileHashMode::OnDemand:
    case FileHashMode::Suspicious:
    case FileHashMode::Forensic:
    case FileHashMode::Always:
      return hashFileSHA256(path);
  }
  return "";
}

}  // namespace monix::collectors::fs
