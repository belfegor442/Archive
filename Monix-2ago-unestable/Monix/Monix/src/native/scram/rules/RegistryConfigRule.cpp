#include "RegistryConfigRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <cstdlib>

namespace monix {

void RegistryConfigRule::Evaluate(const Snapshot& current,
                                  const Snapshot* previous,
                                  std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  // 1. Registry key create
  if (current.regKeyCountRun > prev.regKeyCountRun + 1) {
    findings.push_back({
      L"Registry key creation in Run.",
      L"New subkeys detected under the Run key, indicating persistence addition.",
      L"Run keys: " + std::to_wstring(prev.regKeyCountRun) + L" -> " + std::to_wstring(current.regKeyCountRun) + L".",
      10
    });
  }

  // 2. Registry key delete
  if (current.regKeyCountRun < prev.regKeyCountRun - 1) {
    findings.push_back({
      L"Registry key deletion in Run.",
      L"Subkeys removed from the Run key.",
      L"Run keys removed: " + std::to_wstring(prev.regKeyCountRun - current.regKeyCountRun) + L".",
      12
    });
  }

  // 3. Registry value create
  if (current.regValueCountRun > prev.regValueCountRun + 1) {
    findings.push_back({
      L"Registry value creation in Run.",
      L"New values added to the Run key, possibly establishing persistence.",
      L"Run values: " + std::to_wstring(prev.regValueCountRun) + L" -> " + std::to_wstring(current.regValueCountRun) + L".",
      10
    });
  }

  // 4. Registry value delete
  if (current.regValueCountRun < prev.regValueCountRun - 1) {
    findings.push_back({
      L"Registry value deletion in Run.",
      L"Values removed from the Run key.",
      L"Run values removed: " + std::to_wstring(prev.regValueCountRun - current.regValueCountRun) + L".",
      12
    });
  }

  // 5. Registry value modify
  if (current.regHashRun != prev.regHashRun && current.regValueCountRun == prev.regValueCountRun) {
    findings.push_back({
      L"Registry value modification in Run.",
      L"Value data changed in Run key without count change, indicating modification of existing persistence.",
      L"Run hash changed, values stable.",
      14
    });
  }

  // 6. Run key modification (new entries added/removed)
  if (current.regHashRun != prev.regHashRun && current.regValueCountRun != prev.regValueCountRun) {
    findings.push_back({
      L"Run key modification detected.",
      L"The HKLM Run key hash has changed, indicating new or modified startup entries.",
      L"Run hash: " + std::to_wstring(prev.regHashRun) + L" -> " + std::to_wstring(current.regHashRun) + L".",
      12
    });
  }

  // 7. Shell open command change
  if (current.regHashShell != prev.regHashShell && prev.regHashShell != 0) {
    findings.push_back({
      L"Shell open command changed.",
      L"Registry shell\\open\\command key has been modified, which can intercept program execution.",
      L"Shell hash: " + std::to_wstring(prev.regHashShell) + L" -> " + std::to_wstring(current.regHashShell) + L".",
      16
    });
  }

  // 8. Startup folder change (only significant changes)
  if (current.startupFolderCount != prev.startupFolderCount) {
    const int delta = current.startupFolderCount - prev.startupFolderCount;
    if (delta > 3 || delta < -3) {
      findings.push_back({
        L"Startup folder change detected.",
        L"Number of items in the startup folder has changed significantly.",
        L"Startup items: " + std::to_wstring(prev.startupFolderCount) + L" -> " + std::to_wstring(current.startupFolderCount) + L".",
        10
      });
    }
  }

  // 9. Policy key modification
  if (current.regHashPolicies != prev.regHashPolicies && prev.regHashPolicies != 0) {
    findings.push_back({
      L"Policy key modification detected.",
      L"Group Policy registry keys have been modified.",
      L"Policies hash changed.",
      12
    });
  }

  // 10. Service config change
  if (current.regHashServices != prev.regHashServices && prev.regHashServices != 0) {
    findings.push_back({
      L"Service configuration change.",
      L"A service registry configuration has been modified.",
      L"Services hash: " + std::to_wstring(prev.regHashServices) + L" -> " + std::to_wstring(current.regHashServices) + L".",
      14
    });
  }

  // 11. Driver config change (only significant changes — driver counts fluctuate)
  if (current.regKeyCountDrivers != prev.regKeyCountDrivers && prev.regKeyCountDrivers != 0) {
    const int delta = current.regKeyCountDrivers - prev.regKeyCountDrivers;
    if (delta > 5 || delta < -5) {
      findings.push_back({
        L"Driver configuration change.",
        L"The number of registered driver services has changed significantly.",
        L"Driver services: " + std::to_wstring(prev.regKeyCountDrivers) + L" -> " + std::to_wstring(current.regKeyCountDrivers) + L".",
        16
      });
    }
  }

  // 12. Telemetry config change
  if (current.regHashTelemetry != prev.regHashTelemetry && prev.regHashTelemetry != 0) {
    findings.push_back({
      L"Telemetry configuration change.",
      L"Windows telemetry/DiagTrack settings have been modified.",
      L"Telemetry hash changed.",
      10
    });
  }

  // 13. Audit policy change
  if (current.regHashAudit != prev.regHashAudit && prev.regHashAudit != 0) {
    findings.push_back({
      L"Audit policy change.",
      L"Local security audit policy registry keys have been modified.",
      L"Audit policy hash changed.",
      14
    });
  }

  // 14. Security policy change
  if (current.regHashUac != prev.regHashUac && prev.regHashUac != 0) {
    findings.push_back({
      L"Security policy change.",
      L"Security-related LSA registry values have been modified.",
      L"UAC/security policy hash changed.",
      16
    });
  }

  // 15. Firewall policy change (delta-based: only significant changes)
  if (current.regHashFirewall != prev.regHashFirewall && prev.regHashFirewall != 0) {
    const long long diff = static_cast<long long>(current.regHashFirewall) - static_cast<long long>(prev.regHashFirewall);
    const int fwDelta = static_cast<int>(diff < 0 ? -diff : diff);
    if (fwDelta > 10) {
      findings.push_back({
        L"Firewall policy change.",
        L"Windows Firewall policy registry keys have been modified.",
        L"Firewall hash: " + std::to_wstring(prev.regHashFirewall) + L" -> " + std::to_wstring(current.regHashFirewall) + L".",
        18
      });
    }
  }

  // 16. UAC policy change (removed: duplicate of rule 14 above)

  // 17. Task scheduler config change
  if (current.regHashTaskSched != prev.regHashTaskSched && prev.regHashTaskSched != 0) {
    findings.push_back({
      L"Task Scheduler configuration change.",
      L"Task Scheduler registry cache has been modified.",
      L"TaskSched hash changed.",
      12
    });
  }

  // 18. COM registration change
  if (current.regKeyCountCom != prev.regKeyCountCom && prev.regKeyCountCom != 0) {
    findings.push_back({
      L"COM registration change.",
      L"Number of registered COM classes has changed.",
      L"COM CLSIDs: " + std::to_wstring(prev.regKeyCountCom) + L" -> " + std::to_wstring(current.regKeyCountCom) + L".",
      10
    });
  }

  // 19. Shell extension registration
  if (current.regValueCountShellExt != prev.regValueCountShellExt && prev.regValueCountShellExt != 0) {
    findings.push_back({
      L"Shell extension registration change.",
      L"Approved shell extensions list has been modified.",
      L"Shell extensions: " + std::to_wstring(prev.regValueCountShellExt) + L" -> " + std::to_wstring(current.regValueCountShellExt) + L".",
      10
    });
  }

  // 20. File association change
  if (current.regHashFileAssoc != prev.regHashFileAssoc && prev.regHashFileAssoc != 0) {
    findings.push_back({
      L"File association change.",
      L"File type associations have been modified.",
      L"File assoc hash changed.",
      12
    });
  }

  // 21. Default app change
  if (current.regHashDefApp != prev.regHashDefApp && prev.regHashDefApp != 0) {
    findings.push_back({
      L"Default application change.",
      L"Default application settings have been modified.",
      L"Default app hash changed.",
      8
    });
  }

  // 22. Environment variable change
  if (current.envVarCount != prev.envVarCount) {
    findings.push_back({
      L"Environment variable change.",
      L"Number of environment variables has changed.",
      L"Env vars: " + std::to_wstring(prev.envVarCount) + L" -> " + std::to_wstring(current.envVarCount) + L".",
      10
    });
  }

  // 23. Path variable change
  if (current.regHashEnvPath != prev.regHashEnvPath && prev.regHashEnvPath != 0) {
    findings.push_back({
      L"PATH variable change.",
      L"The system PATH environment variable has been modified.",
      L"PATH hash: " + std::to_wstring(prev.regHashEnvPath) + L" -> " + std::to_wstring(current.regHashEnvPath) + L".",
      14
    });
  }

  // 24. Config file hash change
  if (current.regHashConfigFile != prev.regHashConfigFile && prev.regHashConfigFile != 0) {
    findings.push_back({
      L"Configuration file hash change.",
      L"A monitored configuration file has been modified.",
      L"Config hash changed.",
      12
    });
  }

  // 25. App association registry change
  if (current.regHashAppAssoc != prev.regHashAppAssoc && prev.regHashAppAssoc != 0) {
    findings.push_back({
      L"Application association registry change.",
      L"File extension application associations have been modified.",
      L"App assoc hash changed.",
      10
    });
  }
}

} // namespace monix
