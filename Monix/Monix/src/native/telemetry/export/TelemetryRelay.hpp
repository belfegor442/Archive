#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "WebSocketServer.hpp"
#include "../Snapshot.hpp"
#include "../../scram/ScramEngine.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

class TelemetryRelay {
 public:
  explicit TelemetryRelay(WsConfig config = {}) : server_(config) {}

  bool Start() { return server_.Start(); }
  void Stop() { server_.Stop(); }
  bool IsRunning() const { return server_.IsRunning(); }
  int ClientCount() const { return server_.ClientCount(); }
  int Port() const { return server_.Port(); }

  void BroadcastSnapshot(const Snapshot& snap, const ScramResult& scram,
                         int sampleIndex, uint64_t timestampMs) {
    if (!server_.IsRunning() || server_.ClientCount() == 0) return;
    server_.Broadcast(SerializeSnapshot(snap, scram, sampleIndex, timestampMs));
  }

  void BroadcastScramUpdate(const ScramResult& scram, uint64_t timestampMs) {
    if (!server_.IsRunning() || server_.ClientCount() == 0) return;
    server_.Broadcast(SerializeScramUpdate(scram, timestampMs));
  }

  void BroadcastStatus(const std::string& status) {
    if (!server_.IsRunning()) return;
    std::string msg = R"({"type":"status","message":")" + EscapeJsonA(status) + R"("})";
    server_.Broadcast(msg);
  }

 private:
  static std::string EscapeJsonA(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
      switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:   out += c; break;
      }
    }
    return out;
  }

  static std::string EscapeJsonW(const std::wstring& ws) {
    return EscapeJsonA(WideToUtf8(ws));
  }

  static std::string SerializeSnapshot(const Snapshot& snap, const ScramResult& scram,
                                       int sampleIndex, uint64_t timestampMs) {
    std::ostringstream o;
    double ramPct = snap.ramTotalBytes > 0
        ? (static_cast<double>(snap.ramUsedBytes) * 100.0 / snap.ramTotalBytes) : 0.0;
    o << R"({"type":"snapshot","ts":)" << timestampMs
      << R"(,"sample":)" << sampleIndex
      << R"(,"cpu":{"pct":)" << snap.cpuPct
      << R"(,"cores":)" << snap.cpuCores
      << R"(,"freqMhz":)" << snap.cpuBaseClockMhz
      << R"(,"tempC":)" << snap.cpuCoreTempC
      << R"(,"thermalThrottle":)" << (snap.cpuThrottling ? "true" : "false")
      << R"(},"ram":{"pct":)" << ramPct
      << R"(,"usedMb":)" << (snap.ramUsedBytes / 1048576)
      << R"(,"totalMb":)" << (snap.ramTotalBytes / 1048576)
      << R"(,"availMb":)" << (snap.ramAvailBytes / 1048576)
      << R"(},"gpu":{"pct":)" << snap.gpuPct
      << R"(,"tempC":)" << snap.gpuTempC
      << R"(,"vramUsedMb":)" << (snap.gpuVramUsedBytes / 1048576)
      << R"(,"vramTotalMb":)" << (snap.gpuVramTotalBytes / 1048576)
      << R"(},"disk":{"readMb":)" << (snap.diskReadBytesPerSec / 1048576.0)
      << R"(,"writeMb":)" << (snap.diskWriteBytesPerSec / 1048576.0)
      << R"(,"queueLen":)" << snap.diskQueueLength
      << R"(,"pctUsed":)" << snap.diskPctUsed
      << R"(},"net":{"upMb":)" << (snap.netUpBytesPerSec / 1048576.0)
      << R"(,"downMb":)" << (snap.netDownBytesPerSec / 1048576.0)
      << R"(},"power":{"batteryPct":)" << snap.batteryChargePercent
      << R"(,"batteryLife":)" << snap.batteryLifePercent
      << R"(},"uptime":{"ms":)" << snap.uptimeMs
      << R"(},"processes":{"count":)" << snap.processCount
      << R"(},"security":{"unsignedDrivers":)" << snap.unsignedDriverCount
      << R"(,"debugPort":)" << snap.debugPortActive
      << R"(},"scram":{"risk":)" << scram.riskScore
      << R"(,"headline":")" << EscapeJsonW(scram.headline) << R"(")"
      << R"(,"insight":")" << EscapeJsonW(scram.insight) << R"(")"
      << R"(,"diagnostics":[)";
    for (size_t i = 0; i < scram.diagnostics.size(); ++i) {
      if (i > 0) o << ',';
      o << '"' << EscapeJsonW(scram.diagnostics[i]) << '"';
    }
    o << R"(]}})";
    return o.str();
  }

  static std::string SerializeScramUpdate(const ScramResult& scram, uint64_t timestampMs) {
    std::ostringstream o;
    o << R"({"type":"scram","ts":)" << timestampMs
      << R"(,"risk":)" << scram.riskScore
      << R"(,"headline":")" << EscapeJsonW(scram.headline) << R"(")"
      << R"(,"insight":")" << EscapeJsonW(scram.insight) << R"(")"
      << R"(,"diagnostics":[)";
    for (size_t i = 0; i < scram.diagnostics.size(); ++i) {
      if (i > 0) o << ',';
      o << '"' << EscapeJsonW(scram.diagnostics[i]) << '"';
    }
    o << R"(]})";
    return o.str();
  }

  WebSocketServer server_;
};

} // namespace monix
