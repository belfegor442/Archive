#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <psapi.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <tlhelp32.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "SecurityCollector.hpp"

#pragma comment(lib, "psapi.lib")

namespace monix {

void CollectSecurityData(Snapshot& snapshot) {
  wchar_t exePath[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  snapshot.selfExePath = exePath;

  WINTRUST_FILE_INFO fileInfo = {};
  fileInfo.cbStruct = sizeof(fileInfo);
  fileInfo.pcwszFilePath = exePath;
  fileInfo.hFile = nullptr;
  fileInfo.pgKnownSubject = nullptr;

  GUID actionId = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  WINTRUST_DATA wintrustData = {};
  wintrustData.cbStruct = sizeof(wintrustData);
  wintrustData.pPolicyCallbackData = nullptr;
  wintrustData.pSIPClientData = nullptr;
  wintrustData.dwUIChoice = WTD_UI_NONE;
  wintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrustData.dwUnionChoice = WTD_CHOICE_FILE;
  wintrustData.dwStateAction = WTD_STATEACTION_VERIFY;
  wintrustData.hWVTStateData = nullptr;
  wintrustData.pwszURLReference = nullptr;
  wintrustData.dwProvFlags = WTD_SAFER_FLAG;
  wintrustData.dwUIContext = 0;
  wintrustData.pFile = &fileInfo;

  LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &wintrustData);
  wintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &wintrustData);

  snapshot.selfSignatureValid = (status == ERROR_SUCCESS) ? 1 : 0;

  HANDLE hFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile != INVALID_HANDLE_VALUE) {
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize != INVALID_FILE_SIZE && fileSize < 64 * 1024 * 1024) {
      std::vector<BYTE> fileData(fileSize);
      DWORD bytesRead = 0;
      if (ReadFile(hFile, fileData.data(), fileSize, &bytesRead, nullptr)) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        if (CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
          if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            if (CryptHashData(hHash, fileData.data(), bytesRead, 0)) {
              snapshot.selfHashComputed = 1;
            }
            CryptDestroyHash(hHash);
          }
          CryptReleaseContext(hProv, 0);
        }
      }
    }
    CloseHandle(hFile);
  }

  snapshot.unsignedDriverCount = 0;
  snapshot.unsignedDriverNames.clear();

  static const std::set<std::wstring> kKernelImages = {
    L"ntoskrnl.exe", L"hal.dll", L"bootvid.dll", L"kdcom.dll",
    L"ci.dll", L"msrpc.sys", L"pshed.dll", L"wmilib.sys",
    L"CLASSPNP.SYS", L"disk.sys", L"partmgr.sys", L"volmgr.sys",
    L"mountmgr.sys", L"volsnap.sys", L"rdyboost.sys", L"mssmbios.sys",
    L"intelppm.sys", L"msisadrv.sys", L"swenum.sys", L"rdpbus.sys",
    L"umbus.sys", L"CompositeBus.sys", L"ACPI.sys", L"msdxm.ocx",
  };

  DWORD bytesNeeded = 0;
  EnumDeviceDrivers(nullptr, 0, &bytesNeeded);
  if (bytesNeeded > 0) {
    std::vector<void*> bases(bytesNeeded / sizeof(void*));
    if (EnumDeviceDrivers(bases.data(), bytesNeeded, &bytesNeeded)) {
      DWORD driverCount = bytesNeeded / sizeof(void*);
      for (DWORD i = 0; i < driverCount && i < bases.size(); ++i) {
        wchar_t nameBuf[MAX_PATH] = {};
        if (GetDeviceDriverBaseNameW(bases[i], nameBuf, MAX_PATH)) {
          wchar_t nameLower[MAX_PATH] = {};
          wcscpy_s(nameLower, nameBuf);
          for (wchar_t* p = nameLower; *p; ++p) *p = towlower(*p);
          if (kKernelImages.find(nameLower) != kKernelImages.end()) {
            continue;
          }
          wchar_t fullDriverPath[MAX_PATH] = {};
          if (GetDeviceDriverFileNameW(bases[i], fullDriverPath, MAX_PATH)) {
            WINTRUST_FILE_INFO drvInfo = {};
            drvInfo.cbStruct = sizeof(drvInfo);
            drvInfo.pcwszFilePath = fullDriverPath;
            drvInfo.hFile = nullptr;
            drvInfo.pgKnownSubject = nullptr;

            WINTRUST_DATA drvWtData = {};
            drvWtData.cbStruct = sizeof(drvWtData);
            drvWtData.dwUIChoice = WTD_UI_NONE;
            drvWtData.fdwRevocationChecks = WTD_REVOKE_NONE;
            drvWtData.dwUnionChoice = WTD_CHOICE_FILE;
            drvWtData.dwStateAction = WTD_STATEACTION_VERIFY;
            drvWtData.hWVTStateData = nullptr;
            drvWtData.pFile = &drvInfo;

            LONG drvStatus = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &drvWtData);
            drvWtData.dwStateAction = WTD_STATEACTION_CLOSE;
            WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &drvWtData);

            if (drvStatus != ERROR_SUCCESS) {
              snapshot.unsignedDriverCount++;
              snapshot.unsignedDriverNames.insert(nameBuf);
            }
          }
        }
      }
    }
  }

  snapshot.suspiciousScriptHosts = 0;
  snapshot.suspiciousModules.clear();
  snapshot.lsassAccessCount = 0;
  snapshot.debugPortActive = IsDebuggerPresent() ? 1 : 0;

  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
      do {
        std::wstring procName(pe.szExeFile);
        std::transform(procName.begin(), procName.end(), procName.begin(), ::towlower);
        if (procName == L"wscript.exe" || procName == L"cscript.exe" || procName == L"mshta.exe" || procName == L"powershell.exe") {
          snapshot.suspiciousScriptHosts++;
        }
        if (procName == L"consent.exe") {
          snapshot.uacConsentProcesses++;
        }
        if (procName == L"lsass.exe") {
          snapshot.lsassAccessCount++;
        }
      } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
  }

  snapshot.vmIndicators = 0;
  HKEY hKey = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\VBoxGuest", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    snapshot.vmIndicators++;
    RegCloseKey(hKey);
  }
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vmci", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    snapshot.vmIndicators++;
    RegCloseKey(hKey);
  }
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\vmhgfs", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
    snapshot.vmIndicators++;
    RegCloseKey(hKey);
  }

  snapshot.hookModulesDetected = 0;
  snapshot.peHeaderTamper = 0;
  snapshot.processPaths.clear();

  static const std::set<std::wstring> kKnownSafeModules = {
    L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"advapi32.dll",
    L"user32.dll", L"gdi32.dll", L"gdi32full.dll", L"msvcrt.dll",
    L"sechost.dll", L"rpcrt4.dll", L"ucrtbase.dll", L"combase.dll",
    L"ole32.dll", L"oleaut32.dll", L"shell32.dll", L"shlwapi.dll",
    L"comctl32.dll", L"comdlg32.dll", L"ws2_32.dll", L"iphlpapi.dll",
    L"winmm.dll", L"wininet.dll", L"winhttp.dll", L"crypt32.dll",
    L"bcrypt.dll", L"bcryptprimitives.dll", L"ncrypt.dll", L"wintrust.dll",
    L"imagehlp.dll", L"setupapi.dll", L"cfgmgr32.dll", L"devobj.dll",
    L"powrprof.dll", L"clbcatq.dll", L"comsvcs.dll",
    L"version.dll", L"profapi.dll", L"cryptsp.dll", L"wldp.dll",
    L"msasn1.dll", L"cryptdlg.dll", L"imm32.dll", L"msctf.dll",
    L"win32u.dll", L"dxgi.dll", L"d3d11.dll", L"dwmapi.dll",
    L"opengl32.dll", L"dxcore.dll", L"msi.dll", L"schannel.dll",
    L"winnsi.dll", L"dpapi.dll", L"wtsapi32.dll",
    L"userenv.dll", L"bcp47mrm.dll", L"twinapi.appcore.dll",
    L"rmclient.dll", L"threadpoolwinrt.dll", L"propsys.dll",
    L"Windows.Storage.ApplicationData.dll", L"MMDevAPI.dll",
    L"AudioSes.dll", L"wintypes.dll", L"ExecModelClient.dll",
    L"resourcepolicyclient.dll", L"SmartContentProtection.dll",
    L"MpOav.dll",
  };

  hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (hSnap != INVALID_HANDLE_VALUE) {
    MODULEENTRY32W me = {};
    me.dwSize = sizeof(me);
    if (Module32FirstW(hSnap, &me)) {
      do {
        std::wstring modPath(me.szExePath);
        snapshot.processPaths.insert(modPath);

        wchar_t modNameLower[MAX_PATH] = {};
        wcscpy_s(modNameLower, me.szModule);
        for (wchar_t* p = modNameLower; *p; ++p) *p = towlower(*p);
        std::wstring modName(modNameLower);

        if (modName == L"monix.exe") {
          continue;
        }

        bool isSuspiciousName = (modName.find(L"hook") != std::wstring::npos ||
                                  modName.find(L"inject") != std::wstring::npos ||
                                  modName.find(L"detour") != std::wstring::npos);

        if (isSuspiciousName) {
          bool isKnownSafe = false;
          if (kKnownSafeModules.find(modName) != kKnownSafeModules.end()) {
            isKnownSafe = true;
          }
          wchar_t pathLowerCheck[MAX_PATH] = {};
          wcscpy_s(pathLowerCheck, me.szExePath);
          for (wchar_t* p = pathLowerCheck; *p; ++p) *p = towlower(*p);
          if (wcsstr(pathLowerCheck, L"\\windows\\system32\\") != nullptr ||
              wcsstr(pathLowerCheck, L"\\windows\\winsxs\\") != nullptr) {
            isKnownSafe = true;
          }

          if (!isKnownSafe) {
            WINTRUST_FILE_INFO modFileInfo = {};
            modFileInfo.cbStruct = sizeof(modFileInfo);
            modFileInfo.pcwszFilePath = me.szExePath;
            WINTRUST_DATA modWtData = {};
            modWtData.cbStruct = sizeof(modWtData);
            modWtData.dwUIChoice = WTD_UI_NONE;
            modWtData.fdwRevocationChecks = WTD_REVOKE_NONE;
            modWtData.dwUnionChoice = WTD_CHOICE_FILE;
            modWtData.dwStateAction = WTD_STATEACTION_VERIFY;
            modWtData.pFile = &modFileInfo;
            LONG modStatus = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &modWtData);
            modWtData.dwStateAction = WTD_STATEACTION_CLOSE;
            WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &modWtData);

            if (modStatus != ERROR_SUCCESS) {
              snapshot.hookModulesDetected++;
              snapshot.suspiciousModules.insert(me.szModule);
            }
          }
        }

        auto isKnownSafeModule = [&](const std::wstring& path, const std::wstring& name) -> bool {
          if (kKnownSafeModules.find(name) != kKnownSafeModules.end()) return true;
          wchar_t pathLower[MAX_PATH] = {};
          wcscpy_s(pathLower, path.c_str());
          for (wchar_t* p = pathLower; *p; ++p) *p = towlower(*p);
          if (wcsstr(pathLower, L"\\windows\\system32\\") != nullptr) return true;
          if (wcsstr(pathLower, L"\\windows\\winsxs\\") != nullptr) return true;
          if (wcsstr(pathLower, L"\\windows\\syswow64\\") != nullptr) return true;
          return false;
        };

        if (me.modBaseAddr && me.modBaseSize > sizeof(IMAGE_DOS_HEADER)) {
          PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(me.modBaseAddr);
          if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE &&
              dosHeader->e_lfanew > 0 &&
              static_cast<DWORD>(dosHeader->e_lfanew) < me.modBaseSize - sizeof(IMAGE_NT_HEADERS)) {
            PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
              reinterpret_cast<BYTE*>(me.modBaseAddr) + dosHeader->e_lfanew);
            if (ntHeaders->Signature == IMAGE_NT_SIGNATURE) {
              DWORD headerSize = ntHeaders->OptionalHeader.SizeOfHeaders;
              if (headerSize < me.modBaseSize && !isKnownSafeModule(modPath, modName)) {
                HANDLE hModFile = CreateFileW(me.szExePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                if (hModFile != INVALID_HANDLE_VALUE) {
                  DWORD modFileSize = GetFileSize(hModFile, nullptr);
                  if (modFileSize != INVALID_FILE_SIZE && modFileSize >= headerSize) {
                    std::vector<BYTE> diskHeader(headerSize);
                    DWORD br = 0;
                    if (ReadFile(hModFile, diskHeader.data(), headerSize, &br, nullptr)) {
                      std::vector<BYTE> memCopy(headerSize);
                      memcpy(memCopy.data(), me.modBaseAddr, headerSize);
                      auto zeroImageBase = [](std::vector<BYTE>& buf, DWORD lfOffset) {
                        if (lfOffset + sizeof(IMAGE_NT_HEADERS) > buf.size()) return;
                        WORD magic = *reinterpret_cast<WORD*>(buf.data() + lfOffset + 24);
                        DWORD imageBaseOffset = (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) ? 52 : 48;
                        if (lfOffset + imageBaseOffset + sizeof(void*) <= buf.size()) {
                          memset(buf.data() + lfOffset + imageBaseOffset, 0, sizeof(void*));
                        }
                      };
                      zeroImageBase(diskHeader, dosHeader->e_lfanew);
                      zeroImageBase(memCopy, dosHeader->e_lfanew);
                      if (memcmp(diskHeader.data(), memCopy.data(), headerSize) != 0) {
                        snapshot.peHeaderTamper++;
                        snapshot.suspiciousModules.insert(me.szModule);
                      }
                    }
                  }
                  CloseHandle(hModFile);
                }
              }
            }
          }
        }
      } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);
  }

  snapshot.scheduledTaskCount = 0;
  wchar_t taskDir[MAX_PATH] = {};
  if (GetWindowsDirectoryW(taskDir, MAX_PATH)) {
    std::wstring taskPath = std::wstring(taskDir) + L"\\System32\\Tasks";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW((taskPath + L"\\*").c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
          if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
            snapshot.scheduledTaskCount++;
          }
        }
      } while (FindNextFileW(hFind, &fd));
      FindClose(hFind);
    }
  }
}

}
