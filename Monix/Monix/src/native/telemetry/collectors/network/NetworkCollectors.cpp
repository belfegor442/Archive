#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iphlpapi.h>
#include <icmpapi.h>
#include <iptypes.h>
#include <netioapi.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "NetworkCollectors.hpp"
#include "../../../core/TextUtils.hpp"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace monix {

int IcmpPingGateway() {
  HANDLE hIcmp = IcmpCreateFile();
  if (hIcmp == INVALID_HANDLE_VALUE) return -1;
  ULONG aaSize = 0;
  GetAdaptersAddresses(AF_INET, 0, nullptr, nullptr, &aaSize);
  if (aaSize == 0) { IcmpCloseHandle(hIcmp); return -1; }
  std::vector<BYTE> aaBuf(aaSize);
  auto* aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aaBuf.data());
  if (GetAdaptersAddresses(AF_INET, 0, nullptr, aa, &aaSize) != NO_ERROR) { IcmpCloseHandle(hIcmp); return -1; }
  for (auto* a = aa; a; a = a->Next) {
    if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    for (auto* ga = a->FirstGatewayAddress; ga; ga = ga->Next) {
      if (!ga->Address.lpSockaddr) continue;
      if (ga->Address.lpSockaddr->sa_family != AF_INET) continue;
      auto* sa = reinterpret_cast<sockaddr_in*>(ga->Address.lpSockaddr);
      if (sa->sin_addr.S_un.S_addr == 0) continue;
      char replyBuf[512] {};
      DWORD result = IcmpSendEcho(hIcmp, sa->sin_addr.S_un.S_addr, nullptr, 0, nullptr, replyBuf, sizeof(replyBuf), 2000);
      IcmpCloseHandle(hIcmp);
      if (result > 0) {
        auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
        return static_cast<int>(reply->RoundTripTime);
      }
      return -1;
    }
  }
  IcmpCloseHandle(hIcmp);
  return -1;
}

int ProbeDnsResolution() {
  ADDRINFO hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ADDRINFO* result = nullptr;
  auto start = std::chrono::steady_clock::now();
  int rc = getaddrinfo("google.com", nullptr, &hints, &result);
  auto elapsed = std::chrono::steady_clock::now() - start;
  int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
  if (rc != 0 || !result) {
    if (result) freeaddrinfo(result);
    return -1;
  }
  freeaddrinfo(result);
  return ms;
}

std::uint64_t ComputeRouteTableHash() {
  ULONG tableSize = 0;
  GetIpForwardTable(nullptr, &tableSize, FALSE);
  if (tableSize == 0) return 0;
  std::vector<BYTE> buf(tableSize);
  auto* table = reinterpret_cast<PMIB_IPFORWARDTABLE>(buf.data());
  if (GetIpForwardTable(table, &tableSize, FALSE) != NO_ERROR) return 0;
  std::uint64_t hash = 0x12345678;
  for (DWORD i = 0; i < table->dwNumEntries; ++i) {
    const auto& entry = table->table[i];
    hash ^= static_cast<std::uint64_t>(entry.dwForwardDest) << (i % 5);
    hash ^= static_cast<std::uint64_t>(entry.dwForwardMask) << ((i + 2) % 7);
    hash ^= static_cast<std::uint64_t>(entry.dwForwardNextHop) << ((i + 3) % 3);
    hash = (hash << 7) | (hash >> 57);
  }
  return hash;
}

void CollectNetworkDiagnostics(Snapshot& snapshot) {
  snapshot.pingRttMs = IcmpPingGateway();
  snapshot.dnsResolutionMs = ProbeDnsResolution();
  snapshot.dnsResolutionOk = (snapshot.dnsResolutionMs >= 0) ? 1 : 0;
  snapshot.routeTableHash = ComputeRouteTableHash();

  ULONG aaSize = 0;
  GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &aaSize);
  if (aaSize > 0) {
    std::vector<BYTE> aaBuf(aaSize);
    auto* aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(aaBuf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, aa, &aaSize) == NO_ERROR) {
      snapshot.netAdapterCount = 0;
      snapshot.netAdapterOperStatuses.clear();
      snapshot.netAdapterTypes.clear();
      snapshot.vpnAdapterDescriptions.clear();
      snapshot.netAdapterSpeeds.clear();
      snapshot.netAdapterAddresses.clear();
      snapshot.netAdapterGateways.clear();
      for (auto* a = aa; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->OperStatus != IfOperStatusUp) continue;
        snapshot.netAdapterCount++;
        snapshot.netAdapterOperStatuses.insert(a->OperStatus);
        snapshot.netAdapterTypes.insert(a->IfType);
        if (a->IfType == IF_TYPE_TUNNEL || a->IfType == IF_TYPE_PPP || a->IfType == IF_TYPE_IEEE1394) {
          if (a->FriendlyName) snapshot.vpnAdapterDescriptions.insert(a->FriendlyName);
        }
        if (a->Description) {
          std::wstring desc = a->Description;
          for (auto& ch : desc) ch = towlower(ch);
          if (desc.find(L"vpn") != std::wstring::npos || desc.find(L"tunnel") != std::wstring::npos ||
              desc.find(L"wireguard") != std::wstring::npos || desc.find(L"tap") != std::wstring::npos ||
              desc.find(L"openvpn") != std::wstring::npos || desc.find(L"forticlient") != std::wstring::npos) {
            snapshot.vpnAdapterDescriptions.insert(a->FriendlyName ? a->FriendlyName : L"VPN");
          }
        }
        DWORD speed = static_cast<DWORD>(a->TransmitLinkSpeed);
        snapshot.netAdapterSpeeds.insert(speed);
        if (speed > snapshot.netPrimaryLinkSpeedBps) {
          snapshot.netPrimaryLinkSpeedBps = speed;
        }
        for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
          if (!ua->Address.lpSockaddr) continue;
          if (ua->Address.lpSockaddr->sa_family == AF_INET) {
            auto* sa4 = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char ipBuf[INET_ADDRSTRLEN] {};
            inet_ntop(AF_INET, &sa4->sin_addr, ipBuf, sizeof(ipBuf));
            snapshot.netAdapterAddresses.insert(Utf8ToWide(ipBuf));
          }
        }
        for (auto* ga = a->FirstGatewayAddress; ga; ga = ga->Next) {
          if (!ga->Address.lpSockaddr) continue;
          if (ga->Address.lpSockaddr->sa_family == AF_INET) {
            auto* sa4 = reinterpret_cast<sockaddr_in*>(ga->Address.lpSockaddr);
            char ipBuf[INET_ADDRSTRLEN] {};
            inet_ntop(AF_INET, &sa4->sin_addr, ipBuf, sizeof(ipBuf));
            std::wstring gw = Utf8ToWide(ipBuf);
            if (gw != L"0.0.0.0") snapshot.netAdapterGateways.insert(gw);
          }
        }
      }
    }
  }

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD proxyEnabled = 0;
    DWORD dataSize = sizeof(proxyEnabled);
    RegQueryValueExW(hKey, L"ProxyEnable", nullptr, nullptr, reinterpret_cast<BYTE*>(&proxyEnabled), &dataSize);
    snapshot.proxyEnabled = static_cast<int>(proxyEnabled);
    RegCloseKey(hKey);
  }
}

std::uint64_t CountTcpTeardownStates() {
  std::uint64_t count = 0;
  DWORD tableSize = 0;
  GetExtendedTcpTable(nullptr, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
  if (tableSize > 0) {
    std::vector<BYTE> buf(tableSize);
    auto* tcp = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
    if (GetExtendedTcpTable(tcp, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
      for (DWORD i = 0; i < tcp->dwNumEntries; ++i) {
        DWORD st = tcp->table[i].dwState;
        if (st == MIB_TCP_STATE_TIME_WAIT || st == MIB_TCP_STATE_CLOSING || st == MIB_TCP_STATE_LAST_ACK) {
          count++;
        }
      }
    }
  }
  return count;
}

}
