#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "MonixApp.hpp"
#include "app/bootstrap/AppConstants.hpp"
#include "app/bootstrap/CliParser.hpp"
#include "platform/win32/DpiSetup.hpp"
#include "crash/CrashHandler.hpp"
#include "core/StringUtils.hpp"
#include "core/FileUtils.hpp"
#include "shader/preprocessing/GlslPreprocessor.hpp"
#include "shader/preset_parser/GlslpPresetParser.hpp"
#include "logging/history/LogHistoryLoader.hpp"
#include "settings/SettingHelpers.hpp"
#include "ui/FontManager.hpp"
#include "app/lifecycle/RendererBootstrap.hpp"
#include "platform/win32/WindowFactory.hpp"

using namespace monix;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
  monix::SetupDpiAwareness();
  MonixApp app;
  auto cli = monix::ParseCliArgs(__argc, __wargv);
  if (cli.testMode) {
    app.SetTestMode();
    extern void runCompilationPipelineTests();
    runCompilationPipelineTests();
    extern void runMegadrivePresetTests();
    runMegadrivePresetTests();
    return 0;
  }
  if (cli.testVulkanMode) {
    extern int runVulkanRuntimeTests(HINSTANCE instance);
    return runVulkanRuntimeTests(instance);
  }
  if (cli.captureMode || cli.compareMode) {
    app.SetParityMode(cli.captureMode, cli.compareMode, cli.referenceDir);
  }
  if (cli.runtimeStateMode) {
    app.SetRuntimeStateMode(true, cli.retroarchSnapshotPath);
  }
  if (cli.regressionTestMode) {
    app.SetRegressionTestMode(true, cli.presetsDir, cli.frames);
  }
  return app.Run(instance, showCommand);
}
