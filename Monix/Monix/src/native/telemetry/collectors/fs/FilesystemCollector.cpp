#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shlobj.h>

#include <cstdint>
#include <string>
#include <vector>

#include "FilesystemCollector.hpp"

namespace monix {

void ScanDirectory(const wchar_t* path, int& totalFiles, int& hiddenFiles, int& systemFiles, int& reparsePoints, int& sparseFiles) {
  totalFiles = 0;
  hiddenFiles = 0;
  systemFiles = 0;
  reparsePoints = 0;
  sparseFiles = 0;
  WIN32_FIND_DATAW fd = {};
  std::wstring searchPath = std::wstring(path) + L"\\*";
  HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
  if (hFind == INVALID_HANDLE_VALUE) return;
  do {
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
    totalFiles++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) hiddenFiles++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) systemFiles++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) reparsePoints++;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) sparseFiles++;
  } while (FindNextFileW(hFind, &fd));
  FindClose(hFind);
}

int CountAlternateDataStreams(const wchar_t* filePath) {
  int adsCount = 0;
  WIN32_FIND_STREAM_DATA streamData = {};
  HANDLE hStream = FindFirstStreamW(filePath, FindStreamInfoStandard, &streamData, 0);
  if (hStream != INVALID_HANDLE_VALUE) {
    do {
      if (streamData.cStreamName[0] != L':') {
        adsCount++;
      }
    } while (FindNextStreamW(hStream, &streamData));
    FindClose(hStream);
  }
  return adsCount;
}

void CollectFilesystemData(Snapshot& snapshot) {
  snapshot.fsSystem32FileCount = 0;
  snapshot.fsSystem32HiddenCount = 0;
  snapshot.fsSystem32SystemCount = 0;
  snapshot.fsDriversFileCount = 0;
  snapshot.fsDriversHiddenCount = 0;
  snapshot.fsRecycleBinContentCount = 0;
  snapshot.fsProgramFilesCount = 0;
  snapshot.fsVolumeDirtyBit = 0;
  snapshot.fsVolumeLabel.clear();
  snapshot.fsVolumeSerial = 0;
  snapshot.fsMountPointCount = 0;
  snapshot.fsReparsePointCount = 0;
  snapshot.fsSparseFileCount = 0;
  snapshot.fsAdsWithDataCount = 0;
  snapshot.fsIntegrityFailures = 0;
  snapshot.fsUsnJournalId = 0;
  snapshot.fsLastUsn = 0;
  snapshot.fsCorruptionWarnings = 0;
  snapshot.fsChkdskPending = 0;

  wchar_t winDir[MAX_PATH] = {};
  GetWindowsDirectoryW(winDir, MAX_PATH);
  std::wstring sys32 = std::wstring(winDir) + L"\\System32";
  std::wstring drivers = std::wstring(winDir) + L"\\System32\\drivers";
  std::wstring recycle = L"";

  wchar_t recycleBinPath[MAX_PATH] = {};
  if (SHGetFolderPathW(nullptr, CSIDL_BITBUCKET, nullptr, 0, recycleBinPath) == S_OK) {
    recycle = recycleBinPath;
  }

  ScanDirectory(sys32.c_str(), snapshot.fsSystem32FileCount, snapshot.fsSystem32HiddenCount, snapshot.fsSystem32SystemCount, snapshot.fsReparsePointCount, snapshot.fsSparseFileCount);
  int driversTotal = 0, driversHidden = 0, driversSys = 0, driversReparse = 0, driversSparse = 0;
  ScanDirectory(drivers.c_str(), driversTotal, driversHidden, driversSys, driversReparse, driversSparse);
  snapshot.fsDriversFileCount = driversTotal;
  snapshot.fsDriversHiddenCount = driversHidden;
  snapshot.fsReparsePointCount += driversReparse;
  snapshot.fsSparseFileCount += driversSparse;

  WIN32_FIND_DATAW fd = {};
  std::wstring recyclePath = recycle + L"\\*";
  HANDLE hFind = FindFirstFileW(recyclePath.c_str(), &fd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
        snapshot.fsRecycleBinContentCount++;
      }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
  }

  wchar_t progFiles[MAX_PATH] = {};
  if (GetEnvironmentVariableW(L"ProgramFiles", progFiles, MAX_PATH)) {
    int pfTotal = 0, pfHidden = 0, pfSys = 0, pfReparse = 0, pfSparse = 0;
    ScanDirectory(progFiles, pfTotal, pfHidden, pfSys, pfReparse, pfSparse);
    snapshot.fsProgramFilesCount = pfTotal;
    snapshot.fsReparsePointCount += pfReparse;
  }

  wchar_t volRoot[] = {L'C', L':', L'\\', 0};
  wchar_t volLabel[MAX_PATH + 1] = {};
  DWORD volSerial = 0;
  DWORD maxCompLen = 0;
  DWORD fsFlags = 0;
  if (GetVolumeInformationW(volRoot, volLabel, MAX_PATH + 1, &volSerial, &maxCompLen, &fsFlags, nullptr, 0)) {
    snapshot.fsVolumeLabel = volLabel;
    snapshot.fsVolumeSerial = volSerial;
    if (fsFlags & 0x00000002) {
      snapshot.fsVolumeDirtyBit = 1;
      snapshot.fsChkdskPending = 1;
    }
    if (fsFlags & FILE_PERSISTENT_ACLS) {
    }
  }

  DWORD mountCount = 0;
  wchar_t mountPoints[8192] = {};
  if (GetVolumePathNamesForVolumeNameW(L"C:\\", mountPoints, 8192, &mountCount)) {
    snapshot.fsMountPointCount = 0;
    wchar_t* p = mountPoints;
    while (*p) {
      snapshot.fsMountPointCount++;
      p += wcslen(p) + 1;
    }
  }

  wchar_t testPath[MAX_PATH] = {};
  wcscpy_s(testPath, winDir);
  wcscat_s(testPath, L"\\System32\\ntoskrnl.exe");
  snapshot.fsAdsWithDataCount = CountAlternateDataStreams(testPath);

  if (snapshot.fsVolumeDirtyBit == 0) {
    snapshot.fsCorruptionWarnings = 0;
  }
}

}
