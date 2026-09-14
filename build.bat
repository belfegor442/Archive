@echo off
setlocal

set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

echo === Archive Build Script (Release) ===
echo.

echo [1/3] Configuring CMake (Release)...
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++.exe -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo [2/3] Building...
cmake --build build
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo [3/3] Running tests...
build\bin\archive_tests.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Tests failed
    exit /b 1
)

echo.
echo === Build successful! ===
echo Binary: build\bin\archive.exe
