#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../snapshot/SystemSnapshot.hpp"
#include "../correlation/CorrelationEngine.hpp"
#include "../anomaly/AnomalyDetector.hpp"

namespace monix {

struct DiagnosticCheck {
  std::wstring id;
  std::wstring name;
  std::wstring category;
  std::wstring description;
  int severity = 0;
  bool passed = true;
  std::wstring detail;
  std::vector<std::wstring> recommendations;
  uint64_t timestampNs = 0;
};

struct DiagnosticReport {
  uint64_t id = 0;
  uint64_t timestampNs = 0;
  int totalChecks = 0;
  int passedChecks = 0;
  int failedChecks = 0;
  int warningChecks = 0;
  int overallScore = 100;
  std::vector<DiagnosticCheck> checks;
  std::map<std::wstring, int> categoryScores;
};

class DiagnosticsEngine {
public:
  DiagnosticsEngine() {
    RegisterBuiltinChecks();
  }

  DiagnosticReport Run(const SystemSnapshot& snap,
                        const CorrelationEngine& correlation,
                        const AnomalyDetector& anomaly,
                        uint64_t timestampNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    DiagnosticReport report;
    report.id = nextReportId_++;
    report.timestampNs = timestampNs;

    for (auto& check : checks_) {
      DiagnosticCheck result;
      result.id = check.id;
      result.name = check.name;
      result.category = check.category;
      result.description = check.description;
      result.timestampNs = timestampNs;
      check.evaluate(snap, correlation, anomaly, result);
      report.checks.push_back(std::move(result));
    }

    report.totalChecks = static_cast<int>(report.checks.size());
    for (auto& c : report.checks) {
      if (c.passed) report.passedChecks++;
      else if (c.severity >= 3) report.failedChecks++;
      else report.warningChecks++;
    }

    std::map<std::wstring, int> catTotal, catPassed;
    for (auto& c : report.checks) {
      catTotal[c.category]++;
      if (c.passed) catPassed[c.category]++;
    }
    for (auto& [cat, total] : catTotal) {
      report.categoryScores[cat] =
        static_cast<int>((static_cast<double>(catPassed[cat]) / total) * 100.0);
    }

    int score = 100;
    for (auto& c : report.checks) {
      if (!c.passed) {
        if (c.severity == 4) score -= 25;
        else if (c.severity == 3) score -= 15;
        else if (c.severity == 2) score -= 10;
        else if (c.severity == 1) score -= 5;
        else score -= 2;
      }
    }
    report.overallScore = (score < 0) ? 0 : score;

    return report;
  }

  void AddCheck(DiagnosticCheck check) {
    std::lock_guard<std::mutex> lock(mutex_);
    checks_.push_back(std::move(check));
  }

  std::vector<DiagnosticCheck> RecentReports(int count = 10) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DiagnosticCheck> result;
    int n = std::min(count, static_cast<int>(checks_.size()));
    for (int i = static_cast<int>(checks_.size()) - n;
         i < static_cast<int>(checks_.size()); ++i) {
      result.push_back(checks_[i]);
    }
    return result;
  }

private:
  struct CheckDef {
    std::wstring id;
    std::wstring name;
    std::wstring category;
    std::wstring description;
    int severity = 0;
    std::function<void(const SystemSnapshot&, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck&)> evaluate;
  };

  void RegisterBuiltinChecks() {
    {
      CheckDef c;
      c.id = L"CPU_HEALTH";
      c.name = L"CPU Health";
      c.category = L"hardware";
      c.description = L"CPU temperature and utilization check";
      c.severity = 3;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        if (snap.thermal.cpuCoreTempC > 85.0) {
          result.passed = false;
          result.severity = 4;
          result.detail = L"CPU temp " +
            std::to_wstring(static_cast<int>(snap.thermal.cpuCoreTempC)) + L"C";
          result.recommendations.push_back(L"Check CPU cooler and thermal paste");
          result.recommendations.push_back(L"Clean dust from CPU heatsink");
        } else if (snap.thermal.cpuCoreTempC > 75.0) {
          result.passed = false;
          result.severity = 2;
          result.detail = L"CPU temp elevated: " +
            std::to_wstring(static_cast<int>(snap.thermal.cpuCoreTempC)) + L"C";
          result.recommendations.push_back(L"Monitor CPU temperature");
        } else {
          result.detail = L"CPU temp normal: " +
            std::to_wstring(static_cast<int>(snap.thermal.cpuCoreTempC)) + L"C";
        }
      };
      checks_.push_back(c);
    }
    {
      CheckDef c;
      c.id = L"MEMORY_HEALTH";
      c.name = L"Memory Health";
      c.category = L"memory";
      c.description = L"RAM usage check";
      c.severity = 2;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        double usedPct = snap.memory.UsedPct();
        if (usedPct > 90.0) {
          result.passed = false;
          result.severity = 3;
          result.detail = L"RAM at " + std::to_wstring(static_cast<int>(usedPct)) + L"%";
          result.recommendations.push_back(L"Close memory-heavy applications");
          result.recommendations.push_back(L"Consider adding more RAM");
        } else if (usedPct > 75.0) {
          result.passed = false;
          result.severity = 1;
          result.detail = L"RAM at " + std::to_wstring(static_cast<int>(usedPct)) + L"%";
          result.recommendations.push_back(L"Monitor memory usage");
        } else {
          result.detail = L"RAM at " + std::to_wstring(static_cast<int>(usedPct)) + L"%";
        }
      };
      checks_.push_back(c);
    }
    {
      CheckDef c;
      c.id = L"STORAGE_HEALTH";
      c.name = L"Storage Health";
      c.category = L"storage";
      c.description = L"Disk space and health check";
      c.severity = 3;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        if (!snap.storage.ready) {
          result.passed = false;
          result.severity = 4;
          result.detail = L"Storage not ready";
          result.recommendations.push_back(L"Check disk connection");
          result.recommendations.push_back(L"Run disk diagnostics");
          return;
        }
        if (snap.storage.totalBytes > 0) {
          double freePct = (static_cast<double>(snap.storage.freeBytes) /
            static_cast<double>(snap.storage.totalBytes)) * 100.0;
          if (freePct < 10.0) {
            result.passed = false;
            result.severity = 3;
            result.detail = L"Disk free: " + std::to_wstring(static_cast<int>(freePct)) + L"%";
            result.recommendations.push_back(L"Free up disk space");
            result.recommendations.push_back(L"Run disk cleanup");
          } else if (freePct < 20.0) {
            result.passed = false;
            result.severity = 1;
            result.detail = L"Disk free: " + std::to_wstring(static_cast<int>(freePct)) + L"%";
            result.recommendations.push_back(L"Plan to free disk space");
          } else {
            result.detail = L"Disk free: " + std::to_wstring(static_cast<int>(freePct)) + L"%";
          }
        }
        if (snap.storage.wearPct > 80.0) {
          result.passed = false;
          result.severity = 2;
          result.detail += L" | SSD wear: " + std::to_wstring(static_cast<int>(snap.storage.wearPct)) + L"%";
          result.recommendations.push_back(L"SSD wear high — plan replacement");
        }
      };
      checks_.push_back(c);
    }
    {
      CheckDef c;
      c.id = L"NETWORK_HEALTH";
      c.name = L"Network Health";
      c.category = L"network";
      c.description = L"Network connectivity check";
      c.severity = 2;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        if (snap.network.rttMs > 200.0) {
          result.passed = false;
          result.severity = 3;
          result.detail = L"RTT: " + std::to_wstring(static_cast<int>(snap.network.rttMs)) + L"ms";
          result.recommendations.push_back(L"Check network connection");
          result.recommendations.push_back(L"Restart network adapter");
        } else if (snap.network.rttMs > 100.0) {
          result.passed = false;
          result.severity = 1;
          result.detail = L"RTT: " + std::to_wstring(static_cast<int>(snap.network.rttMs)) + L"ms";
          result.recommendations.push_back(L"Monitor network latency");
        } else {
          result.detail = L"RTT: " + std::to_wstring(static_cast<int>(snap.network.rttMs)) + L"ms";
        }
        if (snap.network.droppedPktPct > 5.0) {
          result.passed = false;
          result.severity = 2;
          result.detail += L" | Drops: " + std::to_wstring(static_cast<int>(snap.network.droppedPktPct)) + L"%";
          result.recommendations.push_back(L"Check network stability");
        }
      };
      checks_.push_back(c);
    }
    {
      CheckDef c;
      c.id = L"SECURITY_HEALTH";
      c.name = L"Security Health";
      c.category = L"security";
      c.description = L"Security configuration check";
      c.severity = 3;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        bool ok = true;
        if (snap.security.defenderRealtimePct < 100.0) {
          result.passed = false;
          result.severity = 3;
          result.detail = L"Defender realtime protection off";
          result.recommendations.push_back(L"Enable Windows Defender realtime protection");
          ok = false;
        }
        if (snap.security.rdpEnabled) {
          result.passed = false;
          result.severity = 2;
          result.detail = L"RDP enabled";
          result.recommendations.push_back(L"Disable RDP if not needed");
          ok = false;
        }
        if (snap.security.sshEnabled) {
          result.passed = false;
          result.severity = 1;
          result.detail = L"SSH enabled";
          result.recommendations.push_back(L"Review SSH configuration");
          ok = false;
        }
        if (snap.security.failedLoginAttempts > 5) {
          result.passed = false;
          result.severity = 3;
          result.detail = L"Failed logins: " + std::to_wstring(snap.security.failedLoginAttempts);
          result.recommendations.push_back(L"Investigate failed login attempts");
          ok = false;
        }
        if (ok) {
          result.detail = L"Security checks passed";
        }
      };
      checks_.push_back(c);
    }
    {
      CheckDef c;
      c.id = L"RELIABILITY_HEALTH";
      c.name = L"Reliability Health";
      c.category = L"reliability";
      c.description = L"System reliability check";
      c.severity = 3;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        bool ok = true;
        if (snap.reliability.crashDumpDetected) {
          result.passed = false;
          result.severity = 4;
          result.detail = L"Crash dump detected — code 0x" +
            std::to_wstring(snap.reliability.lastBugCheckCode);
          result.recommendations.push_back(L"Analyze crash dump");
          result.recommendations.push_back(L"Check drivers and hardware");
          ok = false;
        }
        if (snap.reliability.hardwareErrors > 0) {
          result.passed = false;
          result.severity = 3;
          result.detail = L"Hardware errors: " + std::to_wstring(snap.reliability.hardwareErrors);
          result.recommendations.push_back(L"Run hardware diagnostics");
          ok = false;
        }
        if (snap.reliability.storageErrors > 0) {
          result.passed = false;
          result.severity = 3;
          result.detail = L"Storage errors: " + std::to_wstring(snap.reliability.storageErrors);
          result.recommendations.push_back(L"Check disk health");
          ok = false;
        }
        if (ok) {
          result.detail = L"Reliability checks passed";
        }
      };
      checks_.push_back(c);
    }
    {
      CheckDef c;
      c.id = L"POWER_HEALTH";
      c.name = L"Power Health";
      c.category = L"power";
      c.description = L"Power configuration check";
      c.severity = 2;
      c.evaluate = [](const SystemSnapshot& snap, const CorrelationEngine&,
                       const AnomalyDetector&, DiagnosticCheck& result) {
        if (snap.power.batteryPresent) {
          if (snap.power.batteryChargePct < 15.0 && !snap.power.acConnected) {
            result.passed = false;
            result.severity = 3;
            result.detail = L"Battery critical: " +
              std::to_wstring(static_cast<int>(snap.power.batteryChargePct)) + L"%";
            result.recommendations.push_back(L"Connect charger immediately");
          } else if (snap.power.batteryWearPct > 30.0) {
            result.passed = false;
            result.severity = 1;
            result.detail = L"Battery wear: " +
              std::to_wstring(static_cast<int>(snap.power.batteryWearPct)) + L"%";
            result.recommendations.push_back(L"Consider battery replacement");
          } else {
            result.detail = L"Battery: " +
              std::to_wstring(static_cast<int>(snap.power.batteryChargePct)) + L"%";
          }
        } else {
          result.detail = L"No battery detected";
        }
      };
      checks_.push_back(c);
    }
  }

  std::vector<CheckDef> checks_;
  uint64_t nextReportId_ = 1;
  mutable std::mutex mutex_;
};

}
