#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>

#include <cstdint>
#include <string>

namespace monix::etw {

enum class ProviderLevel {
  Critical = 1,
  Error = 2,
  Warning = 3,
  Informational = 4,
  Verbose = 5
};

struct ProviderConfig {
  GUID providerGuid = {};
  std::wstring name;
  ProviderLevel level = ProviderLevel::Informational;
};

class EtwProvider {
public:
  EtwProvider() = default;

  bool Register(const ProviderConfig& config) {
    config_ = config;
    registered_ = true;
    return true;
  }

  bool WriteEvent(WORD eventType, const void* userData, ULONG userDataSize) {
    if (!registered_) return false;

    EVENT_DATA_DESCRIPTOR dataDesc = {};
    if (userData && userDataSize > 0) {
      EventDataDescCreate(&dataDesc, userData, userDataSize);
    }

    eventsWritten_++;
    return true;
  }

  bool WriteString(WORD eventType, const wchar_t* message) {
    if (!registered_ || !message) return false;
    eventsWritten_++;
    return true;
  }

  bool IsRegistered() const { return registered_; }
  std::uint64_t EventsWritten() const { return eventsWritten_; }
  std::uint64_t EventsLost() const { return eventsLost_; }

private:
  ProviderConfig config_;
  bool registered_ = false;
  std::uint64_t eventsWritten_ = 0;
  std::uint64_t eventsLost_ = 0;
};

}