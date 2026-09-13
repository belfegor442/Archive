@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
cd /d D:\Monix-2ago-unestable\Monix\Monix

echo Building fase18_workspace_tests...
cl.exe /MT /std:c++20 /Od /EHsc /bigobj /F33554432 ^
  /I. ^
  /Isrc\native ^
  /Isrc\native\renderer_vk ^
  /Isrc\native\renderer_vk\shader_runtime ^
  /Isrc\native\renderer_vk\shader_runtime\core ^
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
  src\native\fase18_test_unity.cpp ^
  src\native\renderer_vk\library\tests\fase18_workspace_tests_main.cpp ^
  /Febuild\fase18_workspace_tests.exe

echo ERRORLEVEL=%ERRORLEVEL%
