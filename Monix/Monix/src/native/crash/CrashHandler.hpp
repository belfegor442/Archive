#pragma once

#include <windows.h>
#include <cstdint>

namespace monix {

LONG CALLBACK CrashVehHandler(EXCEPTION_POINTERS* ep);

} // namespace monix
