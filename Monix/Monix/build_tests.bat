@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

cd /d D:\Monix-2ago-unestable\Monix\Monix

cl.exe /MT /std:c++20 /Od /EHsc /W4 /I. ^
  src\native\events\EventId.cpp ^
  src\native\events\EventBuilder.cpp ^
  src\native\events\EventFactory.cpp ^
  src\native\events\EventSerializer.cpp ^
  src\native\events\EventDeserializer.cpp ^
  src\native\events\tests\test_main.cpp ^
  /Febuild\events_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F16777216 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\tests ^
  src\native\shader_runtime_test_unity.cpp ^
  src\native\renderer_vk\shader_runtime\tests\shader_runtime_tests_main.cpp ^
  /Febuild\shader_runtime_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F16777216 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\tests ^
  src\native\production_hardening_test_unity.cpp ^
  src\native\renderer_vk\shader_runtime\tests\production_hardening_tests.cpp ^
  src\native\renderer_vk\shader_runtime\tests\production_hardening_tests_main.cpp ^
  /Febuild\production_hardening_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F16777216 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\tests ^
  src\native\integration_test_unity.cpp ^
  src\native\renderer_vk\shader_runtime\tests\real_shader_integration_tests.cpp ^
  src\native\renderer_vk\shader_runtime\tests\integration_tests_main.cpp ^
  /Febuild\integration_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F16777216 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\library ^
  /Isrc\native\renderer_vk\library\tests ^
  src\native\shader_library_test_unity.cpp ^
  src\native\renderer_vk\library\tests\shader_library_tests_main.cpp ^
  /Febuild\shader_library_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F33554432 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\dependencies ^
  /Isrc\native\renderer_vk\library ^
  /Isrc\native\renderer_vk\library\tests ^
  /Isrc\native\renderer_vk\validation ^
  src\native\shader_library_compiler_test_unity.cpp ^
  src\native\renderer_vk\library\tests\shader_library_compiler_tests_main.cpp ^
  /Febuild\shader_library_compiler_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F33554432 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\dependencies ^
  /Isrc\native\renderer_vk\library ^
  /Isrc\native\renderer_vk\library\tests ^
  /Isrc\native\renderer_vk\validation ^
  /Isrc\native\renderer_vk\ui ^
  /Isrc\native\renderer_vk\ui\tests ^
  src\native\shader_browser_panel_test_unity.cpp ^
  src\native\renderer_vk\ui\tests\shader_browser_panel_tests_main.cpp ^
  /Febuild\shader_browser_panel_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F33554432 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\dependencies ^
  /Isrc\native\renderer_vk\shader_runtime\tests ^
  /Isrc\native\renderer_vk\library ^
  /Isrc\native\renderer_vk\library\tests ^
  /Isrc\native\renderer_vk\validation ^
  /Isrc\native\renderer_vk\debug ^
  /Isrc\native\renderer_vk\public ^
   src\native\gpu_validation_test_unity.cpp ^
   src\native\renderer_vk\tests\gpu_validation_tests_main.cpp ^
   opengl32.lib user32.lib gdi32.lib kernel32.lib ^
   /Febuild\gpu_validation_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F33554432 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\dependencies ^
  /Isrc\native\renderer_vk\shader_runtime\tests ^
  /Isrc\native\renderer_vk\library ^
  /Isrc\native\renderer_vk\library\tests ^
  /Isrc\native\renderer_vk\validation ^
  /Isrc\native\renderer_vk\debug ^
  /Isrc\native\renderer_vk\public ^
  src\native\fase13_validation_test_unity.cpp ^
  src\native\renderer_vk\tests\fase13_validation_tests_main.cpp ^
   opengl32.lib user32.lib gdi32.lib kernel32.lib ^
   /Febuild\fase13_validation_tests.exe

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F33554432 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\core ^
  /Isrc\native\renderer_vk\compiler ^
  /Isrc\native\renderer_vk\graph ^
  /Isrc\native\renderer_vk\preset ^
  /Isrc\native\renderer_vk\runtime ^
  /Isrc\native\renderer_vk\vulkan ^
  /Isrc\native\renderer_vk\shader_runtime\adapters ^
  /Isrc\native\renderer_vk\shader_runtime\dependencies ^
  /Isrc\native\renderer_vk\shader_runtime\tests ^
  /Isrc\native\renderer_vk\library ^
  /Isrc\native\renderer_vk\library\tests ^
  /Isrc\native\renderer_vk\validation ^
  /Isrc\native\renderer_vk\debug ^
  /Isrc\native\renderer_vk\public ^
  /Isrc\native\renderer_vk\ui ^
  /Isrc\native\renderer_vk\ui\tests ^
  /Isrc\native\renderer_vk\shader_runtime\core ^
  src\native\external_compat_test_unity.cpp ^
  src\native\renderer_vk\tests\external_compat_tests_main.cpp ^
  /Febuild\external_compat_tests.exe
