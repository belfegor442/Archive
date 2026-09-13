#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>

#include <cstdint>
#include <string>
#include <vector>

#include "ThermalCollector.hpp"

#pragma comment(lib, "wbemuuid.lib")

namespace monix {

void CollectThermalData(Snapshot& snapshot) {
  snapshot.cpuCoreTempC = 0.0;
  snapshot.cpuCoreTempMax = 0.0;
  snapshot.motherboardTempC = 0.0;
  snapshot.vrmTempC = 0.0;
  snapshot.ambientTempC = 0.0;
  snapshot.cpuThrottleTempC = 0.0;
  snapshot.cpuThrottling = 0;
  snapshot.fanSpeeds.clear();
  snapshot.pumpSpeed = 0;
  snapshot.pumpPresent = 0;
  snapshot.thermalSensorCount = 0;
  snapshot.thermalSensorFailures = 0;
  snapshot.thermalHeatSoakIndex = 0.0;
  snapshot.coolingCurveSlope = 0.0;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool coInited = SUCCEEDED(hr);
  if (coInited) {
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLoc);
    if (SUCCEEDED(hr)) {
      IWbemServices* pSvc = nullptr;
      hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
      if (SUCCEEDED(hr)) {
        CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        IEnumWbemClassObject* pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM MSAcpi_ThermalZoneTemperature"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          double totalTemp = 0.0;
          int zoneCount = 0;
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            double zoneTemp = 0.0;
            if (SUCCEEDED(pObj->Get(L"CurrentTemperature", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_I4 || vt.vt == VT_UI4) {
                zoneTemp = (static_cast<double>(vt.lVal) - 2732.0) / 10.0;
              } else if (vt.vt == VT_R8 || vt.vt == VT_R4) {
                zoneTemp = vt.dblVal;
              }
              VariantClear(&vt);
            }
            if (zoneTemp > 0.0 && zoneTemp < 150.0) {
              totalTemp += zoneTemp;
              zoneCount++;
              snapshot.thermalSensorCount++;
              if (zoneTemp > snapshot.cpuCoreTempMax) {
                snapshot.cpuCoreTempMax = zoneTemp;
              }
            } else {
              snapshot.thermalSensorFailures++;
            }
            pObj->Release();
          }
          pEnumerator->Release();
          if (zoneCount > 0) {
            snapshot.cpuCoreTempC = totalTemp / zoneCount;
          }
        }

        IWbemServices* pCimSvc = nullptr;
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pCimSvc);
        if (SUCCEEDED(hr)) {
          CoSetProxyBlanket(pCimSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

          pEnumerator = nullptr;
          hr = pCimSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_Fan"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
          if (SUCCEEDED(hr)) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              snapshot.fanCount++;
              VARIANT vt;
              if (SUCCEEDED(pObj->Get(L"DesiredSpeed", 0, &vt, nullptr, nullptr))) {
                if (vt.vt == VT_I4 || vt.vt == VT_UI4) {
                  snapshot.fanSpeeds.push_back(static_cast<int>(vt.lVal));
                }
                VariantClear(&vt);
              } else {
                snapshot.fanSpeeds.push_back(0);
              }
              pObj->Release();
            }
            pEnumerator->Release();
          }

          pEnumerator = nullptr;
          hr = pCimSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM Win32_TemperatureProbe"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, &pEnumerator);
          if (SUCCEEDED(hr)) {
            IWbemClassObject* pObj = nullptr;
            ULONG returned = 0;
            while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
              snapshot.thermalSensorCount++;
              VARIANT vt;
              if (SUCCEEDED(pObj->Get(L"CurrentReading", 0, &vt, nullptr, nullptr))) {
                if (vt.vt == VT_NULL) {
                  snapshot.thermalSensorFailures++;
                }
                VariantClear(&vt);
              }
              pObj->Release();
            }
            pEnumerator->Release();
          }

          pCimSvc->Release();
        }
        pSvc->Release();
      }
      pLoc->Release();
    }
  }

  snapshot.motherboardTempC = 0.0;
  snapshot.vrmTempC = 0.0;
  snapshot.ambientTempC = 0.0;

  if (snapshot.cpuPct > 90.0 && snapshot.cpuCoreTempC > 85.0) {
    snapshot.cpuThrottling = 1;
  }

  if (coInited) CoUninitialize();
}

}
