@echo off
setlocal

set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

echo Cleaning build directory...
if exist build rmdir /s /q build

echo Clean complete.
