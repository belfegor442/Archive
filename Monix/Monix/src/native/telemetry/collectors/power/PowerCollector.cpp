#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <powrprof.h>
#include <Wbemidl.h>
#include <comdef.h>

#include <cstdint>
#include <string>
#include <vector>

#include "PowerCollector.hpp"

#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "wbemuuid.lib")

namespace monix {

void CollectPowerData(Snapshot& snapshot) {
  SYSTEM_POWER_STATUS sps = {};
  if (GetSystemPowerStatus(&sps)) {
    snapshot.acLineStatus = sps.ACLineStatus;
    snapshot.batteryFlag = sps.BatteryFlag;
    snapshot.batteryLifePercent = sps.BatteryLifePercent;
    if (sps.BatteryLifeTime != (DWORD)-1) {
      snapshot.batteryLifeTimeSec = sps.BatteryLifeTime;
    }
  }

  GUID* pActiveGuid = nullptr;
  if (PowerGetActiveScheme(nullptr, &pActiveGuid) == ERROR_SUCCESS && pActiveGuid) {
    snapshot.powerPlanGuid = *pActiveGuid;

    GUID saverGuid = {0xa1841308, 0x3541, 0x4fab, {0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a}};
    GUID perfGuid = {0x8c5e7fda, 0xe8bf, 0x4a96, {0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c}};
    GUID balancedGuid = {0x381b4222, 0xf694, 0x41f0, {0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e}};

    if (memcmp(pActiveGuid, &saverGuid, sizeof(GUID)) == 0) {
      snapshot.powerSaverActive = 1;
      snapshot.highPerfActive = 0;
      snapshot.balancedActive = 0;
      snapshot.powerPlanIndex = 0;
    } else if (memcmp(pActiveGuid, &perfGuid, sizeof(GUID)) == 0) {
      snapshot.powerSaverActive = 0;
      snapshot.highPerfActive = 1;
      snapshot.balancedActive = 0;
      snapshot.powerPlanIndex = 2;
    } else {
      snapshot.powerSaverActive = 0;
      snapshot.highPerfActive = 0;
      snapshot.balancedActive = 1;
      snapshot.powerPlanIndex = 1;
    }

    LocalFree(pActiveGuid);
  }

  snapshot.batteryChargeRate = 0;
  snapshot.batteryChargeState = 0;
  snapshot.batteryWearLevel = -1;
  snapshot.batteryCycleCount = -1;
  snapshot.batteryTemperature = -1;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool coInited = SUCCEEDED(hr);
  if (coInited) {
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (SUCCEEDED(hr)) {
      IWbemServices* pSvc = nullptr;
      hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
      if (SUCCEEDED(hr)) {
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        IEnumWbemClassObject* pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_Battery"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"ChargeRate", 0, &vt, nullptr, nullptr))) {
              if (vt.vt != VT_NULL) snapshot.batteryChargeRate = vt.lVal;
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"EstimatedChargeRemaining", 0, &vt, nullptr, nullptr))) {
              if (vt.vt != VT_NULL) snapshot.batteryChargePercent = vt.lVal;
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"ChargeCompletionTime", 0, &vt, nullptr, nullptr))) {
              VariantClear(&vt);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_Battery"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"DesignCapacity", 0, &vt, nullptr, nullptr))) {
              int designCap = (vt.vt != VT_NULL) ? static_cast<int>(vt.lVal) : 0;
              VariantClear(&vt);
              if (SUCCEEDED(pObj->Get(L"FullChargeCapacity", 0, &vt, nullptr, nullptr))) {
                int fullCap = (vt.vt != VT_NULL) ? static_cast<int>(vt.lVal) : 0;
                VariantClear(&vt);
                if (designCap > 0 && fullCap > 0) {
                  snapshot.batteryWearLevel = 100 - (fullCap * 100 / designCap);
                }
              }
            }
            if (SUCCEEDED(pObj->Get(L"CycleCount", 0, &vt, nullptr, nullptr))) {
              if (vt.vt != VT_NULL) snapshot.batteryCycleCount = static_cast<int>(vt.lVal);
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"Temperature", 0, &vt, nullptr, nullptr))) {
              if (vt.vt != VT_NULL) snapshot.batteryTemperature = static_cast<int>(vt.lVal) / 10;
              VariantClear(&vt);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }
        pSvc->Release();
      }
      pLoc->Release();
    }
    CoUninitialize();
  }

  snapshot.idlePowerDrawHigh = 0;
  if (snapshot.cpuPct < 5.0 && snapshot.acLineStatus == 0 && snapshot.batteryLifeTimeSec > 0 && snapshot.batteryLifeTimeSec < 3600) {
    snapshot.idlePowerDrawHigh = 1;
  }

  snapshot.sleepStateActive = -1;
  snapshot.hibernateActive = -1;
  snapshot.modernStandbyActive = -1;
}

}
