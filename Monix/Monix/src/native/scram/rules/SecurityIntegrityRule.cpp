#include "SecurityIntegrityRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void SecurityIntegrityRule::Evaluate(const Snapshot& current,
                                     const Snapshot* previous,
                                     std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  // 1. Unsigned driver load
  for (const auto& drv : current.unsignedDriverNames) {
    if (prev.unsignedDriverNames.find(drv) == prev.unsignedDriverNames.end()) {
      findings.push_back({
        L"Unsigned driver load: " + drv,
        L"A kernel driver without a valid digital signature has been loaded.",
        L"Unsigned: " + drv,
        18
      });
    }
  }

  // 2. Code signature invalid (self)
  if (prev.selfSignatureValid == 1 && current.selfSignatureValid == 0) {
    findings.push_back({
      L"Code signature invalid on executable.",
      L"The running executable no longer passes signature verification.",
      L"Signature: valid -> invalid on " + current.selfExePath,
      20
    });
  }
  if (current.selfSignatureValid == 0 && prev.selfSignatureValid != 0) {
    findings.push_back({
      L"Executable signature verification failed.",
      L"The main executable does not have a valid code signature.",
      L"No valid signature on executable.",
      16
    });
  }

  // 3. Certificate chain failure (proxy: unsigned drivers)
  if (current.unsignedDriverCount > prev.unsignedDriverCount && prev.unsignedDriverCount > 0) {
    findings.push_back({
      L"Certificate chain failure detected.",
      L"Additional unsigned drivers loaded, indicating certificate chain issues.",
      L"Unsigned drivers: " + std::to_wstring(prev.unsignedDriverCount) + L" -> " + std::to_wstring(current.unsignedDriverCount),
      14
    });
  }

  // 4. Tampered executable hash
  if (prev.selfHashComputed == 1 && current.selfHashComputed == 0) {
    findings.push_back({
      L"Tampered executable hash detected.",
      L"The executable file has been modified since last verification.",
      L"Executable hash verification changed.",
      20
    });
  }

  // 5. Binary reputation change (proxy: hash change)
  if (prev.selfHashComputed == 0 && current.selfHashComputed == 1) {
    findings.push_back({
      L"Binary reputation restored.",
      L"Executable hash now passes verification after previous failure.",
      L"Executable hash re-verified.",
      2
    });
  }

  // 6. Suspicious DLL sideloading (new unsigned hook/inject/detour modules)
  for (const auto& mod : current.suspiciousModules) {
    if (prev.suspiciousModules.find(mod) == prev.suspiciousModules.end()) {
      findings.push_back({
        L"Untrusted module loaded: " + mod,
        L"A module with suspicious naming has been loaded without valid digital signature.",
        L"Untrusted module: " + mod,
        14
      });
    }
  }

  // 7. Suspicious script execution
  if (current.suspiciousScriptHosts > prev.suspiciousScriptHosts && prev.suspiciousScriptHosts == 0) {
    findings.push_back({
      L"Suspicious script execution detected.",
      L"Script host processes (wscript/cscript/mshta/powershell) detected running.",
      L"Script hosts: " + std::to_wstring(current.suspiciousScriptHosts),
      12
    });
  }
  if (current.suspiciousScriptHosts > 3) {
    findings.push_back({
      L"Script execution burst detected.",
      L"Abnormal number of script host processes running simultaneously.",
      L"Script hosts: " + std::to_wstring(current.suspiciousScriptHosts) + L" active",
      14
    });
  }

  // 9. UAC prompt escalation (delta-based)
  if (current.uacConsentProcesses > prev.uacConsentProcesses) {
    findings.push_back({
      L"UAC prompt escalation detected.",
      L"User Account Control consent dialog is active, indicating privilege escalation request.",
      L"UAC consent processes: " + std::to_wstring(current.uacConsentProcesses),
      10
    });
  }

  // 10. Admin token creation (proxy: UAC consent + high integrity — delta-based)
  if (current.uacConsentProcesses > prev.uacConsentProcesses && current.cpuPct > 50.0) {
    findings.push_back({
      L"Admin token creation detected.",
      L"UAC consent active under load, indicating admin token elevation.",
      L"Admin token creation: UAC active under load.",
      8
    });
  }

  // 11. Protected process access (proxy: lsass process count changes — only after baseline established)
  if (prev.lsassAccessCount > 0 && current.lsassAccessCount > prev.lsassAccessCount) {
    findings.push_back({
      L"LSASS process count increase detected.",
      L"Additional lsass.exe instances have appeared, which is abnormal.",
      L"LSASS instances: " + std::to_wstring(current.lsassAccessCount),
      18
    });
  }

  // 12. LSASS enumeration pattern (multiple lsass instances)
  if (prev.lsassAccessCount > 0 && current.lsassAccessCount > prev.lsassAccessCount && current.lsassAccessCount > 3) {
    findings.push_back({
      L"LSASS enumeration pattern detected.",
      L"Multiple lsass.exe instances observed, a rare and suspicious condition.",
      L"LSASS count: " + std::to_wstring(current.lsassAccessCount),
      16
    });
  }

  // 13. Credential dump pattern (proxy: lsass + script host — only after baseline)
  if (prev.lsassAccessCount > 0 && current.lsassAccessCount > 1 && current.suspiciousScriptHosts > 0) {
    findings.push_back({
      L"Credential dump pattern detected.",
      L"Script host processes running with multiple LSASS instances suggest credential harvesting.",
      L"Credential risk: " + std::to_wstring(current.suspiciousScriptHosts) + L" scripts, " + std::to_wstring(current.lsassAccessCount) + L" LSASS",
      20
    });
  }

  // 14. Anti-debugging indicator
  if (current.debugPortActive == 1 && prev.debugPortActive == 0) {
    findings.push_back({
      L"Anti-debugging indicator detected.",
      L"Debugger has been attached to the running process.",
      L"Debugger attached to process.",
      10
    });
  }

  // 15. Anti-VM indicator
  if (current.vmIndicators > prev.vmIndicators && prev.vmIndicators == 0) {
    findings.push_back({
      L"Anti-VM indicator detected.",
      L"Virtual machine guest services detected, indicating sandbox/analysis environment.",
      L"VM indicators: " + std::to_wstring(current.vmIndicators),
      8
    });
  }

  // 16. Hook installation attempt (unsigned hook module loaded)
  if (prev.hookModulesDetected > 0 && current.hookModulesDetected > prev.hookModulesDetected) {
    findings.push_back({
      L"Unsigned hook module loaded.",
      L"An untrusted module with hook/inject/detour naming has been loaded without valid signature.",
      L"Hook modules: " + std::to_wstring(current.hookModulesDetected),
      16
    });
  }

  // 17. Inline patch detection (proxy: PE header tamper)
  if (current.peHeaderTamper > prev.peHeaderTamper && prev.peHeaderTamper == 0) {
    findings.push_back({
      L"Inline patch detected.",
      L"PE header of a loaded module has been modified in memory.",
      L"PE tamper: " + std::to_wstring(current.peHeaderTamper) + L" module(s)",
      18
    });
  }

  // 18. Import table patch detection (proxy: PE header tamper — delta-based)
  if (current.peHeaderTamper > prev.peHeaderTamper) {
    findings.push_back({
      L"Import table patch risk detected.",
      L"PE header tampering may indicate import table patching.",
      L"Import table patch risk: " + std::to_wstring(current.peHeaderTamper) + L" tampered headers.",
      8
    });
  }

  // 19. Syscall stub patch detection — REMOVED: proxy detection without forensic evidence.
  // PE tamper + hook-named modules is not sufficient evidence for syscall interception.
  // Rule 20 (PE header tamper delta) provides evidence-based detection.

  // 20. PE header tamper
  if (current.peHeaderTamper > prev.peHeaderTamper) {
    findings.push_back({
      L"PE header tamper detected.",
      L"Additional module headers have been modified in memory since last sample.",
      L"PE tamper delta: " + std::to_wstring(prev.peHeaderTamper) + L" -> " + std::to_wstring(current.peHeaderTamper),
      16
    });
  }

  // 21. Integrity check failure (hash verification lost)
  if (prev.selfHashComputed != 0 && current.selfHashComputed == 0) {
    findings.push_back({
      L"Integrity check failure.",
      L"Self-integrity verification failed for the running executable.",
      L"Self check: sig=" + std::to_wstring(current.selfSignatureValid) + L" hash=" + std::to_wstring(current.selfHashComputed),
      14
    });
  }

  // 22. Sandbox escape indicator (proxy: VM detection + hook modules — delta-based)
  if ((current.vmIndicators > prev.vmIndicators || current.hookModulesDetected > prev.hookModulesDetected)
      && current.vmIndicators > 0 && current.hookModulesDetected > 0) {
    findings.push_back({
      L"Sandbox escape indicator detected.",
      L"VM environment with hook modules suggests sandbox analysis or escape attempt.",
      L"Sandbox risk: VM=" + std::to_wstring(current.vmIndicators) + L" hooks=" + std::to_wstring(current.hookModulesDetected),
      16
    });
  }

  // 23. Privilege misuse pattern (proxy: UAC + admin processes — delta-based)
  if ((current.uacConsentProcesses > prev.uacConsentProcesses || current.suspiciousScriptHosts > prev.suspiciousScriptHosts)
      && current.uacConsentProcesses > 0 && current.suspiciousScriptHosts > 0) {
    findings.push_back({
      L"Privilege misuse pattern detected.",
      L"UAC escalation combined with script execution indicates privilege abuse.",
      L"Privilege risk: UAC=" + std::to_wstring(current.uacConsentProcesses) + L" scripts=" + std::to_wstring(current.suspiciousScriptHosts),
      14
    });
  }

  // 24. Suspicious scheduled task
  if (current.scheduledTaskCount > prev.scheduledTaskCount + 5 && prev.scheduledTaskCount > 0) {
    findings.push_back({
      L"Suspicious scheduled task creation.",
      L"Multiple new scheduled tasks have been created, a common persistence mechanism.",
      L"Tasks: " + std::to_wstring(prev.scheduledTaskCount) + L" -> " + std::to_wstring(current.scheduledTaskCount),
      12
    });
  }

  // 25. Persistence mechanism detection
  if (current.scheduledTaskCount > prev.scheduledTaskCount + 10 && prev.scheduledTaskCount > 0) {
    findings.push_back({
      L"Persistence mechanism detected.",
      L"Large number of new scheduled tasks indicate persistence installation.",
      L"New tasks: +" + std::to_wstring(current.scheduledTaskCount - prev.scheduledTaskCount),
      16
    });
  }
}

} // namespace monix
