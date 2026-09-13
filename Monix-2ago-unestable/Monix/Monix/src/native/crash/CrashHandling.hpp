#pragma once

#include <windows.h>

namespace monix::internal {

void LogSehToCrashLog(const char* phase, unsigned int code, const char* exName);
void SehTranslator(unsigned int code, _EXCEPTION_POINTERS* ep);
bool HeapOk(const char* tag);
void CheckStackCanaries();

}  // namespace monix::internal
