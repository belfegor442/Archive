#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "../core/SessionManager.hpp"
#include "../core/snapshot/SystemSnapshot.hpp"
#include "../events/SystemEvent.hpp"
#include "../scram/ScramEngine.hpp"
#include "../core/Timeline.hpp"

namespace monix {

struct SessionSaveData {
  SessionMetadata metadata;
  std::vector<SystemSnapshot> snapshots;
  std::vector<SystemEvent> events;
  std::vector<ScramResult> findings;
};

class SessionPersistence {
public:
  explicit SessionPersistence(const std::wstring& basePath) : basePath_(basePath) {
    std::filesystem::create_directories(basePath);
  }

  bool SaveSession(const Session& session) {
    std::wstring path = basePath_ + L"\\" +
      session.metadata.id + L".bin";
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wb");
    if (!f) return false;

    WriteMetadata(f, session.metadata);
    WriteSnapshots(f, session.snapshots);
    WriteEvents(f, session.events);
    WriteFindings(f, session.findings);

    fclose(f);
    return true;
  }

  SessionSaveData LoadSession(const std::wstring& sessionId) {
    SessionSaveData data;
    std::wstring path = basePath_ + L"\\" + sessionId + L".bin";
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"rb");
    if (!f) return data;

    data.metadata = ReadMetadata(f);
    data.snapshots = ReadSnapshots(f);
    data.events = ReadEvents(f);
    data.findings = ReadFindings(f);

    fclose(f);
    return data;
  }

  std::vector<SessionMetadata> ListSessions() {
    std::vector<SessionMetadata> sessions;
    for (const auto& entry : std::filesystem::directory_iterator(basePath_)) {
      if (entry.path().extension() == L".bin") {
        FILE* f = nullptr;
        _wfopen_s(&f, entry.path().c_str(), L"rb");
        if (f) {
          sessions.push_back(ReadMetadata(f));
          fclose(f);
        }
      }
    }
    return sessions;
  }

  bool DeleteSession(const std::wstring& sessionId) {
    std::wstring path = basePath_ + L"\\" + sessionId + L".bin";
    return std::filesystem::remove(path);
  }

  bool ExportSession(const Session& session, const std::wstring& path) {
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wb");
    if (!f) return false;

    WriteMetadata(f, session.metadata);
    WriteSnapshots(f, session.snapshots);
    WriteEvents(f, session.events);
    WriteFindings(f, session.findings);

    fclose(f);
    return true;
  }

private:
  static void WriteWString(FILE* f, const std::wstring& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    fwrite(&len, sizeof(uint32_t), 1, f);
    fwrite(s.data(), sizeof(wchar_t), len, f);
  }

  static std::wstring ReadWString(FILE* f) {
    uint32_t len = 0;
    if (fread(&len, sizeof(uint32_t), 1, f) != 1) return {};
    std::wstring s(len, L'\0');
    if (fread(&s[0], sizeof(wchar_t), len, f) != len) return {};
    return s;
  }

  static void WriteMetadata(FILE* f, const SessionMetadata& m) {
    WriteWString(f, m.id);
    WriteWString(f, m.name);
    fwrite(&m.startTimeNs, sizeof(uint64_t), 1, f);
    fwrite(&m.endTimeNs, sizeof(uint64_t), 1, f);
    WriteWString(f, m.machineName);
    WriteWString(f, m.osVersion);
    WriteWString(f, m.monixVersion);
    fwrite(&m.totalSnapshots, sizeof(int), 1, f);
    fwrite(&m.totalEvents, sizeof(int), 1, f);
    fwrite(&m.totalFindings, sizeof(int), 1, f);
    fwrite(&m.peakRiskScore, sizeof(int), 1, f);
    WriteWString(f, m.notes);
  }

  static SessionMetadata ReadMetadata(FILE* f) {
    SessionMetadata m;
    m.id = ReadWString(f);
    m.name = ReadWString(f);
    if (fread(&m.startTimeNs, sizeof(uint64_t), 1, f) != 1) return m;
    if (fread(&m.endTimeNs, sizeof(uint64_t), 1, f) != 1) return m;
    m.machineName = ReadWString(f);
    m.osVersion = ReadWString(f);
    m.monixVersion = ReadWString(f);
    if (fread(&m.totalSnapshots, sizeof(int), 1, f) != 1) return m;
    if (fread(&m.totalEvents, sizeof(int), 1, f) != 1) return m;
    if (fread(&m.totalFindings, sizeof(int), 1, f) != 1) return m;
    if (fread(&m.peakRiskScore, sizeof(int), 1, f) != 1) return m;
    m.notes = ReadWString(f);
    return m;
  }

  static void WriteSnapshots(FILE* f, const std::vector<SystemSnapshot>& snaps) {
    uint32_t count = static_cast<uint32_t>(snaps.size());
    fwrite(&count, sizeof(uint32_t), 1, f);
    for (const auto& s : snaps) {
      fwrite(&s.id, sizeof(uint64_t), 1, f);
      fwrite(&s.timestampNs, sizeof(uint64_t), 1, f);
      fwrite(&s.cpu.pct, sizeof(double), 1, f);
      fwrite(&s.cpu.cores, sizeof(double) * 64, 1, f);
      fwrite(&s.cpu.packagePowerW, sizeof(double), 1, f);
      fwrite(&s.memory.usedBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.memory.totalBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.memory.compressedBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.memory.pageFaultsPerSec, sizeof(double), 1, f);
      fwrite(&s.gpu.pct, sizeof(double), 1, f);
      fwrite(&s.gpu.tempC, sizeof(double), 1, f);
      fwrite(&s.gpu.powerW, sizeof(double), 1, f);
      fwrite(&s.gpu.usedBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.gpu.totalBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.thermal.cpuCoreTempC, sizeof(double), 1, f);
      fwrite(&s.thermal.cpuPackageTempC, sizeof(double), 1, f);
      fwrite(&s.thermal.gpuTempC, sizeof(double), 1, f);
      fwrite(&s.thermal.ssdTempC, sizeof(double), 1, f);
      fwrite(&s.thermal.fanCount, sizeof(int), 1, f);
      fwrite(&s.thermal.fanSpeeds.data(), sizeof(double) * 10, 1, f);
      fwrite(&s.thermal.cpuThrottling, sizeof(int), 1, f);
      fwrite(&s.thermal.thermalLimitReason, sizeof(uint32_t), 1, f);
      fwrite(&s.processes.count, sizeof(int), 1, f);
      fwrite(&s.processes.threadCount, sizeof(int), 1, f);
      fwrite(&s.processes.handleCount, sizeof(int), 1, f);
      fwrite(&s.processes.ramUsedBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.processes.cpuPct, sizeof(double), 1, f);
      fwrite(&s.processes.iops, sizeof(double), 1, f);
      fwrite(&s.processes.netBytesSec, sizeof(double), 1, f);
      uint32_t pcount = static_cast<uint32_t>(s.processes.list.size());
      fwrite(&pcount, sizeof(uint32_t), 1, f);
      for (const auto& p : s.processes.list) {
        fwrite(&p.pid, sizeof(uint32_t), 1, f);
        fwrite(&p.cpuPct, sizeof(double), 1, f);
        fwrite(&p.ramBytes, sizeof(uint64_t), 1, f);
        fwrite(&p.threadCount, sizeof(int), 1, f);
      }
      fwrite(&s.network.upKbps, sizeof(double), 1, f);
      fwrite(&s.network.downKbps, sizeof(double), 1, f);
      fwrite(&s.network.rttMs, sizeof(double), 1, f);
      fwrite(&s.network.activeConns, sizeof(int), 1, f);
      fwrite(&s.network.droppedPktPct, sizeof(double), 1, f);
      fwrite(&s.network.tcpRetransmitPct, sizeof(double), 1, f);
      fwrite(&s.storage.diskReadMBs, sizeof(double), 1, f);
      fwrite(&s.storage.diskWriteMBs, sizeof(double), 1, f);
      fwrite(&s.storage.iops, sizeof(double), 1, f);
      fwrite(&s.storage.queueDepth, sizeof(double), 1, f);
      fwrite(&s.storage.activeTimePct, sizeof(double), 1, f);
      fwrite(&s.storage.ready, sizeof(int), 1, f);
      fwrite(&s.storage.wearPct, sizeof(double), 1, f);
      fwrite(&s.storage.tempC, sizeof(double), 1, f);
      fwrite(&s.storage.totalBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.storage.usedBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.storage.freeBytes, sizeof(uint64_t), 1, f);
      fwrite(&s.storage.readPct, sizeof(double), 1, f);
      fwrite(&s.storage.writePct, sizeof(double), 1, f);
      fwrite(&s.security.defenderRealtimePct, sizeof(double), 1, f);
      fwrite(&s.security.defenderScanStatus, sizeof(int), 1, f);
      fwrite(&s.security.defenderThreatsDetected, sizeof(int), 1, f);
      fwrite(&s.security.lastFullScanAgeHours, sizeof(double), 1, f);
      fwrite(&s.security.rdpEnabled, sizeof(int), 1, f);
      fwrite(&s.security.sshEnabled, sizeof(int), 1, f);
      fwrite(&s.security.uptimeHours, sizeof(double), 1, f);
      fwrite(&s.security.failedLoginAttempts, sizeof(int), 1, f);
      fwrite(&s.security.enabledUsers, sizeof(int), 1, f);
      fwrite(&s.security.activeAdmins, sizeof(int), 1, f);
      fwrite(&s.security.lastRebootReason, sizeof(int), 1, f);
      fwrite(&s.security.crashesToday, sizeof(int), 1, f);
      fwrite(&s.security.biosVersion, sizeof(wchar_t) * 64, 1, f);
      fwrite(&s.security.driversLoaded, sizeof(int), 1, f);
      fwrite(&s.security.bootConfigValid, sizeof(int), 1, f);
      fwrite(&s.security.evBadDriver7Days, sizeof(int), 1, f);
      fwrite(&s.power.source, sizeof(int), 1, f);
      fwrite(&s.power.batteryPresent, sizeof(int), 1, f);
      fwrite(&s.power.batteryChargePct, sizeof(double), 1, f);
      fwrite(&s.power.batteryChargeCycles, sizeof(int), 1, f);
      fwrite(&s.power.batteryCapacityMWh, sizeof(double), 1, f);
      fwrite(&s.power.batteryWearPct, sizeof(double), 1, f);
      fwrite(&s.power.batteryFullLifetimeS, sizeof(double), 1, f);
      fwrite(&s.power.batteryTimeToEmptyS, sizeof(double), 1, f);
      fwrite(&s.power.batteryTimeToFullS, sizeof(double), 1, f);
      fwrite(&s.power.acConnected, sizeof(int), 1, f);
      fwrite(&s.power.acOnline, sizeof(int), 1, f);
      fwrite(&s.power.acMaxWatts, sizeof(double), 1, f);
      fwrite(&s.power.cpuTdpW, sizeof(double), 1, f);
      fwrite(&s.power.systemLoadW, sizeof(double), 1, f);
      fwrite(&s.power.totalPowerW, sizeof(double), 1, f);
      fwrite(&s.power.psuTemperatureC, sizeof(double), 1, f);
      fwrite(&s.power.systemPowerCapW, sizeof(double), 1, f);
      fwrite(&s.power.batteryDischargeRateW, sizeof(double), 1, f);
      fwrite(&s.power.charging, sizeof(int), 1, f);
      fwrite(&s.power.timeOnBatteryS, sizeof(double), 1, f);
      fwrite(&s.power.timeOnAcS, sizeof(double), 1, f);
      fwrite(&s.power.lastWakeReason, sizeof(int), 1, f);
      fwrite(&s.power.cpusActive, sizeof(int), 1, f);
      fwrite(&s.reliability.biosHealthStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.bootHealthStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.bootHealthSummary, sizeof(int), 1, f);
      fwrite(&s.reliability.bootUptimeStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.lastBootStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.lastShutdownStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.lastShutdownReason, sizeof(int), 1, f);
      fwrite(&s.reliability.lastShutdownTimeNs, sizeof(uint64_t), 1, f);
      fwrite(&s.reliability.crashDumpDetected, sizeof(int), 1, f);
      fwrite(&s.reliability.lastCrashTimeNs, sizeof(uint64_t), 1, f);
      fwrite(&s.reliability.lastBugCheckCode, sizeof(int), 1, f);
      fwrite(&s.reliability.bugCheckTimeNs, sizeof(uint64_t), 1, f);
      fwrite(&s.reliability.lastBugCheckTimeNs, sizeof(uint64_t), 1, f);
      fwrite(&s.reliability.agedCrashes, sizeof(int) * 16, 1, f);
      fwrite(&s.reliability.lastUpdateRebootStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.secureBootEnabled, sizeof(int), 1, f);
      fwrite(&s.reliability.deviceGuardStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.hardwareErrors, sizeof(int), 1, f);
      fwrite(&s.reliability.badMemoryErrors, sizeof(int), 1, f);
      fwrite(&s.reliability.hyperVStatus, sizeof(int), 1, f);
      fwrite(&s.reliability.totalMemoryErrors, sizeof(int), 1, f);
      fwrite(&s.reliability.storageErrors, sizeof(int), 1, f);
      fwrite(&s.reliability.dpcTimeS, sizeof(double), 1, f);
      fwrite(&s.reliability.isrTimeS, sizeof(double), 1, f);
      fwrite(&s.reliability.dpcCount, sizeof(double), 1, f);
      fwrite(&s.reliability.isrCount, sizeof(double), 1, f);
    }
  }

  static std::vector<SystemSnapshot> ReadSnapshots(FILE* f) {
    uint32_t count = 0;
    if (fread(&count, sizeof(uint32_t), 1, f) != 1) return {};
    std::vector<SystemSnapshot> snaps;
    snaps.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      SystemSnapshot s;
      if (fread(&s.id, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.timestampNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.cpu.pct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.cpu.cores, sizeof(double) * 64, 1, f) != 1) break;
      if (fread(&s.cpu.packagePowerW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.memory.usedBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.memory.totalBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.memory.compressedBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.memory.pageFaultsPerSec, sizeof(double), 1, f) != 1) break;
      if (fread(&s.gpu.pct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.gpu.tempC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.gpu.powerW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.gpu.usedBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.gpu.totalBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.thermal.cpuCoreTempC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.thermal.cpuPackageTempC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.thermal.gpuTempC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.thermal.ssdTempC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.thermal.fanCount, sizeof(int), 1, f) != 1) break;
      if (fread(&s.thermal.fanSpeeds.data(), sizeof(double) * 10, 1, f) != 1) break;
      if (fread(&s.thermal.cpuThrottling, sizeof(int), 1, f) != 1) break;
      if (fread(&s.thermal.thermalLimitReason, sizeof(uint32_t), 1, f) != 1) break;
      if (fread(&s.processes.count, sizeof(int), 1, f) != 1) break;
      if (fread(&s.processes.threadCount, sizeof(int), 1, f) != 1) break;
      if (fread(&s.processes.handleCount, sizeof(int), 1, f) != 1) break;
      if (fread(&s.processes.ramUsedBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.processes.cpuPct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.processes.iops, sizeof(double), 1, f) != 1) break;
      if (fread(&s.processes.netBytesSec, sizeof(double), 1, f) != 1) break;
      uint32_t pcount = 0;
      if (fread(&pcount, sizeof(uint32_t), 1, f) != 1) break;
      s.processes.list.resize(pcount);
      for (uint32_t j = 0; j < pcount; ++j) {
        if (fread(&s.processes.list[j].pid, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&s.processes.list[j].cpuPct, sizeof(double), 1, f) != 1) break;
        if (fread(&s.processes.list[j].ramBytes, sizeof(uint64_t), 1, f) != 1) break;
        if (fread(&s.processes.list[j].threadCount, sizeof(int), 1, f) != 1) break;
      }
      if (fread(&s.network.upKbps, sizeof(double), 1, f) != 1) break;
      if (fread(&s.network.downKbps, sizeof(double), 1, f) != 1) break;
      if (fread(&s.network.rttMs, sizeof(double), 1, f) != 1) break;
      if (fread(&s.network.activeConns, sizeof(int), 1, f) != 1) break;
      if (fread(&s.network.droppedPktPct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.network.tcpRetransmitPct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.diskReadMBs, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.diskWriteMBs, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.iops, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.queueDepth, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.activeTimePct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.ready, sizeof(int), 1, f) != 1) break;
      if (fread(&s.storage.wearPct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.tempC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.totalBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.storage.usedBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.storage.freeBytes, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.storage.readPct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.storage.writePct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.security.defenderRealtimePct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.security.defenderScanStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.defenderThreatsDetected, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.lastFullScanAgeHours, sizeof(double), 1, f) != 1) break;
      if (fread(&s.security.rdpEnabled, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.sshEnabled, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.uptimeHours, sizeof(double), 1, f) != 1) break;
      if (fread(&s.security.failedLoginAttempts, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.enabledUsers, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.activeAdmins, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.lastRebootReason, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.crashesToday, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.biosVersion, sizeof(wchar_t) * 64, 1, f) != 1) break;
      if (fread(&s.security.driversLoaded, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.bootConfigValid, sizeof(int), 1, f) != 1) break;
      if (fread(&s.security.evBadDriver7Days, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.source, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.batteryPresent, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.batteryChargePct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.batteryChargeCycles, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.batteryCapacityMWh, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.batteryWearPct, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.batteryFullLifetimeS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.batteryTimeToEmptyS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.batteryTimeToFullS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.acConnected, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.acOnline, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.acMaxWatts, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.cpuTdpW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.systemLoadW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.totalPowerW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.psuTemperatureC, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.systemPowerCapW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.batteryDischargeRateW, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.charging, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.timeOnBatteryS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.timeOnAcS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.power.lastWakeReason, sizeof(int), 1, f) != 1) break;
      if (fread(&s.power.cpusActive, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.biosHealthStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.bootHealthStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.bootHealthSummary, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.bootUptimeStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.lastBootStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.lastShutdownStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.lastShutdownReason, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.lastShutdownTimeNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.reliability.crashDumpDetected, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.lastCrashTimeNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.reliability.lastBugCheckCode, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.bugCheckTimeNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.reliability.lastBugCheckTimeNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&s.reliability.agedCrashes, sizeof(int) * 16, 1, f) != 1) break;
      if (fread(&s.reliability.lastUpdateRebootStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.secureBootEnabled, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.deviceGuardStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.hardwareErrors, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.badMemoryErrors, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.hyperVStatus, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.totalMemoryErrors, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.storageErrors, sizeof(int), 1, f) != 1) break;
      if (fread(&s.reliability.dpcTimeS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.reliability.isrTimeS, sizeof(double), 1, f) != 1) break;
      if (fread(&s.reliability.dpcCount, sizeof(double), 1, f) != 1) break;
      if (fread(&s.reliability.isrCount, sizeof(double), 1, f) != 1) break;
      snaps.push_back(std::move(s));
    }
    return snaps;
  }

  static void WriteEvents(FILE* f, const std::vector<SystemEvent>& events) {
    uint32_t count = static_cast<uint32_t>(events.size());
    fwrite(&count, sizeof(uint32_t), 1, f);
    for (const auto& e : events) {
      fwrite(&e.id, sizeof(uint64_t), 1, f);
      fwrite(&e.timestampNs, sizeof(uint64_t), 1, f);
      uint8_t cat = static_cast<uint8_t>(e.category);
      fwrite(&cat, 1, 1, f);
      uint8_t sev = static_cast<uint8_t>(e.severity);
      fwrite(&sev, 1, 1, f);
      fwrite(&e.uiActionable, sizeof(int), 1, f);
      fwrite(&e.processId, sizeof(int), 1, f);
      fwrite(&e.processIdCause, sizeof(int), 1, f);
      fwrite(&e.snapshotIdBefore, sizeof(uint64_t), 1, f);
      fwrite(&e.snapshotIdAfter, sizeof(uint64_t), 1, f);
      fwrite(&e.sessionEventIndex, sizeof(int), 1, f);
      fwrite(&e.ruleId, sizeof(int), 1, f);
      WriteWString(f, e.type);
      WriteWString(f, e.subsystem);
      WriteWString(f, e.description);
      WriteWString(f, e.metadata);
      WriteWString(f, e.correlationId);
      WriteWString(f, e.processName);
      WriteWString(f, e.causeEntityId);
      WriteWString(f, e.lastSnapshotId);
    }
  }

  static std::vector<SystemEvent> ReadEvents(FILE* f) {
    uint32_t count = 0;
    if (fread(&count, sizeof(uint32_t), 1, f) != 1) return {};
    std::vector<SystemEvent> events;
    events.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      SystemEvent e;
      if (fread(&e.id, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&e.timestampNs, sizeof(uint64_t), 1, f) != 1) break;
      uint8_t cat = 0;
      if (fread(&cat, 1, 1, f) != 1) break;
      e.category = static_cast<EventCategory>(cat);
      uint8_t sev = 0;
      if (fread(&sev, 1, 1, f) != 1) break;
      e.severity = static_cast<EventSeverity>(sev);
      if (fread(&e.uiActionable, sizeof(int), 1, f) != 1) break;
      if (fread(&e.processId, sizeof(int), 1, f) != 1) break;
      if (fread(&e.processIdCause, sizeof(int), 1, f) != 1) break;
      if (fread(&e.snapshotIdBefore, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&e.snapshotIdAfter, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&e.sessionEventIndex, sizeof(int), 1, f) != 1) break;
      if (fread(&e.ruleId, sizeof(int), 1, f) != 1) break;
      e.type = ReadWString(f);
      e.subsystem = ReadWString(f);
      e.description = ReadWString(f);
      e.metadata = ReadWString(f);
      e.correlationId = ReadWString(f);
      e.processName = ReadWString(f);
      e.causeEntityId = ReadWString(f);
      e.lastSnapshotId = ReadWString(f);
      events.push_back(std::move(e));
    }
    return events;
  }

  static void WriteFindings(FILE* f, const std::vector<ScramResult>& findings) {
    uint32_t count = static_cast<uint32_t>(findings.size());
    fwrite(&count, sizeof(uint32_t), 1, f);
    for (const auto& fn : findings) {
      fwrite(&fn.id, sizeof(uint64_t), 1, f);
      fwrite(&fn.timestampNs, sizeof(uint64_t), 1, f);
      fwrite(&fn.riskScore, sizeof(int), 1, f);
      fwrite(&fn.ruleId, sizeof(int), 1, f);
      fwrite(&fn.cooldownMinutes, sizeof(int), 1, f);
      fwrite(&fn.cooldownUntilNs, sizeof(uint64_t), 1, f);
      fwrite(&fn.suppressed, sizeof(int), 1, f);
      fwrite(&fn.sessionFindingIndex, sizeof(int), 1, f);
      WriteWString(f, fn.headline);
      WriteWString(f, fn.insight);
      WriteWString(f, fn.context);
      WriteWString(f, fn.correlationId);
      WriteWString(f, fn.causeEntityId);
      WriteWString(f, fn.lastSnapshotId);
    }
  }

  static std::vector<ScramResult> ReadFindings(FILE* f) {
    uint32_t count = 0;
    if (fread(&count, sizeof(uint32_t), 1, f) != 1) return {};
    std::vector<ScramResult> findings;
    findings.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      ScramResult fn;
      if (fread(&fn.id, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&fn.timestampNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&fn.riskScore, sizeof(int), 1, f) != 1) break;
      if (fread(&fn.ruleId, sizeof(int), 1, f) != 1) break;
      if (fread(&fn.cooldownMinutes, sizeof(int), 1, f) != 1) break;
      if (fread(&fn.cooldownUntilNs, sizeof(uint64_t), 1, f) != 1) break;
      if (fread(&fn.suppressed, sizeof(int), 1, f) != 1) break;
      if (fread(&fn.sessionFindingIndex, sizeof(int), 1, f) != 1) break;
      fn.headline = ReadWString(f);
      fn.insight = ReadWString(f);
      fn.context = ReadWString(f);
      fn.correlationId = ReadWString(f);
      fn.causeEntityId = ReadWString(f);
      fn.lastSnapshotId = ReadWString(f);
      findings.push_back(std::move(fn));
    }
    return findings;
  }

  std::wstring basePath_;
};

}
