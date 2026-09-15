#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace monix {

struct NetworkFlow {
  std::wstring name;
  int activeConnections = 0;
  std::wstring remote;
  std::wstring state;
};

struct NetworkState {
  uint64_t upBytesPerSec = 0;
  uint64_t downBytesPerSec = 0;
  int inboundConnections = 0;
  int outboundConnections = 0;
  int udpConnectionCount = 0;
  int dnsPseudo = 0;
  int latencyMs = -1;
  std::wstring latencySource = L"unavailable";
  int pingRttMs = -1;
  int dnsResolutionMs = -1;
  int dnsResolutionOk = -1;
  uint64_t tcpRetransmits = 0;
  uint64_t tcpResets = 0;
  uint64_t routeTableHash = 0;
  int proxyEnabled = 0;
  int adapterCount = 0;
  uint64_t primaryLinkSpeedBps = 0;
  std::set<std::wstring> adapterAddresses;
  std::set<std::wstring> adapterGateways;
  std::set<DWORD> adapterSpeeds;
  std::set<DWORD> adapterOperStatuses;
  std::set<DWORD> adapterTypes;
  std::set<std::wstring> vpnAdapterDescriptions;
  std::vector<NetworkFlow> flows;
};

}
