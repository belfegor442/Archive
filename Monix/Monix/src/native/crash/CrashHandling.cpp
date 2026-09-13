#include "CrashHandling.hpp"

#include "../TelemetryInternal.hpp"

#include <windows.h>
#include <cstdio>
#include <stdexcept>

namespace monix::internal {

void LogSehToCrashLog(const char* phase, unsigned int code, const char* exName) {
  HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
    GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    SetFilePointer(h, 0, nullptr, FILE_END);
    char buf[512];
    int len = snprintf(buf, sizeof(buf), "SEH in %s Code=0x%08X (%s) TID=%lu\n",
      phase, code, exName, (unsigned long)GetCurrentThreadId());
    if (len > 0 && static_cast<size_t>(len) < sizeof(buf)) {
      DWORD written = 0;
      WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
    }
    FlushFileBuffers(h);
    CloseHandle(h);
  }
}

void SehTranslator(unsigned int code, _EXCEPTION_POINTERS* /*ep*/) {
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
  if (code == 0xC00000FD) {
    HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "SEH:STACK_OVERFLOW at exception %s (0x%08X)\n", name, code);
      if (len > 0 && static_cast<size_t>(len) < sizeof(buf)) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
      }
      CloseHandle(h);
    }
    TerminateProcess(GetCurrentProcess(), 0xC00000FD);
  }
  throw std::runtime_error(std::string("SEH:") + name);
}

bool HeapOk(const char* tag) {
  BOOL ok = HeapValidate(GetProcessHeap(), 0, nullptr);
  if (!ok) {
    HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "HEAP CORRUPTED at: %s\n", tag);
      if (len > 0 && static_cast<size_t>(len) < sizeof(buf)) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
      }
      CloseHandle(h);
    }
  }
  return ok != FALSE;
}

void CheckStackCanaries() {
  volatile uint64_t stackCanary1 = 0xDEADBEEFCAFE1234ULL;
  volatile uint64_t stackCanary2 = 0x1234567890ABCDEFULL;

  (void)stackCanary1;
  (void)stackCanary2;

  if (stackCanary1 != 0xDEADBEEFCAFE1234ULL || stackCanary2 != 0x1234567890ABCDEFULL) {
    HANDLE h = CreateFileA(GetCrashLogPath().c_str(),
      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SetFilePointer(h, 0, nullptr, FILE_END);
      char buf[256];
      int len = snprintf(buf, sizeof(buf), "STACK CANARY CORRUPTED at POLL:RETURN TID=%lu\n",
        (unsigned long)GetCurrentThreadId());
      if (len > 0 && static_cast<size_t>(len) < sizeof(buf)) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(len), &written, nullptr);
      }
      CloseHandle(h);
    }
  }
}

}  // namespace monix::internal
