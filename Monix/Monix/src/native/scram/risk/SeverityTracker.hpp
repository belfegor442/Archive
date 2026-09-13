#pragma once

#include <algorithm>
#include <cstdint>

namespace monix::scram {

enum class SeverityLevel {
  Info = 0,
  Warning = 1,
  Error = 2,
  Critical = 3
};

inline const wchar_t* SeverityName(SeverityLevel s) {
  switch (s) {
    case SeverityLevel::Info:     return L"INFO";
    case SeverityLevel::Warning:  return L"WARNING";
    case SeverityLevel::Error:    return L"ERROR";
    case SeverityLevel::Critical: return L"CRITICAL";
  }
  return L"UNKNOWN";
}

inline const char* SeverityNameA(SeverityLevel s) {
  switch (s) {
    case SeverityLevel::Info:     return "INFO";
    case SeverityLevel::Warning:  return "WARNING";
    case SeverityLevel::Error:    return "ERROR";
    case SeverityLevel::Critical: return "CRITICAL";
  }
  return "UNKNOWN";
}

class SeverityTracker {
public:
  SeverityTracker() = default;

  void SetHysteresis(int holdThreshold) { holdThreshold_ = holdThreshold; }
  void SetErrorHoldThreshold(int threshold) { errorHoldThreshold_ = threshold; }

  SeverityLevel Update(int currentRisk) {
    int rawSeverity = currentRisk >= 70 ? 3 : currentRisk >= 50 ? 2 : currentRisk >= 30 ? 1 : 0;

    if (rawSeverity == 2) {
      errorHold_++;
    } else {
      errorHold_ = 0;
    }

    if (rawSeverity < 2 && currentSeverity_ == SeverityLevel::Critical) {
      rawSeverity = 2;
    }

    if (errorHold_ >= errorHoldThreshold_ && rawSeverity >= 2) {
      rawSeverity = 3;
    }

    SeverityLevel raw = static_cast<SeverityLevel>(rawSeverity);

    if (raw > currentSeverity_) {
      currentSeverity_ = raw;
      holdCounter_ = 0;
    } else if (raw < currentSeverity_) {
      holdCounter_++;
      if (holdCounter_ < holdThreshold_) {
        raw = currentSeverity_;
      } else {
        currentSeverity_ = raw;
        holdCounter_ = 0;
      }
    } else {
      holdCounter_ = 0;
    }

    return currentSeverity_;
  }

  SeverityLevel Current() const { return currentSeverity_; }
  SeverityLevel Previous() const { return previousSeverity_; }

  bool Changed() const { return currentSeverity_ != previousSeverity_; }

  void Snapshot() { previousSeverity_ = currentSeverity_; }

  void Reset() {
    currentSeverity_ = SeverityLevel::Info;
    previousSeverity_ = SeverityLevel::Info;
    holdCounter_ = 0;
    errorHold_ = 0;
  }

  int HoldCounter() const { return holdCounter_; }
  int ErrorHold() const { return errorHold_; }

private:
  SeverityLevel currentSeverity_ = SeverityLevel::Info;
  SeverityLevel previousSeverity_ = SeverityLevel::Info;
  int holdCounter_ = 0;
  int errorHold_ = 0;
  int holdThreshold_ = 6;
  int errorHoldThreshold_ = 10;
};

}