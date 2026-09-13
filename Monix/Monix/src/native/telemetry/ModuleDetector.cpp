#include "ModuleDetector.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <wintrust.h>
#include <softpub.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <shlwapi.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shlwapi.lib")

namespace monix {

ModuleDetector::ModuleDetector() {
  Initialize();
}

void ModuleDetector::Initialize() {
  if (initialized_) return;

  knownSystemModules_ = {
    L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"advapi32.dll",
    L"sechost.dll", L"rpcrt4.dll", L"msvcrt.dll", L"ucrtbase.dll",
    L"user32.dll", L"gdi32.dll", L"gdiplus.dll", L"shell32.dll",
    L"shlwapi.dll", L"comctl32.dll", L"comdlg32.dll", L"ole32.dll",
    L"oleaut32.dll", L"shell32.dll", L"explorer.exe", L"csrss.exe",
    L"smss.exe", L"lsass.exe", L"services.exe", L"winlogon.exe",
    L"wininit.exe", L"svchost.exe", L"fontdrvhost.exe", L"dwm.exe",
    L"conhost.exe", L"sihost.exe", L"taskhostw.exe", L"RuntimeBroker.exe",
    L"SearchUI.exe", L"ShellExperienceHost.exe", L"StartMenuExperienceHost.exe",
    L"SettingSyncHost.exe", L"GameBarPresenceWriter.exe", L"SecurityHealthService.exe",
    L"MpCmdRun.exe", L"MsMpEng.exe", L"NisSrv.exe", L"SearchIndexer.exe",
    L"SearchProtocolHost.exe", L"SearchFilterHost.exe", L"backgroundTaskHost.exe",
    L"LockApp.exe", L"SystemSettings.exe", L"SpeechRuntime.exe",
    L"Windows.UI.Logon.dll", L"Windows.UI.Shell.dll", L"Windows.UI.Xaml.dll",
    L"Windows.Storage.ApplicationData.dll", L"Windows.ApplicationModel.dll",
    L"Windows.Networking.dll", L"Windows.Foundation.dll", L"Windows.System.dll",
    L"iertutil.dll", L"ws2_32.dll", L"winhttp.dll", L"wininet.dll",
    L"iphlpapi.dll", L"mswsock.dll", L"nlaapi.dll", L"dnsapi.dll",
    L"crypt32.dll", L"bcrypt.dll", L"ncrypt.dll", L"wintrust.dll",
    L"cryptsp.dll", L"bcryptprimitives.dll", L"ntmarta.dll", L"wldap32.dll",
    L"userenv.dll", L"profapi.dll", L"clr.dll", L"mscoree.dll",
    L"mscorlib.dll", L"combase.dll", L"ieframe.dll", L"jscript.dll",
    L"vbscript.dll", L"scrrun.dll", L"mshtml.dll", L"urlmon.dll",
    L"shdocvw.dll", L"winmm.dll", L"msacm32.dll", L"avicap32.dll",
    L"setupapi.dll", L"devobj.dll", L"cfgmgr32.dll", L"newdev.dll",
    L"pnputil.dll", L"drvstore.dll", L"infapi.dll", L"fltlib.dll",
    L"win32u.dll", L"win32kfull.sys", L"win32kbase.sys", L"dxgkrnl.sys",
    L"dxgmms1.sys", L"dxgmms2.sys", L"ndis.sys", L"tcpip.sys",
    L"afd.sys", L"fltmgr.sys", L"fileinfo.sys", L"luafv.sys",
    L"msrpc.sys", L"ksecdd.sys", L"cng.sys", L"ci.dll",
    L"msvcp_win.dll", L"apphelp.dll", L"dbgcore.dll", L"dbghelp.dll",
    L"dbgeng.dll", L"wer.dll", L"faultrep.dll", L"werdiag.dll",
    L"version.dll", L"mpsvc.dll", L"SecurityHealthProxyStub.dll",
  };

  knownSafeModules_ = knownSystemModules_;
  knownSafeModules_.insert({
    L"vulkan-1.dll", L"opencl.dll", L"opengl32.dll", L"d3d11.dll",
    L"d3d12.dll", L"dxgi.dll", L"dcomp.dll", L"dwmcore.dll",
    L"nvwgf2umx.dll", L"nvwgf2um.dll", L"nvd3d9wrap.dll",
    L"nvllddmm.dll", L"nvoglv64.dll", L"nvoglv32.dll",
    L"amdihk64.dll", L"amdocl64.dll", L"atiumd64.dll", L"atiumdag.dll",
    L"atiadlxx.dll", L"atio6axx.dll", L"atig6txx.dll", L"atig6pxx.dll",
    L"IntelOpenCLICD.dll", L"igdrcl.dll", L"igdgmm64.dll",
    L"gdiplus.dll", L"msvcp140.dll", L"vcruntime140.dll", L"vcruntime140_1.dll",
    L"concrt140.dll", L"api-ms-win-*.dll", L"ext-ms-*.dll",
    L"kernel.appcore.dll", L"windows.storage.dll", L"twinapi.appcore.dll",
    L"twinui.appcore.dll", L"rmclient.dll", L"eventaggregation.dll",
    L"windows.graphics.dll", L"windows.data.pdf.dll",
  });

  suspiciousNamePatterns_ = {
    L"hook", L"inject", L"detour", L"inject", L"spy",
    L"keylog", L"capture", L"sniff", L"monitor", L"backdoor",
    L"trojan", L"rootkit", L"stealer", L"grabber", L"loader",
  };

  whitelistedHookModules_ = {
    L"winhttp.dll", L"ws2_32.dll", L"iphlpapi.dll",
    L"dbghelp.dll", L"dbgcore.dll", L"dbgeng.dll",
    L"version.dll", L"shlwapi.dll", L"comctl32.dll",
    L"dwmapi.dll", L"dcomp.dll", L"dxgi.dll",
    L"nvwgf2umx.dll", L"nvwgf2um.dll", L"amdihk64.dll",
    L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll",
  };

  initialized_ = true;
}

bool ModuleDetector::IsKnownSystemModule(const std::wstring& moduleName) const {
  return knownSystemModules_.find(moduleName) != knownSystemModules_.end();
}

bool ModuleDetector::IsKnownSafeModule(const std::wstring& moduleName) const {
  return knownSafeModules_.find(moduleName) != knownSafeModules_.end();
}

std::wstring ModuleDetector::GetModuleFullPath(HMODULE hModule) const {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(hModule, path, MAX_PATH);
  return path;
}

std::uint64_t ModuleDetector::GetModuleFileSize(const std::wstring& path) const {
  HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
    nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) return 0;
  LARGE_INTEGER size{};
  GetFileSizeEx(hFile, &size);
  CloseHandle(hFile);
  return static_cast<std::uint64_t>(size.QuadPart);
}

std::uint32_t ModuleDetector::GetModuleTimestamp(const std::wstring& path) const {
  HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
    nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) return 0;
  FILETIME ft{};
  GetFileTime(hFile, nullptr, nullptr, &ft);
  CloseHandle(hFile);
  ULARGE_INTEGER ul;
  ul.LowPart = ft.dwLowDateTime;
  ul.HighPart = ft.dwHighDateTime;
  return static_cast<std::uint32_t>(ul.QuadPart & 0xFFFFFFFF);
}

bool ModuleDetector::IsSystemDirectory(const std::wstring& path) const {
  std::wstring lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
  return lower.find(L"\\windows\\system32\\") != std::wstring::npos ||
         lower.find(L"\\windows\\syswow64\\") != std::wstring::npos ||
         lower.find(L"\\windows\\winsxs\\") != std::wstring::npos ||
         lower.find(L"\\windows\\servicing\\") != std::wstring::npos ||
         lower.find(L"\\windows\\microsoft.net\\") != std::wstring::npos;
}

int ModuleDetector::CheckProtectionStatus(HMODULE hModule) const {
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(hModule, &mbi, sizeof(mbi)) == 0) return -1;
  DWORD oldProtect = mbi.Protect;
  if (oldProtect & PAGE_EXECUTE) return 1;
  if (oldProtect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) return 0;
  if (oldProtect & PAGE_NOACCESS) return 2;
  return 0;
}

bool ModuleDetector::VerifyModuleSignature(const std::wstring& filePath, int& signatureValid,
                                           std::wstring& signerName, std::wstring& issuerName) const {
  WINTRUST_FILE_INFO fileInfo{};
  fileInfo.cbStruct = sizeof(fileInfo);
  fileInfo.pcwszFilePath = filePath.c_str();
  fileInfo.hFile = nullptr;
  fileInfo.pgKnownSubject = nullptr;

  GUID actionId = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  WINTRUST_DATA wintrustData{};
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

  if (status == ERROR_SUCCESS) {
    signatureValid = 1;
    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;
    if (CryptQueryObject(CERT_QUERY_OBJECT_FILE, filePath.c_str(),
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr, nullptr, &hStore, &hMsg, nullptr)) {
      DWORD signerInfoSize = 0;
      CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &signerInfoSize);
      if (signerInfoSize > 0) {
        std::vector<char> signerInfoBuf(signerInfoSize);
        if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0,
            signerInfoBuf.data(), &signerInfoSize)) {
          PCMSG_SIGNER_INFO pSignerInfo = reinterpret_cast<PCMSG_SIGNER_INFO>(signerInfoBuf.data());
          CERT_INFO certInfo{};
          certInfo.Issuer = pSignerInfo->Issuer;
          certInfo.SerialNumber = pSignerInfo->SerialNumber;
          PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
            hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
            CERT_FIND_SUBJECT_CERT, &certInfo, nullptr);
          if (pCertContext) {
            wchar_t nameBuf[256]{};
            CertGetNameStringW(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
              0, nullptr, nameBuf, sizeof(nameBuf) / sizeof(wchar_t));
            signerName = nameBuf;
            wchar_t issuerBuf[256]{};
            CertGetNameStringW(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
              0, (void*)szOID_COMMON_NAME, issuerBuf, sizeof(issuerBuf) / sizeof(wchar_t));
            issuerName = issuerBuf;
            CertFreeCertificateContext(pCertContext);
          }
        }
      }
      if (hStore) CertCloseStore(hStore, 0);
      if (hMsg) CryptMsgClose(hMsg);
    }
  } else if (status == TRUST_E_NOSIGNATURE) {
    signatureValid = 0;
  } else {
    signatureValid = 0;
  }

  wintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionId, &wintrustData);

  return signatureValid == 1;
}

std::wstring ModuleDetector::ComputeModuleHash(const std::wstring& filePath) const {
  HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
    nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) return L"";

  BCRYPT_ALG_HANDLE hAlg = nullptr;
  BCRYPT_HASH_HANDLE hHash = nullptr;
  std::wstring result;

  if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
    if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
      BYTE buffer[8192]{};
      DWORD bytesRead = 0;
      while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        BCryptHashData(hHash, buffer, bytesRead, 0);
      }
      BYTE hash[32]{};
      BCryptFinishHash(hHash, hash, sizeof(hash), 0);
      wchar_t hex[65]{};
      for (int i = 0; i < 32; ++i) {
        swprintf_s(hex + i * 2, 3, L"%02x", hash[i]);
      }
      result = hex;
      BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
  }

  CloseHandle(hFile);
  return result;
}

bool ModuleDetector::DetectHookIndicators(const ModuleEvidence& module) const {
  if (module.isKnownSafe) return false;
  if (module.isSystemModule) return false;

  std::wstring lowerName = module.name;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
  for (const auto& pattern : suspiciousNamePatterns_) {
    if (lowerName.find(pattern) != std::wstring::npos) {
      return true;
    }
  }
  if (whitelistedHookModules_.find(lowerName) != whitelistedHookModules_.end()) {
    return false;
  }
  return false;
}

bool ModuleDetector::DetectPeHeaderTamper(const ModuleEvidence& module) const {
  if (module.fullPath.empty()) return false;
  if (IsSystemDirectory(module.fullPath)) return false;

  HANDLE hFile = CreateFileW(module.fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
    nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) return false;

  DWORD fileSize = GetFileSize(hFile, nullptr);
  if (fileSize < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS)) {
    CloseHandle(hFile);
    return false;
  }

  std::vector<char> fileData(fileSize);
  DWORD bytesRead = 0;
  if (!ReadFile(hFile, fileData.data(), fileSize, &bytesRead, nullptr) || bytesRead != fileSize) {
    CloseHandle(hFile);
    return false;
  }
  CloseHandle(hFile);

  auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(fileData.data());
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

  auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(fileData.data() + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

  HMODULE hModule = GetModuleHandleW(module.fullPath.c_str());
  if (!hModule) return false;

  auto* memNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
    reinterpret_cast<char*>(hModule) + dosHeader->e_lfanew);

  if (ntHeaders->OptionalHeader.ImageBase != memNtHeaders->OptionalHeader.ImageBase) {
    return true;
  }
  if (ntHeaders->FileHeader.TimeDateStamp != memNtHeaders->FileHeader.TimeDateStamp) {
    return true;
  }

  return false;
}

void ModuleDetector::EnumerateProcessModules(DWORD processId, std::vector<ModuleEvidence>& outModules) {
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
  if (hSnap == INVALID_HANDLE_VALUE) return;

  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);

  if (Module32FirstW(hSnap, &me)) {
    do {
      ModuleEvidence mod;
      mod.name = me.szModule;
      mod.fullPath = me.szExePath;
      mod.pid = static_cast<int>(me.th32ProcessID);
      mod.fileSize = me.modBaseSize;
      mod.machineType = 0;
      mod.state = DataState::Valid;

      std::wstring lowerName = mod.name;
      std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

      mod.isSystemModule = IsSystemDirectory(mod.fullPath);
      mod.isKnownSafe = IsKnownSafeModule(lowerName);

      mod.hookIndicators = DetectHookIndicators(mod);

      if (!mod.fullPath.empty() && mod.fileSize > 0) {
        mod.signatureValid = -1;
        mod.signerName = L"";
        mod.issuerName = L"";
        VerifyModuleSignature(mod.fullPath, mod.signatureValid, mod.signerName, mod.issuerName);

        mod.sha256Hash = ComputeModuleHash(mod.fullPath);

        if (!mod.isKnownSafe && !mod.isSystemModule) {
          mod.peHeaderTamper = DetectPeHeaderTamper(mod);
        }
      }

      mod.protectionStatus = CheckProtectionStatus(me.hModule);

      if (mod.hookIndicators && !mod.isKnownSafe) {
        mod.classification = ModuleClassification::Suspicious;
        mod.suspiciousIndicators.push_back(L"Name matches suspicious pattern");
      } else if (mod.peHeaderTamper) {
        mod.classification = ModuleClassification::Anomalous;
        mod.suspiciousIndicators.push_back(L"PE header mismatch between disk and memory");
      } else if (mod.signatureValid == 0 && !mod.isKnownSafe) {
        mod.classification = ModuleClassification::Unsigned;
        mod.suspiciousIndicators.push_back(L"No valid Authenticode signature");
      } else if (mod.signatureValid == 1 && !mod.isKnownSafe) {
        mod.classification = ModuleClassification::SignedUnknown;
      } else if (mod.isKnownSafe) {
        mod.classification = ModuleClassification::Trusted;
      } else if (mod.isSystemModule) {
        mod.classification = ModuleClassification::Known;
      } else {
        mod.classification = ModuleClassification::Unknown;
      }

      outModules.push_back(std::move(mod));
    } while (Module32NextW(hSnap, &me));
  }

  CloseHandle(hSnap);
}

void ModuleDetector::CollectModules(DWORD processId, std::vector<ModuleEvidence>& outModules) {
  EnumerateProcessModules(processId, outModules);
}

void ModuleDetector::CollectAllProcessModules(std::vector<ModuleEvidence>& outModules) {
  outModules.clear();

  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return;

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);

  if (Process32FirstW(hSnap, &pe)) {
    do {
      std::vector<ModuleEvidence> processModules;
      CollectModules(pe.th32ProcessID, processModules);
      for (auto& mod : processModules) {
        mod.processName = pe.szExeFile;
      }
      outModules.insert(outModules.end(),
        std::make_move_iterator(processModules.begin()),
        std::make_move_iterator(processModules.end()));
    } while (Process32NextW(hSnap, &pe));
  }

  CloseHandle(hSnap);
  lastCollected_ = outModules;
}

void ModuleDetector::ClassifyModule(ModuleEvidence& module) const {
  std::wstring lowerName = module.name;
  std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);

  module.isSystemModule = IsSystemDirectory(module.fullPath);
  module.isKnownSafe = IsKnownSafeModule(lowerName);
  module.hookIndicators = DetectHookIndicators(module);

  if (module.hookIndicators && !module.isKnownSafe) {
    module.classification = ModuleClassification::Suspicious;
    module.suspiciousIndicators.push_back(L"Name matches suspicious pattern");
  } else if (module.peHeaderTamper) {
    module.classification = ModuleClassification::Anomalous;
    module.suspiciousIndicators.push_back(L"PE header mismatch");
  } else if (module.signatureValid == 0 && !module.isKnownSafe) {
    module.classification = ModuleClassification::Unsigned;
  } else if (module.signatureValid == 1 && !module.isKnownSafe) {
    module.classification = ModuleClassification::SignedUnknown;
  } else if (module.isKnownSafe) {
    module.classification = ModuleClassification::Trusted;
  } else if (module.isSystemModule) {
    module.classification = ModuleClassification::Known;
  } else {
    module.classification = ModuleClassification::Unknown;
  }
}

void ModuleDetector::SetBaseline(const std::vector<ModuleEvidence>& baselineModules) {
  baselineModules_ = baselineModules;
}

void ModuleDetector::DetectChanges(const std::vector<ModuleEvidence>& currentModules,
                                   std::vector<EnvironmentChange>& outChanges) const {
  std::map<std::wstring, const ModuleEvidence*> baselineMap;
  for (const auto& mod : baselineModules_) {
    std::wstring key = mod.fullPath + L"|" + std::to_wstring(mod.pid);
    baselineMap[key] = &mod;
  }

  std::set<std::wstring> currentKeys;
  for (const auto& mod : currentModules) {
    std::wstring key = mod.fullPath + L"|" + std::to_wstring(mod.pid);
    currentKeys.insert(key);

    auto it = baselineMap.find(key);
    if (it == baselineMap.end()) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Module;
      change.kind = EnvironmentChange::ChangeKind::Added;
      change.entityName = mod.name;
      change.entityPath = mod.fullPath;
      change.processName = mod.processName;
      change.pid = mod.pid;
      change.evidence = L"classification=" + std::wstring(ModuleClassificationName(mod.classification));
      if (!mod.sha256Hash.empty()) {
        change.evidence += L" hash=" + mod.sha256Hash;
      }
      if (!mod.signerName.empty()) {
        change.evidence += L" signer=" + mod.signerName;
      }
      change.significant = (mod.classification != ModuleClassification::Trusted &&
                            mod.classification != ModuleClassification::Known);
      outChanges.push_back(std::move(change));
    } else {
      const ModuleEvidence& prev = *it->second;
      if (prev.sha256Hash != mod.sha256Hash && !mod.sha256Hash.empty()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Module;
        change.kind = EnvironmentChange::ChangeKind::Modified;
        change.entityName = mod.name;
        change.entityPath = mod.fullPath;
        change.processName = mod.processName;
        change.pid = mod.pid;
        change.oldValue = L"hash=" + prev.sha256Hash;
        change.newValue = L"hash=" + mod.sha256Hash;
        change.evidence = L"Module hash changed";
        change.significant = true;
        outChanges.push_back(std::move(change));
      }
      if (prev.signatureValid != mod.signatureValid) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Module;
        change.kind = EnvironmentChange::ChangeKind::StateChanged;
        change.entityName = mod.name;
        change.entityPath = mod.fullPath;
        change.processName = mod.processName;
        change.pid = mod.pid;
        change.oldValue = L"signed=" + std::to_wstring(prev.signatureValid);
        change.newValue = L"signed=" + std::to_wstring(mod.signatureValid);
        change.evidence = L"Signature state changed";
        change.significant = true;
        outChanges.push_back(std::move(change));
      }
      if (prev.classification != mod.classification) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Module;
        change.kind = EnvironmentChange::ChangeKind::StateChanged;
        change.entityName = mod.name;
        change.entityPath = mod.fullPath;
        change.processName = mod.processName;
        change.pid = mod.pid;
        change.oldValue = std::wstring(ModuleClassificationName(prev.classification));
        change.newValue = std::wstring(ModuleClassificationName(mod.classification));
        change.evidence = L"Classification changed";
        change.significant = (mod.classification == ModuleClassification::Suspicious ||
                              mod.classification == ModuleClassification::Anomalous ||
                              mod.classification == ModuleClassification::Unsigned);
        outChanges.push_back(std::move(change));
      }
    }
  }

  for (const auto& [key, prevPtr] : baselineMap) {
    if (currentKeys.find(key) == currentKeys.end()) {
      const ModuleEvidence& prev = *prevPtr;
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Module;
      change.kind = EnvironmentChange::ChangeKind::Removed;
      change.entityName = prev.name;
      change.entityPath = prev.fullPath;
      change.processName = prev.processName;
      change.pid = prev.pid;
      change.evidence = L"Module unloaded";
      change.significant = false;
      outChanges.push_back(std::move(change));
    }
  }
}

} // namespace monix
