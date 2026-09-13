#pragma once

#include <string>

namespace monix {

struct CliArgs {
  bool testMode = false;
  bool testVulkanMode = false;
  bool captureMode = false;
  bool compareMode = false;
  std::wstring referenceDir;
  bool runtimeStateMode = false;
  std::wstring retroarchSnapshotPath;
  bool regressionTestMode = false;
  std::wstring presetsDir;
  int frames = 3;
};

CliArgs ParseCliArgs(int argc, wchar_t** argv);

} // namespace monix
