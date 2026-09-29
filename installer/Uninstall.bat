@echo off
REM ============================================================
REM  XFBIN Import for 3ds Max - remove a ZIP installation
REM
REM  Removes %APPDATA%\Autodesk\ApplicationPlugins\XfbinImport.
REM  If you used the Setup .exe, uninstall it from
REM  Windows Settings -> Apps instead.
REM ============================================================

setlocal enabledelayedexpansion
set "DEST=%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport"

echo.
echo Removing: %DEST%
echo.

if not exist "%DEST%" (
    echo Nothing to do - XFBIN Import is not installed for this user.
    echo.
    pause
    exit /b 0
)

set "LOCKED="
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%DEST%\Contents\%%V\XfbinImport.dlu" (
        (call ) 2>nul 1>>"%DEST%\Contents\%%V\XfbinImport.dlu" || set "LOCKED=1"
    )
)

if defined LOCKED (
    echo ERROR: The plugin is in use.
    echo Close every 3ds Max window and run Uninstall.bat again.
    echo.
    pause
    exit /b 1
)

rmdir /s /q "%DEST%"
if exist "%DEST%" (
    echo ERROR: The folder could not be removed.
    echo.
    pause
    exit /b 1
)

echo Removed.
echo.
pause
exit /b 0
