$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $root "src\native\main.cpp"
$monixAppSource = Join-Path $root "src\native\MonixApp.cpp"
$cliParserSource = Join-Path $root "src\native\app\bootstrap\CliParser.cpp"
$pipelineTestsSource = Join-Path $root "src\native\tests\renderer_vk\test_pipeline.cpp"
$runtimeTestsSource = Join-Path $root "src\native\tests\renderer_vk\test_runtime.cpp"
$crashHandlerSource = Join-Path $root "src\native\crash\CrashHandler.cpp"
$vulkanRendererSource = Join-Path $root "src\native\vulkan_renderer.cpp"
$rendererVkSource = Join-Path $root "src\native\renderer_vk_unity.cpp"
$settingsRegistrySource = Join-Path $root "src\native\settings\SettingsRegistry.cpp"
$settingDefsSource = Join-Path $root "src\native\settings\registry\SettingDefs.cpp"
$settingSerializerSource = Join-Path $root "src\native\settings\serialization\SettingSerializer.cpp"
$textUtilsSource = Join-Path $root "src\native\core\TextUtils.cpp"
$collectorsSource = Join-Path $root "src\native\telemetry\Collectors.cpp"
$projectRoot = Split-Path (Split-Path $root -Parent) -Parent
$kernelDiagSource = Join-Path $projectRoot "src\native\kernel\KernelDiagnostics.cpp"
$loginSource = Join-Path $root "login\LoginOverlay.cpp"
$monixKernelSource = Join-Path $root "login\MonixKernel.cpp"
$kernelDisplaySource = Join-Path $root "login\KernelDisplay.cpp"
$bootUpSource = Join-Path $root "login\BootUp.cpp"
$bootUpDisplaySource = Join-Path $root "login\BootUpDisplay.cpp"
$gdiPlusLoaderSource = Join-Path $root "login\GdiPlusLoader.cpp"
$glBackendSource = Join-Path $root "src\native\renderer_vk\opengl\GlBackend.cpp"
$hwCSource = Join-Path $root "sensors\hardware.c"
$cpuAsmSource = Join-Path $root "sensors\cpu.asm"
$resourceFile = Join-Path $root "src\native\resources.rc"
$icoFile = Join-Path $root "ico.ico"
$rcedit = Join-Path $root "tools\rcedit\rcedit.exe"
$outputDir = Join-Path $root "build"
$output = Join-Path $outputDir "Monix.exe"
$resOutput = Join-Path $outputDir "resources.res"
$objOutput = Join-Path $outputDir "main.obj"
$monixAppObjOutput = Join-Path $outputDir "MonixApp.obj"
$cliParserObjOutput = Join-Path $outputDir "CliParser.obj"
$pipelineTestsObjOutput = Join-Path $outputDir "test_pipeline.obj"
$runtimeTestsObjOutput = Join-Path $outputDir "test_runtime.obj"
$crashHandlerObjOutput = Join-Path $outputDir "CrashHandler.obj"
$vkSourceDir = Join-Path $root "src\native\renderer_vk\vk"
$vulkanRendererObjOutput = Join-Path $outputDir "vulkan_renderer.obj"
$rendererVkObjOutput = Join-Path $outputDir "renderer_vk_unity.obj"
$settingsRegistryObjOutput = Join-Path $outputDir "SettingsRegistry.obj"
$settingDefsObjOutput = Join-Path $outputDir "SettingDefs.obj"
$settingSerializerObjOutput = Join-Path $outputDir "SettingSerializer.obj"
$textUtilsObjOutput = Join-Path $outputDir "TextUtils.obj"
$collectorsObjOutput = Join-Path $outputDir "Collectors.obj"
$kernelDiagObjOutput = Join-Path $outputDir "KernelDiagnostics.obj"
$loginObjOutput = Join-Path $outputDir "LoginOverlay.obj"
$monixKernelObjOutput = Join-Path $outputDir "MonixKernel.obj"
$kernelDisplayObjOutput = Join-Path $outputDir "KernelDisplay.obj"
$bootUpObjOutput = Join-Path $outputDir "BootUp.obj"
$bootUpDisplayObjOutput = Join-Path $outputDir "BootUpDisplay.obj"
$gdiPlusLoaderObjOutput = Join-Path $outputDir "GdiPlusLoader.obj"
$glBackendObjOutput = Join-Path $outputDir "GlBackend.obj"
$hwCObjOutput = Join-Path $outputDir "hardware.obj"
$cpuAsmObjOutput = Join-Path $outputDir "cpu.obj"

New-Item -ItemType Directory -Force $outputDir | Out-Null

# --- Locate Visual Studio Build Tools vcvarsall.bat ---
$vcvarsall = Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio" -Recurse -Filter "vcvarsall.bat" -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -like "*BuildTools*" -or $_.FullName -like "*Community*" -or $_.FullName -like "*Professional*" -or $_.FullName -like "*Enterprise*" } |
  Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvarsall) { throw "vcvarsall.bat not found. Install Visual Studio Build Tools." }
Write-Host "Using vcvarsall: $vcvarsall"

# --- Locate Windows SDK for rc.exe ---
$sdkBase = "C:\Program Files (x86)\Windows Kits\10\Include"
$sdkVer = Get-ChildItem $sdkBase -Directory -ErrorAction SilentlyContinue |
  Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $sdkVer) { throw "Windows SDK not found." }
$sdkInc = Join-Path $sdkBase $sdkVer
$sdkBin = "C:\Program Files (x86)\Windows Kits\10\bin\$sdkVer"
Write-Host "Using Windows SDK: $sdkVer"

# --- Compile resources with rc.exe ---
Write-Host "Compiling resources..."
$rcExe = Join-Path $sdkBin "x64\rc.exe"
$resCompiled = $false
if (Test-Path $rcExe) {
  & $rcExe /I "$sdkInc\um" /I "$sdkInc\shared" /fo $resOutput $resourceFile
  if ($LASTEXITCODE -eq 0) {
    $resCompiled = $true
  } else {
    Write-Host "Warning: rc.exe failed, continuing without resources"
    $resOutput = ""
  }
} else {
  Write-Host "Warning: rc.exe not found, continuing without resources"
  $resOutput = ""
}

# --- Build the compile+link commands via vcvarsall ---
# We generate a batch script that sources vcvarsall then runs cl.exe and link.exe
$buildBat = Join-Path $outputDir "build-temp.bat"
$linkArgsStr = "/subsystem:windows /entry:wWinMainCRTStartup /map:""$outputDir\Monix.map"" /out:""$output"" ""$objOutput"" ""$monixAppObjOutput"" ""$cliParserObjOutput"" ""$pipelineTestsObjOutput"" ""$runtimeTestsObjOutput"" ""$crashHandlerObjOutput"" ""$vulkanRendererObjOutput"" ""$rendererVkObjOutput"" ""$settingsRegistryObjOutput"" ""$settingDefsObjOutput"" ""$settingSerializerObjOutput"" ""$textUtilsObjOutput"" ""$collectorsObjOutput"" ""$kernelDiagObjOutput"" ""$loginObjOutput"" ""$monixKernelObjOutput"" ""$kernelDisplayObjOutput"" ""$bootUpObjOutput"" ""$bootUpDisplayObjOutput"" ""$gdiPlusLoaderObjOutput"" ""$glBackendObjOutput"" ""$hwCObjOutput"" ""$cpuAsmObjOutput"""
if ($resCompiled -and (Test-Path $resOutput)) {
  $linkArgsStr += " ""$resOutput"""
}
$linkArgsStr += " user32.lib gdi32.lib psapi.lib comdlg32.lib shell32.lib shlwapi.lib iphlpapi.lib ws2_32.lib ole32.lib opengl32.lib uuid.lib winmm.lib kernel32.lib ntdll.lib setupapi.lib bcrypt.lib wintrust.lib crypt32.lib advapi32.lib powrprof.lib gdiplus.lib pdh.lib winhttp.lib"
# Ultra Logger .obj files
$ulObjFiles = @()
$coreDirs = @("events", "eventbus", "validation", "collectors", "security", "integration", "platform")
foreach ($d in $coreDirs) {
  $cppFiles = Get-ChildItem -Path (Join-Path $projectRoot "src\core\$d") -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue
  foreach ($f in $cppFiles) {
    $ulObjFiles += "`"$outputDir\$($f.BaseName).obj`""
  }
}
$winPlatFiles = Get-ChildItem -Path (Join-Path $projectRoot "src\core\platform\windows") -Filter "*.cpp" -ErrorAction SilentlyContinue
foreach ($f in $winPlatFiles) {
  $ulObjFiles += "`"$outputDir\$($f.BaseName).obj`""
}
# Test .obj files
$testDirs = @("events", "eventbus", "validation")
foreach ($d in $testDirs) {
  $testFiles = Get-ChildItem -Path (Join-Path $projectRoot "tests\core\$d") -Filter "*Tests.cpp" -ErrorAction SilentlyContinue
  foreach ($f in $testFiles) {
    $ulObjFiles += "`"$outputDir\$($f.BaseName).obj`""
  }
}
$collectorTestDirs = Get-ChildItem -Path (Join-Path $projectRoot "tests\core\collectors") -Directory -ErrorAction SilentlyContinue
foreach ($td in $collectorTestDirs) {
  $testFiles = Get-ChildItem -Path $td.FullName -Filter "*Tests.cpp" -ErrorAction SilentlyContinue
  foreach ($f in $testFiles) {
    $ulObjFiles += "`"$outputDir\$($f.BaseName).obj`""
  }
}
$collectorRootTests = Get-ChildItem -Path (Join-Path $projectRoot "tests\core\collectors") -Filter "*Tests.cpp" -File -ErrorAction SilentlyContinue
foreach ($f in $collectorRootTests) {
  $ulObjFiles += "`"$outputDir\$($f.BaseName).obj`""
}
$ulObjFiles += "`"$outputDir\KernelSelfTest.obj`""
foreach ($obj in $ulObjFiles) {
  $linkArgsStr += " $obj"
}
$vkObjFiles = @()
Get-ChildItem -Path $vkSourceDir -Recurse -Filter "*.cpp" | ForEach-Object {
  $vkObj = Join-Path $outputDir ("vk_" + $_.BaseName + ".obj")
  $vkObjFiles += "`"$vkObj`""
  $linkArgsStr += " `"$vkObj`""
}

# Collect scattered MonixApp method .cpp files — matching exactly what batch compiles
$nativeObjFiles = @()
$nativeCpps = @()
# app\lifecycle (recursive)
Get-ChildItem -Path (Join-Path $root "src\native\app\lifecycle") -Filter "*.cpp" -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
# app\bootstrap (recursive, but skip CliParser.cpp since it's compiled separately)
Get-ChildItem -Path (Join-Path $root "src\native\app\bootstrap") -Filter "*.cpp" -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "CliParser.cpp" } | ForEach-Object { $nativeCpps += $_ }
# platform\win32 (recursive)
Get-ChildItem -Path (Join-Path $root "src\native\platform\win32") -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
# logging (root only)
Get-ChildItem -Path (Join-Path $root "src\native\logging") -Filter "*.cpp" -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
# logging\sound, logging\notify, logging\history, logging\export
Get-ChildItem -Path (Join-Path $root "src\native\logging\sound") -Filter "*.cpp" -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
Get-ChildItem -Path (Join-Path $root "src\native\logging\notify") -Filter "*.cpp" -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
Get-ChildItem -Path (Join-Path $root "src\native\logging\history") -Filter "*.cpp" -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
Get-ChildItem -Path (Join-Path $root "src\native\logging\export") -Filter "*.cpp" -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
# telemetry\runtime\TelemetryThread.cpp only
$nativeCpps += Get-Item (Join-Path $root "src\native\telemetry\runtime\TelemetryThread.cpp") -ErrorAction SilentlyContinue
# ui (root only - render panels compiled explicitly below)
Get-ChildItem -Path (Join-Path $root "src\native\ui") -Filter "*.cpp" -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "ShaderBrowserPanel.cpp" } | ForEach-Object { $nativeCpps += $_ }
# settings (SettingHelpers etc.)
Get-ChildItem -Path (Join-Path $root "src\native\settings") -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "SettingDefs.cpp" -and $_.Name -ne "SettingSerializer.cpp" -and $_.Name -ne "SettingsRegistry.cpp" } | ForEach-Object { $nativeCpps += $_ }
# scram\ScramEngine.cpp + scram\rules
$nativeCpps += Get-Item (Join-Path $root "src\native\scram\ScramEngine.cpp") -ErrorAction SilentlyContinue
Get-ChildItem -Path (Join-Path $root "src\native\scram\rules") -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue | ForEach-Object { $nativeCpps += $_ }
# updater\AutoUpdater.cpp
$nativeCpps += Get-Item (Join-Path $root "src\native\updater\AutoUpdater.cpp") -ErrorAction SilentlyContinue
foreach ($f in $nativeCpps) {
  $nativeObj = Join-Path $outputDir ("native_" + $f.BaseName + ".obj")
  $nativeObjFiles += "`"$nativeObj`""
  $linkArgsStr += " `"$nativeObj`""
}
# Also add explicitly compiled files
$extraObjFiles = @("native_AppWindowProc","native_GlslpPresetParser","native_NetworkCollectors","native_SchedulerCollector","native_OsKernelCollector","native_ReliabilityCollector","native_SecurityCollector","native_PowerCollector","native_ThermalCollector","native_HardwareBoardCollector","native_FilesystemCollector","native_RegistryCollector","native_AudioCollector","native_GpuDisplayCollector","native_ReferenceSeeder","native_SnapshotPoller","native_SnapshotConsumer","native_CrashHandling","native_TelemetryThreadRoot","native_EntityTracker","native_SnapshotChangeDetector")
foreach ($name in $extraObjFiles) {
  $linkArgsStr += " `"$outputDir\$name.obj`""
}
# Add all ui/render .obj files (compiled by for /r in bat)
Get-ChildItem -Path (Join-Path $root "src\native\ui\render") -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
  $linkArgsStr += " `"$outputDir\native_$($_.BaseName).obj`""
}

$zigInc = Join-Path $root "tools\zig-dist\zig-x86_64-windows-0.16.0\lib\libc\include\any-windows-any"

# Build collector subdirectory /I flags for recursive compilation
$collectorSubInc = ""
$collectorSubDirs = Get-ChildItem -Path (Join-Path $projectRoot "src\core\collectors") -Directory -ErrorAction SilentlyContinue
foreach ($sd in $collectorSubDirs) {
  $collectorSubInc += " /I`"$($sd.FullName)`""
}

$batContent = @"
@echo off
call "$vcvarsall" x64
if errorlevel 1 goto :error
set INCLUDE=%INCLUDE%;$zigInc
echo Compiling hardware.c with cl.exe /MT /Zm200...
cl.exe /MT /O1 /Zm200 /utf-8 /I"$root\sensors" /c "$hwCSource" /Fo:"$hwCObjOutput"
if errorlevel 1 goto :error
echo Compiling cpu.asm with ml64.exe...
ml64.exe /c /Fo "$cpuAsmObjOutput" "$cpuAsmSource"
if errorlevel 1 goto :error
echo Compiling main.cpp with cl.exe /MT /std:c++20 /Od /Zm800 /bigobj...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /w /I"$root\login" /I"$root\sensors" /c "$source" /Fo:"$objOutput"
if errorlevel 1 goto :error
echo Compiling SettingsRegistry.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /w /I"$root\src\native" /c "$settingsRegistrySource" /Fo:"$settingsRegistryObjOutput"
if errorlevel 1 goto :error
echo Compiling MonixApp.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$monixAppSource" /Fo:"$monixAppObjOutput"
if errorlevel 1 goto :error
echo Compiling CliParser.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /I"$root\src\native" /c "$cliParserSource" /Fo:"$cliParserObjOutput"
if errorlevel 1 goto :error
echo Compiling pipeline test runners...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /I"$root\src\native" /c "$pipelineTestsSource" /Fo:"$pipelineTestsObjOutput"
if errorlevel 1 goto :error
echo Compiling runtime test runner...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /I"$root\src\native" /I"$root\src\native\renderer_vk" /c "$runtimeTestsSource" /Fo:"$runtimeTestsObjOutput"
if errorlevel 1 goto :error
echo Compiling Vulkan module sources...
for /r "$vkSourceDir" %%f in (*.cpp) do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /EHsc /utf-8 /I"$root\src\native" /I"$root\src\native\renderer_vk" /c "%%f" /Fo:"$outputDir\vk_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling CrashHandler.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /I"$root\src\native" /c "$crashHandlerSource" /Fo:"$crashHandlerObjOutput"
if errorlevel 1 goto :error
echo Compiling SettingDefs.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /w /I"$root\src\native" /c "$settingDefsSource" /Fo:"$settingDefsObjOutput"
if errorlevel 1 goto :error
echo Compiling SettingSerializer.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /w /I"$root\src\native" /c "$settingSerializerSource" /Fo:"$settingSerializerObjOutput"
if errorlevel 1 goto :error
echo Compiling TextUtils.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /DWIN32_LEAN_AND_MEAN /w /I"$root\src\native" /c "$textUtilsSource" /Fo:"$textUtilsObjOutput"
if errorlevel 1 goto :error
echo Compiling Collectors.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /DWIN32_LEAN_AND_MEAN /w /I"$root\src\native" /c "$collectorsSource" /Fo:"$collectorsObjOutput"
if errorlevel 1 goto :error
echo Compiling KernelDiagnostics.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /DWIN32_LEAN_AND_MEAN /w /I"$projectRoot\src\native" /c "$kernelDiagSource" /Fo:"$kernelDiagObjOutput"
if errorlevel 1 goto :error
echo Compiling vulkan_renderer.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /DUNICODE /D_UNICODE /w /I"$root\src\native" /c "$vulkanRendererSource" /Fo:"$vulkanRendererObjOutput"
if errorlevel 1 goto :error
echo Compiling renderer_vk runtime...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /DUNICODE /D_UNICODE /w /I"$root\src\native" /c "$rendererVkSource" /Fo:"$rendererVkObjOutput"
if errorlevel 1 goto :error
echo Compiling LoginOverlay.cpp...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /DUNICODE /D_UNICODE /D MONIX_ARCH_X64 /w /I"$root\login" /I"$root\sensors" /c "$loginSource" /Fo:"$loginObjOutput"
if errorlevel 1 goto :error
echo Compiling MonixKernel.cpp...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /DUNICODE /D_UNICODE /D MONIX_ARCH_X64 /w /I"$root\login" /I"$root\sensors" /I"$root\src\native" /c "$monixKernelSource" /Fo:"$monixKernelObjOutput"
if errorlevel 1 goto :error
echo Compiling KernelDisplay.cpp...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /DUNICODE /D_UNICODE /w /I"$root\login" /I"$root\src\native" /c "$kernelDisplaySource" /Fo:"$kernelDisplayObjOutput"
if errorlevel 1 goto :error
echo Compiling BootUp.cpp...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /w /I"$root\login" /c "$bootUpSource" /Fo:"$bootUpObjOutput"
if errorlevel 1 goto :error
echo Compiling BootUpDisplay.cpp...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /DUNICODE /D_UNICODE /w /I"$root\login" /I"$root\src\native" /c "$bootUpDisplaySource" /Fo:"$bootUpDisplayObjOutput"
if errorlevel 1 goto :error
echo Compiling GdiPlusLoader.cpp...
cl.exe /MT /std:c++20 /O2 /Zm200 /EHsc /utf-8 /w /I"$root\login" /c "$gdiPlusLoaderSource" /Fo:"$gdiPlusLoaderObjOutput"
if errorlevel 1 goto :error
echo Compiling GlBackend.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /w /I"$root\src\native" /c "$glBackendSource" /Fo:"$glBackendObjOutput"
if errorlevel 1 goto :error
echo Compiling AppWindowProc.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\AppWindowProc.cpp" /Fo:"$outputDir\native_AppWindowProc.obj"
if errorlevel 1 goto :error
echo Compiling GlslpPresetParser.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\shader\preset_parser\GlslpPresetParser.cpp" /Fo:"$outputDir\native_GlslpPresetParser.obj"
if errorlevel 1 goto :error
echo Compiling NetworkCollectors.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\network\NetworkCollectors.cpp" /Fo:"$outputDir\native_NetworkCollectors.obj"
if errorlevel 1 goto :error
echo Compiling SchedulerCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\sys\SchedulerCollector.cpp" /Fo:"$outputDir\native_SchedulerCollector.obj"
if errorlevel 1 goto :error
echo Compiling OsKernelCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\sys\OsKernelCollector.cpp" /Fo:"$outputDir\native_OsKernelCollector.obj"
if errorlevel 1 goto :error
echo Compiling ReliabilityCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\sys\ReliabilityCollector.cpp" /Fo:"$outputDir\native_ReliabilityCollector.obj"
if errorlevel 1 goto :error
echo Compiling SecurityCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\security\SecurityCollector.cpp" /Fo:"$outputDir\native_SecurityCollector.obj"
if errorlevel 1 goto :error
echo Compiling PowerCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\power\PowerCollector.cpp" /Fo:"$outputDir\native_PowerCollector.obj"
if errorlevel 1 goto :error
echo Compiling ThermalCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\thermal\ThermalCollector.cpp" /Fo:"$outputDir\native_ThermalCollector.obj"
if errorlevel 1 goto :error
echo Compiling HardwareBoardCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\hw\HardwareBoardCollector.cpp" /Fo:"$outputDir\native_HardwareBoardCollector.obj"
if errorlevel 1 goto :error
echo Compiling FilesystemCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\fs\FilesystemCollector.cpp" /Fo:"$outputDir\native_FilesystemCollector.obj"
if errorlevel 1 goto :error
echo Compiling RegistryCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\fs\RegistryCollector.cpp" /Fo:"$outputDir\native_RegistryCollector.obj"
if errorlevel 1 goto :error
echo Compiling AudioCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\audio\AudioCollector.cpp" /Fo:"$outputDir\native_AudioCollector.obj"
if errorlevel 1 goto :error
echo Compiling GpuDisplayCollector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\collectors\gpu\GpuDisplayCollector.cpp" /Fo:"$outputDir\native_GpuDisplayCollector.obj"
if errorlevel 1 goto :error
echo Compiling ReferenceSeeder.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\snapshot\ReferenceSeeder.cpp" /Fo:"$outputDir\native_ReferenceSeeder.obj"
if errorlevel 1 goto :error
echo Compiling Snapshot poller...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\snapshot\SnapshotPoller.cpp" /Fo:"$outputDir\native_SnapshotPoller.obj"
if errorlevel 1 goto :error
echo Compiling SnapshotConsumer.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\snapshot\SnapshotConsumer.cpp" /Fo:"$outputDir\native_SnapshotConsumer.obj"
if errorlevel 1 goto :error
echo Compiling telemetry/state/EntityTracker.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\state\EntityTracker.cpp" /Fo:"$outputDir\native_EntityTracker.obj"
if errorlevel 1 goto :error
echo Compiling telemetry/state/SnapshotChangeDetector.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\state\SnapshotChangeDetector.cpp" /Fo:"$outputDir\native_SnapshotChangeDetector.obj"
if errorlevel 1 goto :error
echo Compiling CrashHandling.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /utf-8 /I"$root\src\native" /c "$root\src\native\crash\CrashHandling.cpp" /Fo:"$outputDir\native_CrashHandling.obj"
if errorlevel 1 goto :error
echo Compiling Ultra Logger core sources...
set UL_SRC=$projectRoot\src\core
set UL_TEST=$projectRoot\tests\core
set UL_OBJ=$outputDir
set UL_INC=/I"%UL_SRC%\events" /I"%UL_SRC%\eventbus" /I"%UL_SRC%\validation" /I"%UL_SRC%\collectors" /I"%UL_SRC%\security" /I"%UL_SRC%\integration" /I"%UL_SRC%\platform" /I"%UL_SRC%\platform\windows" /I"%UL_SRC%" /I"$projectRoot\src\native\kernel" $collectorSubInc
for /r "%UL_SRC%" %%f in (*.cpp) do (
  cl.exe /MT /std:c++20 /O2 /EHsc /w /DWIN32_LEAN_AND_MEAN /DMONIX_KERNEL_BUILD %UL_INC% /c "%%f" /Fo:"%UL_OBJ%\%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling Ultra Logger test files...
for /r "%UL_TEST%" %%f in (*Tests.cpp) do (
  cl.exe /MT /std:c++20 /O2 /EHsc /w /DWIN32_LEAN_AND_MEAN /DMONIX_KERNEL_BUILD %UL_INC% /c "%%f" /Fo:"%UL_OBJ%\%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling scattered MonixApp sources...
for /r "$root\src\native\app\lifecycle" %%f in (*.cpp) do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
for /r "$root\src\native\app\bootstrap" %%f in (*.cpp) do (
  if not "%%~nxf"=="CliParser.cpp" (
    cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
    if errorlevel 1 goto :error
  )
)
for /r "$root\src\native\platform\win32" %%f in (*.cpp) do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling logging sources (excluding broken telemetry refs)...
for %%f in ("$root\src\native\logging\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
for %%f in ("$root\src\native\logging\sound\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
for %%f in ("$root\src\native\logging\notify\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
for %%f in ("$root\src\native\logging\history\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
for %%f in ("$root\src\native\logging\export\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling TelemetryThread root (StaticWndProc)...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\TelemetryThread.cpp" /Fo:"$outputDir\native_TelemetryThreadRoot.obj"
if errorlevel 1 goto :error
echo Compiling ui render files (all)...
for /r "$root\src\native\ui\render" %%f in (*.cpp) do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling settings sources (excluding SettingDefs/SettingSerializer already compiled)...
for /r "$root\src\native\settings" %%f in (*.cpp) do (
  if not "%%~nxf"=="SettingDefs.cpp" if not "%%~nxf"=="SettingSerializer.cpp" if not "%%~nxf"=="SettingsRegistry.cpp" if not "%%~nxf"=="SettingDefs2.cpp" (
    cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
    if errorlevel 1 goto :error
  )
)
echo Compiling telemetry/runtime/TelemetryThread.cpp...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\telemetry\runtime\TelemetryThread.cpp" /Fo:"$outputDir\native_TelemetryThread.obj"
if errorlevel 1 goto :error
echo Compiling ui sources...
for %%f in ("$root\src\native\ui\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling scram sources...
cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "$root\src\native\scram\ScramEngine.cpp" /Fo:"$outputDir\native_ScramEngine.obj"
if errorlevel 1 goto :error
for %%f in ("$root\src\native\scram\rules\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling updater sources...
for %%f in ("$root\src\native\updater\*.cpp") do (
  cl.exe /MT /std:c++20 /O2 /Zm800 /bigobj /EHsc /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I"$root\src\native" /I"$root\login" /I"$root\sensors" /c "%%f" /Fo:"$outputDir\native_%%~nf.obj"
  if errorlevel 1 goto :error
)
echo Compiling KernelSelfTest.cpp...
cl.exe /MT /std:c++20 /O2 /EHsc /w /DWIN32_LEAN_AND_MEAN /DMONIX_KERNEL_BUILD %UL_INC% /c "$projectRoot\src\native\kernel\KernelSelfTest.cpp" /Fo:"%UL_OBJ%\KernelSelfTest.obj"
if errorlevel 1 goto :error
echo Linking with static CRT...
link.exe $linkArgsStr
if errorlevel 1 goto :error
echo Build succeeded.
goto :end
:error
echo Build failed.
exit /b 1
:end
"@

Set-Content -Path $buildBat -Value $batContent -Encoding ASCII
Write-Host "Running build via vcvarsall..."
& cmd /c $buildBat
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

# --- Apply icon ---
$iconStamped = $false
if (Test-Path $rcedit) {
  Write-Host "Applying embedded icon with rcedit..."
  & $rcedit $output --set-icon $icoFile
  if ($LASTEXITCODE -ne 0) {
    Write-Host "Warning: rcedit failed to stamp ico.ico into the executable"
  } else {
    $iconStamped = $true
  }
} else {
  Write-Host "Warning: rcedit not found, skipping post-build icon stamping"
}

# --- Copy fonts to build directory ---
$fontsSrcDir = Join-Path $root "fonts"
$fontsDstDir = Join-Path $outputDir "fonts"
if (Test-Path $fontsSrcDir) {
  New-Item -ItemType Directory -Force $fontsDstDir | Out-Null
  Copy-Item "$fontsSrcDir\*.ttf" $fontsDstDir -Force -ErrorAction SilentlyContinue
  Write-Host "Copied fonts to $fontsDstDir"
}


Write-Host "Built $output ($([math]::Round((Get-Item $output).Length / 1024))KB)"
if ($iconStamped) {
  Write-Host "Icon embedded successfully"
}

# Cleanup temp build script
Remove-Item $buildBat -Force -ErrorAction SilentlyContinue
