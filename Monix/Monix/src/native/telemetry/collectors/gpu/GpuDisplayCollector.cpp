#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "GpuDisplayCollector.hpp"

#pragma comment(lib, "wbemuuid.lib")

namespace monix {

void CollectGpuDisplayInfo(Snapshot& snapshot) {
  {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
      IWbemLocator* pLoc = nullptr;
      hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
      if (SUCCEEDED(hr) && pLoc) {
        IWbemServices* pSvc = nullptr;
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, 0, 0, &pSvc);
        if (SUCCEEDED(hr) && pSvc) {
          CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
          IEnumWbemClassObject* pEnum = nullptr;
          hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(L"SELECT Name, DriverVersion, AdapterRAM, MaxClockSpeed, CurrentClockSpeed FROM Win32_VideoController"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
          if (SUCCEEDED(hr) && pEnum) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            if (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              VARIANT vt;
              VariantInit(&vt);
              if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, nullptr, nullptr)) && vt.bstrVal) {
                snapshot.gpuModel = vt.bstrVal;
              }
              VariantClear(&vt);
              VariantInit(&vt);
              if (SUCCEEDED(pObj->Get(L"DriverVersion", 0, &vt, nullptr, nullptr)) && vt.bstrVal) {
                snapshot.gpuDriverVersion = vt.bstrVal;
              }
              VariantClear(&vt);
              VariantInit(&vt);
              if (SUCCEEDED(pObj->Get(L"AdapterRAM", 0, &vt, nullptr, nullptr))) {
                snapshot.gpuVramTotalBytes = vt.uintVal;
              }
              VariantClear(&vt);
              pObj->Release();
            }
            pEnum->Release();
          }
          pSvc->Release();
        }
        pLoc->Release();
      }
      CoUninitialize();
    }
  }

  snapshot.displayMonitorCount = 0;
  DISPLAY_DEVICEW dd {};
  dd.cb = sizeof(dd);
  for (int i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
    if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) {
      snapshot.displayMonitorCount++;
      snapshot.displayNames.insert(dd.DeviceName);
    }
    dd.cb = sizeof(dd);
  }

  DEVMODEW dm {};
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
    snapshot.displayWidth = dm.dmPelsWidth;
    snapshot.displayHeight = dm.dmPelsHeight;
    snapshot.displayRefreshRateHz = dm.dmDisplayFrequency;
    snapshot.displayBitsPerPel = dm.dmBitsPerPel;
  }

  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    DWORD tdrLevel = 3;
    DWORD dataSize = sizeof(tdrLevel);
    RegQueryValueExW(hKey, L"TdrLevel", nullptr, nullptr, reinterpret_cast<BYTE*>(&tdrLevel), &dataSize);
    snapshot.tdrLevel = static_cast<int>(tdrLevel);
    RegCloseKey(hKey);
  }

  HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
  if (dwm) {
    typedef HRESULT (WINAPI *DwmIsCompositionEnabled_t)(BOOL*);
    auto pDwmIsCompositionEnabled = reinterpret_cast<DwmIsCompositionEnabled_t>(GetProcAddress(dwm, "DwmIsCompositionEnabled"));
    if (pDwmIsCompositionEnabled) {
      BOOL enabled = FALSE;
      if (SUCCEEDED(pDwmIsCompositionEnabled(&enabled))) {
        snapshot.desktopCompositionEnabled = enabled ? 1 : 0;
      }
    }
    FreeLibrary(dwm);
  }
}

}
