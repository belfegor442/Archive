#pragma once

#include <cstdint>
#include <string>

namespace monix {

struct FilesystemState {
  int system32FileCount = 0;
  int system32HiddenCount = 0;
  int system32SystemCount = 0;
  int driversFileCount = 0;
  int driversHiddenCount = 0;
  int recycleBinContentCount = 0;
  int programFilesCount = 0;
  int volumeDirtyBit = 0;
  std::wstring volumeLabel;
  unsigned long volumeSerial = 0;
  int mountPointCount = 0;
  int reparsePointCount = 0;
  int sparseFileCount = 0;
  int adsWithDataCount = 0;
  int integrityFailures = 0;
  unsigned long long usnJournalId = 0;
  long long lastUsn = 0;
  int corruptionWarnings = 0;
  int chkdskPending = 0;
};

}
