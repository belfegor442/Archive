@echo off
setlocal EnableDelayedExpansion

echo ============================================
echo  MONIX v3.1 - Installer
echo ============================================
echo.

set "INSTALL_DIR=%LOCALAPPDATA%\Monix"
set "BIN_DIR=%INSTALL_DIR%\bin"
set "LOG_DIR=%INSTALL_DIR%\logs"
set "CFG_DIR=%INSTALL_DIR%\config"
set "SOUND_DIR=%INSTALL_DIR%\sounds"

echo [1/5] Checking for running Monix...
taskkill /f /im Monix.exe >nul 2>&1
if %errorlevel%==0 (
    echo     Stopped running Monix instance.
)

echo [2/5] Creating directories...
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
if not exist "%CFG_DIR%" mkdir "%CFG_DIR%"
if not exist "%SOUND_DIR%" mkdir "%SOUND_DIR%"
echo     Created: %INSTALL_DIR%

echo [3/5] Copying files...
copy /y "%~dp0..\build\Monix.exe" "%BIN_DIR%\Monix.exe" >nul
if errorlevel 1 (
    echo     ERROR: Could not copy Monix.exe
    pause
    exit /b 1
)
xcopy /y /e /q "%~dp0..\build\fonts" "%BIN_DIR%\fonts\" >nul 2>&1
xcopy /y /e /q "%~dp0..\src\icon" "%BIN_DIR%\icon\" >nul 2>&1
echo     Copied Monix.exe + fonts + icons

echo [4/5] Creating desktop shortcut...
set "SHORTCUT_PATH=%USERPROFILE%\Desktop\Monix.lnk"
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%SHORTCUT_PATH%'); $s.TargetPath = '%BIN_DIR%\Monix.exe'; $s.WorkingDirectory = '%BIN_DIR%'; $s.Description = 'Monix System Monitor'; $s.Save()" >nul 2>&1
if errorlevel 1 (
    echo     WARNING: Could not create desktop shortcut
) else (
    echo     Desktop shortcut created
)

echo [5/5] Writing version info...
echo {"version":"3.1.0","installed":"%date% %time%","path":"%BIN_DIR%"} > "%INSTALL_DIR%\version.json"

echo.
echo ============================================
echo  Installation complete!
echo  Location: %INSTALL_DIR%
echo ============================================
echo.
echo Launch Monix? [Y/N]
set /p choice=
if /i "%choice%"=="Y" (
    start "" "%BIN_DIR%\Monix.exe"
)
