#pragma once

#include <windows.h>

namespace monix {

inline void SetupDpiAwareness() {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (user32) {
    using SetDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto pSetDpiAwarenessContext = reinterpret_cast<SetDpiAwarenessContextFn>(
      GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (pSetDpiAwarenessContext) {
      pSetDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else {
      HMODULE shcore = LoadLibraryW(L"shcore.dll");
      if (shcore) {
        using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
        auto pSetDpi = reinterpret_cast<SetProcessDpiAwarenessFn>(
          GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (pSetDpi) {
          pSetDpi(2);
        }
        FreeLibrary(shcore);
      } else {
        SetProcessDPIAware();
      }
    }
  }
}

} // namespace monix
