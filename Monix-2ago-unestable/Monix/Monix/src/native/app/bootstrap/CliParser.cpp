#include "CliParser.hpp"

#include <cwchar>
#include <cstdlib>

namespace monix {

CliArgs ParseCliArgs(int argc, wchar_t** argv) {
  CliArgs args;
  for (int i = 1; i < argc; i++) {
    if (wcscmp(argv[i], L"--test") == 0 || wcscmp(argv[i], L"-test") == 0) args.testMode = true;
    if (wcscmp(argv[i], L"--test-vulkan") == 0 || wcscmp(argv[i], L"-test-vulkan") == 0) args.testVulkanMode = true;
    if (wcscmp(argv[i], L"--capture") == 0 || wcscmp(argv[i], L"-capture") == 0) args.captureMode = true;
    if (wcscmp(argv[i], L"--compare") == 0 || wcscmp(argv[i], L"-compare") == 0) args.compareMode = true;
    if ((wcscmp(argv[i], L"--reference") == 0 || wcscmp(argv[i], L"-reference") == 0) && i + 1 < argc) {
      args.referenceDir = argv[++i];
    }
    if (wcscmp(argv[i], L"--runtime-state") == 0 || wcscmp(argv[i], L"-runtime-state") == 0) args.runtimeStateMode = true;
    if ((wcscmp(argv[i], L"--retroarch-snapshot") == 0 || wcscmp(argv[i], L"-retroarch-snapshot") == 0) && i + 1 < argc) {
      args.retroarchSnapshotPath = argv[++i];
    }
    if (wcscmp(argv[i], L"--regression-test") == 0 || wcscmp(argv[i], L"-regression-test") == 0) args.regressionTestMode = true;
    if ((wcscmp(argv[i], L"--presets-dir") == 0 || wcscmp(argv[i], L"-presets-dir") == 0) && i + 1 < argc) {
      args.presetsDir = argv[++i];
    }
    if ((wcscmp(argv[i], L"--frames") == 0 || wcscmp(argv[i], L"-frames") == 0) && i + 1 < argc) {
      args.frames = _wtoi(argv[++i]);
    }
  }
  return args;
}

} // namespace monix
