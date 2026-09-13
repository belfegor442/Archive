@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /std:c++20 /MT /bigobj /EHsc /O2 /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /I src\native /I tools\zig-dist\zig-x86_64-windows-0.16.0\lib\libc\include\any-windows-any /I src\external /I src\external\imgui /c /Fobuild\buildlog\ src\native\telemetry\snapshot\SnapshotConsumer.cpp
if %ERRORLEVEL% neq 0 exit /b 1
cl /std:c++20 /MT /bigobj /EHsc /O2 /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /I src\native /I tools\zig-dist\zig-x86_64-windows-0.16.0\lib\libc\include\any-windows-any /I src\external /I src\external\imgui /c /Fobuild\buildlog\ src\native\MonixApp.cpp
if %ERRORLEVEL% neq 0 exit /b 1
echo SINGLE_FILE_COMPILE_OK
