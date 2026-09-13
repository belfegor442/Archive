#pragma once

#include <atomic>
#include <cstdint>

#include "../../Snapshot.hpp"

namespace monix {

extern std::atomic<int> g_sehExceptionCount;
extern std::atomic<int> g_unhandledExceptionCount;
extern std::atomic<int> g_accessViolationCount;
extern std::atomic<int> g_heapCorruptionDetected;
extern std::atomic<int> g_assertionFailureCount;
extern std::atomic<int> g_stackOverflowCount;

unsigned long HashProcessList();
unsigned long HashModuleList(DWORD processId);
void CollectReliabilityData(Snapshot& snapshot);

}
