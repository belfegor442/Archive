#include "PowerBatteryRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <cmath>

namespace monix {

void PowerBatteryRule::Evaluate(const Snapshot& current,
                                const Snapshot* previous,
                                std::vector<ScramFinding>& findings) {
  if (!previous) {
    prevIdlePowerDrawHigh_ = current.idlePowerDrawHigh;
    prevPowerSaverActive_ = current.powerSaverActive;
    prevHighPerfActive_ = current.highPerfActive;
    return;
  }
  const Snapshot& prev = *previous;

  // 1. AC power connect
  if (prev.acLineStatus == 0 && current.acLineStatus == 1) {
    findings.push_back({
      L"AC power connected.",
      L"System has been connected to AC power.",
      L"AC line: disconnected -> connected.",
      4
    });
  }

  // 2. AC power disconnect
  if (prev.acLineStatus == 1 && current.acLineStatus == 0) {
    findings.push_back({
      L"AC power disconnected.",
      L"System has been disconnected from AC power.",
      L"AC line: connected -> disconnected.",
      6
    });
  }

  // 3. Battery present
  if (prev.batteryFlag == 128 && current.batteryFlag != 128) {
    findings.push_back({
      L"Battery detected.",
      L"A battery is now present in the system.",
      L"Battery: not present -> present.",
      2
    });
  }

  // 4. Battery removal
  if (prev.batteryFlag != 128 && current.batteryFlag == 128) {
    findings.push_back({
      L"Battery removed.",
      L"The battery is no longer detected.",
      L"Battery: present -> removed.",
      4
    });
  }

  // 5. Battery charge state
  if (current.batteryLifePercent >= 0 && current.batteryLifePercent <= 100 && prev.batteryLifePercent >= 0) {
    const int chargeDelta = current.batteryLifePercent - prev.batteryLifePercent;
    if (chargeDelta > 5) {
      findings.push_back({
        L"",
        L"",
        L"Battery charged: " + std::to_wstring(prev.batteryLifePercent) + L"% -> " + std::to_wstring(current.batteryLifePercent) + L"%.",
        2
      });
    } else if (chargeDelta < -5) {
      findings.push_back({
        L"",
        L"",
        L"Battery drained: " + std::to_wstring(prev.batteryLifePercent) + L"% -> " + std::to_wstring(current.batteryLifePercent) + L"%.",
        4
      });
    }
  }

  // 6. Battery drain rate spike
  if (current.batteryLifeTimeSec > 0 && prev.batteryLifeTimeSec > 0 && current.acLineStatus == 0) {
    if (prev.batteryLifeTimeSec > current.batteryLifeTimeSec + 600) {
      findings.push_back({
        L"Battery drain rate spike.",
        L"Battery remaining time dropped by more than 10 minutes between samples.",
        L"Drain: " + std::to_wstring(prev.batteryLifeTimeSec / 60) + L"min -> " + std::to_wstring(current.batteryLifeTimeSec / 60) + L"min.",
        10
      });
    }
  }

  // 7. Battery wear level
  if (current.batteryWearLevel >= 0 && current.batteryWearLevel != prev.batteryWearLevel && prev.batteryWearLevel >= 0) {
    if (current.batteryWearLevel > 30) {
      findings.push_back({
        L"Battery wear level elevated.",
        L"Battery wear has increased, indicating reduced capacity.",
        L"Wear: " + std::to_wstring(prev.batteryWearLevel) + L"% -> " + std::to_wstring(current.batteryWearLevel) + L"%.",
        8
      });
    }
  }

  // 8. Battery cycle count
  if (current.batteryCycleCount >= 0 && current.batteryCycleCount != prev.batteryCycleCount && prev.batteryCycleCount >= 0) {
    const int cycleDelta = current.batteryCycleCount - prev.batteryCycleCount;
    if (cycleDelta > 0) {
      findings.push_back({
        L"",
        L"",
        L"Battery cycles: +" + std::to_wstring(cycleDelta) + L" (total: " + std::to_wstring(current.batteryCycleCount) + L").",
        2
      });
    }
  }

  // 9. Charging speed change
  if (current.batteryChargeRate != prev.batteryChargeRate && current.batteryChargeRate != 0 && prev.batteryChargeRate != 0) {
    if (current.batteryChargeRate < prev.batteryChargeRate * 0.5) {
      findings.push_back({
        L"Charging speed dropped.",
        L"Battery charge rate has halved, possibly due to thermal throttling.",
        L"Charge rate: " + std::to_wstring(prev.batteryChargeRate) + L" -> " + std::to_wstring(current.batteryChargeRate) + L" mW.",
        10
      });
    } else if (current.batteryChargeRate > prev.batteryChargeRate * 2) {
      findings.push_back({
        L"",
        L"",
        L"Charging speed increased: " + std::to_wstring(prev.batteryChargeRate) + L" -> " + std::to_wstring(current.batteryChargeRate) + L" mW.",
        2
      });
    }
  }

  // 10. Fast charge state
  if (current.batteryChargeRate > 40000 && prev.batteryChargeRate <= 40000) {
    findings.push_back({
      L"Fast charge state detected.",
      L"Battery charge rate exceeds 40W, indicating fast charging.",
      L"Fast charge: " + std::to_wstring(current.batteryChargeRate) + L" mW.",
      2
    });
  }

  // 11. Idle power draw rise
  if (current.idlePowerDrawHigh == 1 && prevIdlePowerDrawHigh_ == 0) {
    findings.push_back({
      L"Idle power draw elevated.",
      L"System is on battery with low CPU but draining quickly, indicating background activity.",
      L"Idle drain: battery " + std::to_wstring(current.batteryLifeTimeSec / 60) + L"min remaining at " + std::to_wstring((int)current.cpuPct) + L"% CPU.",
      10
    });
  }

  // 12. Suspended power state
  if (current.sleepStateActive == 1 && prev.sleepStateActive == 0) {
    findings.push_back({
      L"Suspended power state.",
      L"System has entered a suspended state.",
      L"Power state: suspended.",
      2
    });
  }

  // 13. Modern standby entry
  if (current.modernStandbyActive == 1 && prev.modernStandbyActive != 1) {
    findings.push_back({
      L"Modern standby entry.",
      L"System has entered Modern Standby (S0 Low Power Idle).",
      L"Standby: entered Modern Standby.",
      2
    });
  }

  // 14. Modern standby exit
  if (current.modernStandbyActive == 0 && prev.modernStandbyActive == 1) {
    findings.push_back({
      L"Modern standby exit.",
      L"System has exited Modern Standby.",
      L"Standby: exited Modern Standby.",
      2
    });
  }

  // 15. Sleep state entry (removed: duplicate of rule 12)

  // 16. Sleep state exit
  if (current.sleepStateActive == 0 && prev.sleepStateActive == 1) {
    findings.push_back({
      L"Sleep state exit.",
      L"System has exited sleep state.",
      L"Sleep: exited sleep state.",
      2
    });
  }

  // 17. Hibernate entry
  if (current.hibernateActive == 1 && prev.hibernateActive == 0) {
    findings.push_back({
      L"Hibernate entry.",
      L"System is entering hibernation.",
      L"Hibernate: entering hibernation.",
      2
    });
  }

  // 18. Hibernate exit
  if (current.hibernateActive == 0 && prev.hibernateActive == 1) {
    findings.push_back({
      L"Hibernate exit.",
      L"System has exited hibernation.",
      L"Hibernate: exited hibernation.",
      2
    });
  }

  // 19. Power plan change
  if (current.powerPlanIndex != prev.powerPlanIndex && prev.powerPlanIndex >= 0 && prev.powerPlanIndex <= 2 && current.powerPlanIndex >= 0 && current.powerPlanIndex <= 2) {
    const wchar_t* plans[] = { L"Power Saver", L"Balanced", L"High Performance" };
    findings.push_back({
      L"Power plan changed.",
      L"Active power plan has been modified.",
      L"Power plan: " + std::wstring(plans[prev.powerPlanIndex]) + L" -> " + std::wstring(plans[current.powerPlanIndex]) + L".",
      8
    });
  }

  // 20. Power saver mode
  if (current.powerSaverActive == 1 && prevPowerSaverActive_ == 0) {
    findings.push_back({
      L"Power saver mode activated.",
      L"System has switched to power saving mode.",
      L"Power plan: Power Saver activated.",
      2
    });
  }

  // 21. High performance mode
  if (current.highPerfActive == 1 && prevHighPerfActive_ == 0) {
    findings.push_back({
      L"High performance mode activated.",
      L"System has switched to high performance mode, increasing power draw.",
      L"Power plan: High Performance activated.",
      8
    });
  }

  // 22. Balanced mode
  if (current.balancedActive == 1 && prev.highPerfActive == 1) {
    findings.push_back({
      L"Balanced mode activated.",
      L"System switched from High Performance to Balanced.",
      L"Power plan: Balanced mode restored.",
      2
    });
  }

  // 23. CPU package power cap
  if (current.cpuPackagePowerCap >= 0 && current.cpuPackagePowerCap != prev.cpuPackagePowerCap && prev.cpuPackagePowerCap >= 0) {
    if (current.cpuPackagePowerCap < prev.cpuPackagePowerCap) {
      findings.push_back({
        L"CPU package power cap reduced.",
        L"CPU power cap has been lowered, limiting maximum CPU performance.",
        L"CPU cap: " + std::to_wstring(prev.cpuPackagePowerCap) + L"% -> " + std::to_wstring(current.cpuPackagePowerCap) + L"%.",
        8
      });
    } else {
      findings.push_back({
        L"",
        L"",
        L"CPU cap: " + std::to_wstring(prev.cpuPackagePowerCap) + L"% -> " + std::to_wstring(current.cpuPackagePowerCap) + L"%.",
        2
      });
    }
  }

  // 24. System-wide power cap
  if (current.systemPowerCap >= 0 && current.systemPowerCap != prev.systemPowerCap && prev.systemPowerCap >= 0) {
    findings.push_back({
      L"System-wide power cap changed.",
      L"System power limit has been modified.",
      L"System cap: " + std::to_wstring(prev.systemPowerCap) + L"% -> " + std::to_wstring(current.systemPowerCap) + L"%.",
      8
    });
  }

  // 25. Battery temperature rise
  if (current.batteryTemperature >= 0 && prev.batteryTemperature >= 0) {
    if (current.batteryTemperature > prev.batteryTemperature + 10) {
      findings.push_back({
        L"Battery temperature rising.",
        L"Battery temperature has increased by more than 10C.",
        L"Battery temp: " + std::to_wstring(prev.batteryTemperature) + L"C -> " + std::to_wstring(current.batteryTemperature) + L"C.",
        10
      });
    }
  }

  prevIdlePowerDrawHigh_ = current.idlePowerDrawHigh;
  prevPowerSaverActive_ = current.powerSaverActive;
  prevHighPerfActive_ = current.highPerfActive;
}

} // namespace monix
