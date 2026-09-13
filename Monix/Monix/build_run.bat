@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d D:\Monix-10sept-stable\Monix-31ago-stable\Monix-2ago-unestable\Monix\Monix
powershell -ExecutionPolicy Bypass -File build.ps1
echo BUILD_EXIT=%ERRORLEVEL%
