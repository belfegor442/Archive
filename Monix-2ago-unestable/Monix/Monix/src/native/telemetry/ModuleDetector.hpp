#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "EnvironmentBaseline.hpp"

namespace monix {

class ModuleDetector {
 public:
  ModuleDetector();

  void Initialize();
  void CollectModules(DWORD processId, std::vector<ModuleEvidence>& outModules);
  void CollectAllProcessModules(std::vector<ModuleEvidence>& outModules);
  void ClassifyModule(ModuleEvidence& module) const;
  bool VerifyModuleSignature(const std::wstring& filePath, int& signatureValid,
                             std::wstring& signerName, std::wstring& issuerName) const;
  std::wstring ComputeModuleHash(const std::wstring& filePath) const;
  bool DetectHookIndicators(const ModuleEvidence& module) const;
  bool DetectPeHeaderTamper(const ModuleEvidence& module) const;
  bool IsKnownSystemModule(const std::wstring& moduleName) const;
  bool IsKnownSafeModule(const std::wstring& moduleName) const;
  void SetBaseline(const std::vector<ModuleEvidence>& baselineModules);
  void DetectChanges(const std::vector<ModuleEvidence>& currentModules,
                     std::vector<EnvironmentChange>& outChanges) const;

  const std::vector<ModuleEvidence>& GetLastCollected() const { return lastCollected_; }

 private:
  void EnumerateProcessModules(DWORD processId, std::vector<ModuleEvidence>& outModules);
  std::wstring GetModuleFullPath(HMODULE hModule) const;
  std::uint64_t GetModuleFileSize(const std::wstring& path) const;
  std::uint32_t GetModuleTimestamp(const std::wstring& path) const;
  int CheckProtectionStatus(HMODULE hModule) const;
  bool IsSystemDirectory(const std::wstring& path) const;
  bool CheckSignatureAgainstStore(const std::wstring& filePath) const;

  std::set<std::wstring> knownSystemModules_;
  std::set<std::wstring> knownSafeModules_;
  std::set<std::wstring> suspiciousNamePatterns_;
  std::set<std::wstring> whitelistedHookModules_;
  std::vector<ModuleEvidence> baselineModules_;
  std::vector<ModuleEvidence> lastCollected_;
  bool initialized_ = false;
};

} // namespace monix
