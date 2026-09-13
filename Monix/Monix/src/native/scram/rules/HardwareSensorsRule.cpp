#include "HardwareSensorsRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <cmath>

namespace monix {

void HardwareSensorsRule::Evaluate(const Snapshot& current,
                                   const Snapshot* previous,
                                   std::vector<ScramFinding>& findings) {
  if (!previous) {
    prevSensorPollFailures_ = current.sensorPollFailures;
    return;
  }
  const Snapshot& prev = *previous;

  // 1. Embedded controller event
  if (current.ecEvents > prev.ecEvents) {
    findings.push_back({
      L"Embedded controller event detected.",
      L"Embedded controller event count increased, indicating low-level hardware event.",
      L"EC events: +" + std::to_wstring(current.ecEvents - prev.ecEvents) + L".",
      8
    });
  }

  // 2. Voltage rail anomaly (12V)
  if (current.voltage12V > 0 && prev.voltage12V > 0) {
    const double delta12 = std::abs(current.voltage12V - prev.voltage12V);
    if (delta12 > 0.5) {
      findings.push_back({
        L"Voltage rail anomaly detected.",
        L"12V rail voltage fluctuated by more than 0.5V between samples.",
        L"12V: " + std::to_wstring((int)(prev.voltage12V * 1000)) + L"mV -> " + std::to_wstring((int)(current.voltage12V * 1000)) + L"mV.",
        12
      });
    }
  }

  // 3. 12V rail drop
  if (current.voltage12V > 0 && prev.voltage12V > 0 && current.voltage12V < prev.voltage12V - 0.3) {
    findings.push_back({
      L"12V rail voltage drop.",
      L"12V supply voltage dropped by more than 300mV, indicating power supply stress.",
      L"12V drop: " + std::to_wstring((int)(prev.voltage12V * 1000)) + L" -> " + std::to_wstring((int)(current.voltage12V * 1000)) + L" mV.",
      14
    });
  }

  // 4. 5V rail drop
  if (current.voltage5V > 0 && prev.voltage5V > 0 && current.voltage5V < prev.voltage5V - 0.2) {
    findings.push_back({
      L"5V rail voltage drop.",
      L"5V supply voltage dropped by more than 200mV.",
      L"5V drop: " + std::to_wstring((int)(prev.voltage5V * 1000)) + L" -> " + std::to_wstring((int)(current.voltage5V * 1000)) + L" mV.",
      14
    });
  }

  // 5. 3.3V rail drop
  if (current.voltage33V > 0 && prev.voltage33V > 0 && current.voltage33V < prev.voltage33V - 0.15) {
    findings.push_back({
      L"3.3V rail voltage drop.",
      L"3.3V supply voltage dropped by more than 150mV.",
      L"3.3V drop: " + std::to_wstring((int)(prev.voltage33V * 1000)) + L" -> " + std::to_wstring((int)(current.voltage33V * 1000)) + L" mV.",
      12
    });
  }

  // 6. Vcore fluctuation
  if (current.voltageVcore > 0 && prev.voltageVcore > 0) {
    const double vcoreDelta = std::abs(current.voltageVcore - prev.voltageVcore);
    if (vcoreDelta > 0.1) {
      findings.push_back({
        L"Vcore voltage fluctuation.",
        L"CPU core voltage fluctuated by more than 100mV, indicating VRM instability.",
        L"Vcore: " + std::to_wstring((int)(prev.voltageVcore * 1000)) + L" -> " + std::to_wstring((int)(current.voltageVcore * 1000)) + L" mV.",
        10
      });
    }
  }

  // 7. VRAM voltage fluctuation
  if (current.voltageVram > 0 && prev.voltageVram > 0) {
    const double vramDelta = std::abs(current.voltageVram - prev.voltageVram);
    if (vramDelta > 0.05) {
      findings.push_back({
        L"VRAM voltage fluctuation.",
        L"Memory/VRAM voltage fluctuated by more than 50mV.",
        L"VRAM V: " + std::to_wstring((int)(prev.voltageVram * 1000)) + L" -> " + std::to_wstring((int)(current.voltageVram * 1000)) + L" mV.",
        10
      });
    }
  }

  // 8. Chipset sensor fault
  if (current.sensorPollFailures > prevSensorPollFailures_ && current.sensorPollFailures > 0) {
    findings.push_back({
      L"Chipset sensor fault.",
      L"Sensor polling failures have increased, indicating chipset sensor issues.",
      L"Sensor faults: " + std::to_wstring(prevSensorPollFailures_) + L" -> " + std::to_wstring(current.sensorPollFailures) + L".",
      14
    });
  }

  // 9. Motherboard sensor fault
  if (current.boardTempHotspot > 0 && prev.boardTempHotspot > 0) {
    const double boardDelta = std::abs(current.boardTempHotspot - prev.boardTempHotspot);
    if (boardDelta > 15.0) {
      findings.push_back({
        L"Motherboard sensor fault.",
        L"Board temperature reading jumped by more than 15C, indicating sensor anomaly.",
        L"Board temp: " + std::to_wstring((int)prev.boardTempHotspot) + L"C -> " + std::to_wstring((int)current.boardTempHotspot) + L"C.",
        14
      });
    }
  }

  // 10. PCIe bus error
  if (current.pcieErrors > prev.pcieErrors) {
    findings.push_back({
      L"PCIe bus error detected.",
      L"PCIe bus error count increased, indicating link instability.",
      L"PCIe errors: +" + std::to_wstring(current.pcieErrors - prev.pcieErrors) + L".",
      12
    });
  }

  // 11. USB controller reset
  if (current.usbResets > prev.usbResets) {
    findings.push_back({
      L"USB controller reset detected.",
      L"USB controller has been reset, indicating USB device or controller instability.",
      L"USB resets: +" + std::to_wstring(current.usbResets - prev.usbResets) + L".",
      8
    });
  }

  // 12. SATA controller reset
  if (current.sataResets > prev.sataResets) {
    findings.push_back({
      L"SATA controller reset detected.",
      L"SATA controller has been reset, indicating storage link instability.",
      L"SATA resets: +" + std::to_wstring(current.sataResets - prev.sataResets) + L".",
      10
    });
  }

  // 13. Thunderbolt controller event
  if (current.thunderboltEvents > prev.thunderboltEvents) {
    findings.push_back({
      L"Thunderbolt controller event.",
      L"Thunderbolt controller event detected, indicating dock or device connectivity change.",
      L"TB events: +" + std::to_wstring(current.thunderboltEvents - prev.thunderboltEvents) + L".",
      6
    });
  }

  // 14. TPM state change
  if (current.tpmReady != prev.tpmReady && prev.tpmPresent == 1) {
    findings.push_back({
      L"TPM state change detected.",
      L"Trusted Platform Module readiness state has changed.",
      L"TPM: " + std::wstring(prev.tpmReady ? L"ready" : L"not ready") + L" -> " + std::wstring(current.tpmReady ? L"ready" : L"not ready") + L".",
      14
    });
  }

  // 15. TPM failure
  if (prev.tpmReady == 1 && current.tpmReady == 0) {
    findings.push_back({
      L"TPM failure detected.",
      L"Trusted Platform Module has become unavailable, affecting security features.",
      L"TPM failed: ready -> not ready.",
      18
    });
  }

  // 16. RTC drift
  if (prev.systemTime100ns > 0 && current.systemTime100ns > 0) {
    const long long timeDelta = static_cast<long long>(current.systemTime100ns - prev.systemTime100ns);
    const long long expectedDelta = (current.uptimeMs - prev.uptimeMs) * 10000LL;
    const long long drift = timeDelta - expectedDelta;
    if (std::abs(drift) > 50000000LL && std::abs(drift) < 100000000LL) {
      findings.push_back({
        L"RTC drift detected.",
        L"Real-time clock has drifted by more than 5 seconds from expected time.",
        L"RTC drift: " + std::to_wstring(drift / 10000) + L"ms.",
        10
      });
    }
  }

  // 17. CMOS battery low
  if (current.cmosBatteryOk == 0 && prev.cmosBatteryOk == 1) {
    findings.push_back({
      L"CMOS battery low.",
      L"CMOS battery voltage is low, settings may be lost on power cycle.",
      L"CMOS battery: OK -> low.",
      8
    });
  }

  // 18. Sensor polling failure
  if (current.sensorPollFailures > prevSensorPollFailures_) {
    findings.push_back({
      L"Sensor polling failure.",
      L"Hardware sensor polling has failed, reducing monitoring coverage.",
      L"Poll failures: " + std::to_wstring(prevSensorPollFailures_) + L" -> " + std::to_wstring(current.sensorPollFailures) + L".",
      10
    });
  }

  // 19. Sensor calibration change
  if (current.voltageVcore > 0 && prev.voltageVcore > 0) {
    const double vcoreRatio = current.voltageVcore / prev.voltageVcore;
    if (vcoreRatio > 1.1 || vcoreRatio < 0.9) {
      findings.push_back({
        L"Sensor calibration change.",
        L"Voltage reading shifted by more than 10%, possibly indicating sensor drift or calibration change.",
        L"Vcore ratio: " + std::to_wstring((int)(vcoreRatio * 100)) + L"%.",
        10
      });
    }
  }

  // 20. Board temperature hotspot (transition-based)
  if (current.boardTempHotspot > 60.0 && current.motherboardTempC > 0
      && current.boardTempHotspot > current.motherboardTempC + 15.0
      && (prev.boardTempHotspot <= 60.0 || prev.boardTempHotspot <= prev.motherboardTempC + 15.0)) {
    findings.push_back({
      L"Board temperature hotspot.",
      L"Board temperature hotspot is significantly higher than average board temperature.",
      L"Hotspot: " + std::to_wstring((int)current.boardTempHotspot) + L"C vs avg " + std::to_wstring((int)current.motherboardTempC) + L"C.",
      10
    });
  }

  // 21. Hardware watchdog event
  if (current.uptimeMs < prev.uptimeMs && prev.uptimeMs > 60000) {
    findings.push_back({
      L"Hardware watchdog event suspected.",
      L"Unexpected system reboot may have been triggered by hardware watchdog timer.",
      L"Reboot: uptime " + std::to_wstring(prev.uptimeMs / 1000) + L"s -> " + std::to_wstring(current.uptimeMs / 1000) + L"s.",
      20
    });
  }

  prevSensorPollFailures_ = current.sensorPollFailures;
}

} // namespace monix
