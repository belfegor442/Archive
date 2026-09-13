#pragma once

#include <cstdint>

#include "../../Snapshot.hpp"

namespace monix {

int IcmpPingGateway();
int ProbeDnsResolution();
std::uint64_t ComputeRouteTableHash();
void CollectNetworkDiagnostics(Snapshot& snapshot);
std::uint64_t CountTcpTeardownStates();

}
