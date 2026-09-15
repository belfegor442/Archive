#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace monix {

struct SecurityState {
  int selfSignatureValid = -1;
  int selfHashComputed = -1;
  int unsignedDriverCount = 0;
  std::set<std::wstring> unsignedDriverNames;
  int suspiciousScriptHosts = 0;
  int uacConsentProcesses = 0;
  int lsassAccessCount = 0;
  int debugPortActive = 0;
  int vmIndicators = 0;
  int hookModulesDetected = 0;
  int peHeaderTamper = 0;
  int scheduledTaskCount = 0;
  std::set<std::wstring> suspiciousModules;
  std::set<std::wstring> processPaths;
  std::wstring selfExePath;
};

}
