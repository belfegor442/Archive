@echo off
set PASS=0
set FAIL=0
for %%f in (events_tests.exe shader_runtime_tests.exe production_hardening_tests.exe integration_tests.exe shader_library_tests.exe shader_library_compiler_tests.exe gpu_validation_tests.exe shader_browser_panel_tests.exe fase13_validation_tests.exe fase16_workspace_tests.exe external_compat_tests.exe) do (
  echo === %%f ===
  if not exist build\%%f (
    echo FAIL: missing executable
    set /a FAIL+=1
  ) else (
    build\%%f 2>&1
    if errorlevel 1 (
      set /a FAIL+=1
    ) else (
      set /a PASS+=1
    )
  )
  echo.
)
echo Executables passed: %PASS%
echo Executables failed: %FAIL%
if %FAIL% NEQ 0 exit /b 1
