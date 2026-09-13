@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 goto :error
echo Compiling test_updater...
cl.exe /MT /std:c++20 /Od /EHsc /DNOMINMAX /DUNICODE /D_UNICODE /I src\native test_updater.cpp src\native\updater\AutoUpdater.cpp src\native\core\TextUtils.cpp /link winhttp.lib /OUT:build\test_updater.exe
if errorlevel 1 goto :error
echo Running test_updater...
build\test_updater.exe
goto :end
:error
echo Build failed!
exit /b 1
:end
