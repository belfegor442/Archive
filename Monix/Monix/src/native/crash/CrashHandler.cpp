#include "CrashHandler.hpp"

#include "../TelemetryInternal.hpp"

#include <cstdint>
#include <cstdio>

namespace monix {

using monix::internal::g_phase;
using monix::internal::g_telTid;
using monix::internal::GetCrashLogPath;

LONG CALLBACK CrashVehHandler(EXCEPTION_POINTERS* ep) {
  if (ep && ep->ExceptionRecord) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    const char* name = "UNKNOWN";
    switch (code) {
      case 0xC0000005: name = "ACCESS_VIOLATION"; break;
      case 0xC00000FD: name = "STACK_OVERFLOW"; break;
      case 0xC0000013A: name = "TERMINATION"; break;
      case 0xC0000374: name = "HEAP_CORRUPTION"; break;
      case 0xE06D7363: name = "CPP_EXCEPTION"; break;
      case 0xE0434352: name = "CLR_EXCEPTION"; break;
      case 0x80000003: name = "BREAKPOINT"; break;
      case 0xC0000008: name = "INVALID_HANDLE"; break;
      case 0xC000000D: name = "INVALID_PARAMETER"; break;
      case 0xC0000088: name = "NOT_ENOUGH_MEMORY"; break;
    }
    HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[512];
      auto addr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);
      auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
      auto offset = addr - base;
      const char* phase = g_phase.load();
      DWORD crashTid = GetCurrentThreadId();
      DWORD telTid = g_telTid.load();
      int len = snprintf(buf, sizeof(buf), "VEH CODE=0x%08X (%s) Phase=%s Addr=0x%llx Offset=0x%llx TID=%lu TelTID=%lu\n",
        (unsigned)code, name, phase ? phase : "?",
        (unsigned long long)addr, (unsigned long long)offset,
        (unsigned long)crashTid, (unsigned long)telTid);
      if (len > 0 && static_cast<size_t>(len) < sizeof(buf)) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
      }
      FlushFileBuffers(h);
      CloseHandle(h);
    }
    return EXCEPTION_CONTINUE_SEARCH;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace monix
