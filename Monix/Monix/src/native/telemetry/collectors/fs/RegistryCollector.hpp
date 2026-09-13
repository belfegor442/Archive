#pragma once

#include "../../Snapshot.hpp"

namespace monix {

unsigned long HashRegistryValues(HKEY hKey);
void CollectRegistryData(Snapshot& snapshot);

}
