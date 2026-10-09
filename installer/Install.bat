@echo off
REM ============================================================
REM  XFBIN Import for 3ds Max - install from the ZIP package
REM
REM  Copies the XfbinImport folder next to this file into
REM    %ProgramData%\Autodesk\ApplicationPlugins\XfbinImport
REM  (for all users; users may create folders there, so no
REM  administrator rights are needed). An old copy in the user
REM  folder (%APPDATA%, up to 2.x) is removed.
REM
REM  The Setup .exe from the release page does the same.
REM ============================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "PKG=%~dp0XfbinImport"
set "DEST=%ProgramData%\Autodesk\ApplicationPlugins\XfbinImport"
set "ALT=%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport"

echo.
echo ========================================
echo  XFBIN Import for 3ds Max - Install
echo ========================================
echo.

if not exist "%PKG%\PackageContents.xml" (
    echo ERROR: The XfbinImport folder was not found next to this file.
    echo Extract the whole ZIP first, then run Install.bat from there.
    echo.
    pause
    exit /b 1
)

REM Is an installed plugin locked? Opening the file for append
REM without writing tests exactly that - it fails while a running
REM 3ds Max holds the plugin.
set "LOCKED="
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%DEST%\Contents\%%V\XfbinImport.dlu" (
        (call ) 2>nul 1>>"%DEST%\Contents\%%V\XfbinImport.dlu" || set "LOCKED=1"
    )
)

if defined LOCKED (
    echo ERROR: The installed plugin is in use.
    echo Close every 3ds Max window and run Install.bat again.
    echo.
    pause
    exit /b 1
)

echo Installing to:
echo   %DEST%
echo.

if exist "%DEST%" rmdir /s /q "%DEST%" >nul 2>&1
mkdir "%DEST%" >nul 2>&1

xcopy /E /I /Y /Q "%PKG%\*" "%DEST%\" >nul
if errorlevel 1 goto :copyfail

if not exist "%DEST%\PackageContents.xml"                   goto :copyfail
if not exist "%DEST%\Contents\MacroScripts\XfbinImport.mcr" goto :copyfail

if exist "%ALT%" rmdir /s /q "%ALT%" >nul 2>&1

echo ========================================
echo  Installed.
echo ========================================
echo.
echo Start 3ds Max and open:
echo.
echo   DH Tools  -^>  XFBIN Import
echo.
echo On 3ds Max 2025 and newer the menu may only appear after a
echo second restart. It is always available under
echo   Customize -^> Customize User Interface -^> Category "DH Tools"
echo.
pause
exit /b 0

:copyfail
echo ERROR: Copying to %DEST% failed.
echo.
pause
exit /b 1
