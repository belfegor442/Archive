#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>

#include <cstdint>
#include <string>
#include <vector>

#include "HardwareBoardCollector.hpp"

#pragma comment(lib, "wbemuuid.lib")

namespace monix {

void CollectHardwareBoardData(Snapshot& snapshot) {
  const DWORD kSmbiosSignature = static_cast<DWORD>('R') | (static_cast<DWORD>('S') << 8) | (static_cast<DWORD>('M') << 16) | (static_cast<DWORD>('B') << 24);
  DWORD smbiosSize = GetSystemFirmwareTable(kSmbiosSignature, 0, nullptr, 0);
  if (smbiosSize > 0) {
    std::vector<BYTE> smbiosBuf(smbiosSize);
    if (GetSystemFirmwareTable(kSmbiosSignature, 0, smbiosBuf.data(), smbiosSize) > 0) {
      DWORD hash = 0;
      for (DWORD i = 0; i < smbiosSize; ++i) {
        hash = ((hash << 5) + hash) + smbiosBuf[i];
      }
      snapshot.smbiosHash = hash;
    }
  }

  const DWORD kAcpiSignature = static_cast<DWORD>('A') | (static_cast<DWORD>('C') << 8) | (static_cast<DWORD>('P') << 16) | (static_cast<DWORD>('I') << 24);
  DWORD acpiSize = GetSystemFirmwareTable(kAcpiSignature, 0, nullptr, 0);
  if (acpiSize > 0) {
    std::vector<BYTE> acpiBuf(acpiSize);
    if (GetSystemFirmwareTable(kAcpiSignature, 0, acpiBuf.data(), acpiSize) > 0) {
      DWORD hash = 0;
      for (DWORD i = 0; i < acpiSize; ++i) {
        hash = ((hash << 5) + hash) + acpiBuf[i];
      }
      snapshot.acpiHash = hash;
    }
  }

  snapshot.biosVersion.clear();
  snapshot.biosManufacturer.clear();
  snapshot.biosMajorVer = 0;
  snapshot.biosMinorVer = 0;
  snapshot.tpmPresent = 0;
  snapshot.tpmReady = 0;
  snapshot.tpmVersion = 0;
  snapshot.voltage12V = 0.0;
  snapshot.voltage5V = 0.0;
  snapshot.voltage33V = 0.0;
  snapshot.voltageVcore = 0.0;
  snapshot.voltageVram = 0.0;
  snapshot.sensorPollFailures = 0;
  snapshot.pcieErrors = 0;
  snapshot.usbResets = 0;
  snapshot.sataResets = 0;
  snapshot.thunderboltEvents = 0;
  snapshot.ecEvents = 0;
  snapshot.cmosBatteryOk = 1;
  snapshot.boardTempHotspot = 0.0;

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
          bstr_t("SELECT * FROM Win32_BIOS"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"Manufacturer", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_BSTR && vt.bstrVal) snapshot.biosManufacturer = vt.bstrVal;
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"SMBIOSBIOSVersion", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_BSTR && vt.bstrVal) snapshot.biosVersion = vt.bstrVal;
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"MajorBIOSVersion", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_I4 || vt.vt == VT_UI4) snapshot.biosMajorVer = static_cast<int>(vt.lVal);
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"MinorBIOSVersion", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_I4 || vt.vt == VT_UI4) snapshot.biosMinorVer = static_cast<int>(vt.lVal);
              VariantClear(&vt);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_VoltageProbe"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          while (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            double reading = 0.0;
            if (SUCCEEDED(pObj->Get(L"CurrentReading", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_I4 || vt.vt == VT_UI4) reading = static_cast<double>(vt.lVal) / 1000.0;
              else if (vt.vt == VT_R8 || vt.vt == VT_R4) reading = vt.dblVal;
              VariantClear(&vt);
            }
            VARIANT vtName;
            if (SUCCEEDED(pObj->Get(L"Name", 0, &vtName, nullptr, nullptr))) {
              if (vtName.vt == VT_BSTR && vtName.bstrVal) {
                std::wstring name = vtName.bstrVal;
                if (name.find(L"12") != std::wstring::npos && name.find(L"V") != std::wstring::npos) snapshot.voltage12V = reading;
                else if (name.find(L"5") != std::wstring::npos && name.find(L"V") != std::wstring::npos) snapshot.voltage5V = reading;
                else if (name.find(L"3.3") != std::wstring::npos && name.find(L"V") != std::wstring::npos) snapshot.voltage33V = reading;
                else if (name.find(L"Core") != std::wstring::npos || name.find(L"Vcore") != std::wstring::npos) snapshot.voltageVcore = reading;
                else if (name.find(L"DDR") != std::wstring::npos || name.find(L"Memory") != std::wstring::npos || name.find(L"VRAM") != std::wstring::npos) snapshot.voltageVram = reading;
              }
              VariantClear(&vtName);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_Tpm"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            snapshot.tpmPresent = 1;
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"IsEnabled", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_BOOL) snapshot.tpmReady = vt.boolVal ? 1 : 0;
              VariantClear(&vt);
            }
            if (SUCCEEDED(pObj->Get(L"SpecVersion", 0, &vt, nullptr, nullptr))) {
              if (vt.vt == VT_BSTR && vt.bstrVal) {
                std::wstring ver = vt.bstrVal;
                if (ver.find(L"2.0") != std::wstring::npos) snapshot.tpmVersion = 20;
                else if (ver.find(L"1.2") != std::wstring::npos) snapshot.tpmVersion = 12;
              }
              VariantClear(&vt);
            }
            pObj->Release();
          }
          pEnumerator->Release();
        }

        pEnumerator = nullptr;
        hr = pSvc->ExecQuery(
          bstr_t("WQL"),
          bstr_t("SELECT * FROM Win32_BaseBoard"),
          WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
          nullptr, &pEnumerator);
        if (SUCCEEDED(hr)) {
          IWbemClassObject* pObj = nullptr;
          ULONG returned = 0;
          if (pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &returned) == S_OK && returned > 0) {
            VARIANT vt;
            if (SUCCEEDED(pObj->Get(L"HostingBoard", 0, &vt, nullptr, nullptr))) {
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
  }
  if (coInited) CoUninitialize();
}

}
