@echo off
setlocal EnableDelayedExpansion

echo ============================================
echo  MONIX - Uninstaller
echo ============================================
echo.

set "INSTALL_DIR=%LOCALAPPDATA%\Monix"

echo [1/3] Stopping Monix...
taskkill /f /im Monix.exe >nul 2>&1
if %errorlevel%==0 (
    echo     Stopped running Monix instance.
) else (
    echo     Monix not running.
)

echo [2/3] Removing files...
if exist "%INSTALL_DIR%" (
    rmdir /s /q "%INSTALL_DIR%"
    echo     Removed: %INSTALL_DIR%
) else (
    echo     Not found: %INSTALL_DIR%
)

echo [3/3] Removing desktop shortcut...
if exist "%USERPROFILE%\Desktop\Monix.lnk" (
    del /f "%USERPROFILE%\Desktop\Monix.lnk"
    echo     Removed desktop shortcut
)

echo.
echo ============================================
echo  Uninstall complete!
echo ============================================
echo.
pause
