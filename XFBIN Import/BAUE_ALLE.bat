@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo.
echo ========================================
echo  XFBIN Import 1.5.2 - Max 2016 bis 2027
echo ========================================
echo.

REM ============================================================
REM  Visual Studio finden
REM
REM  Uebernommen aus dem Buildscript von WhiteoutDex, das genau
REM  diesen Versionsbereich schon baut. vswhere liegt bei jeder
REM  VS-Installation an derselben Stelle und sagt, was da ist -
REM  besser als einen Generator fest einzutragen und beim
REM  naechsten VS-Sprung wieder anzufassen.
REM
REM  Ueberschreibbar:  set XFBIN_GENERATOR=Visual Studio 17 2022
REM ============================================================
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not defined XFBIN_GENERATOR (
    set "XFBIN_GENERATOR=Visual Studio 17 2022"

    if exist "%VSWHERE%" (
        for /f "delims=" %%R in ('"%VSWHERE%" -version [18.0^,19.0^) -property installationPath 2^>nul') do (
            if exist "%%R" set "XFBIN_GENERATOR=Visual Studio 18 2026"
        )
    )
)

REM ---- Strenge Compiler-Warnungen? ----
REM ON  = /W4 (plus /permissive- ab Max 2019)
REM OFF = /W3
if not defined XFBIN_STRICT set XFBIN_STRICT=ON

REM ---- Ausgabe-Ordner ----
set OUTPUT=%~dp0output

echo Generator:  %XFBIN_GENERATOR%
echo Warnungen:  %XFBIN_STRICT%
echo Ausgabe:    %OUTPUT%
echo.

REM STOP_ON_ERROR=1 bricht nach dem ersten Fehlschlag ab. Bei einem
REM Quellcode-Fehler scheitern ohnehin alle Versionen mit derselben
REM Meldung - dann muss man nicht zwoelfmal warten.
if not defined STOP_ON_ERROR set STOP_ON_ERROR=1
REM Auf 0 setzen, um trotz Fehler alle Versionen durchzubauen:
REM   set STOP_ON_ERROR=0
set BUILD_FAILED=0

REM Eine einzelne Version bauen:  BAUE_ALLE.bat 2019
set "SINGLE=%~1"

REM ============================================================
REM  Zuerst das eigenstaendige Werkzeug. Es braucht kein Max SDK
REM  und faellt damit als erstes durch, wenn im Parser ein Fehler
REM  steckt - dann muss man nicht zwoelf SDK-Builds abwarten.
REM ============================================================
if not defined SINGLE (
    call :build_tool
    if "%STOP_ON_ERROR%"=="1" if "!BUILD_FAILED!"=="1" goto :summary
)

for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    set "DO=1"
    if defined SINGLE if not "%%V"=="!SINGLE!" set "DO=0"

    if "!DO!"=="1" (
        call :build_version %%V
        if "%STOP_ON_ERROR%"=="1" if "!BUILD_FAILED!"=="1" goto :summary
    )
)

:summary

echo.
echo ========================================
echo  Zusammenfassung
echo ========================================
echo.
if exist "%OUTPUT%\tools\xfbindump.exe" (echo  Werkzeug: OK) else (echo  Werkzeug: FEHLT)
set FOUND=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    call :report %%V
)
echo.
echo  !FOUND! Version(en) gebaut. Dateien liegen in: %OUTPUT%
echo.
pause
exit /b 0

REM ============================================================
:build_tool

echo ----------------------------------------
echo  Werkzeug xfbindump.exe (ohne Max SDK)
echo ----------------------------------------
echo  Konfiguriere...

rmdir /s /q "build_tool" >nul 2>&1

cmake -B "build_tool" -G "%XFBIN_GENERATOR%" -A x64 ^
    -DXFBIN_BUILD_PLUGIN=OFF ^
    -DXFBIN_BUILD_TOOL=ON ^
    -DXFBIN_STRICT=%XFBIN_STRICT% > "build_tool_configure.log" 2>&1
if errorlevel 1 (
    echo.
    echo  FEHLER beim Konfigurieren. Die letzten Zeilen:
    echo  ------------------------------------------------------------
    powershell -NoProfile -Command "Get-Content 'build_tool_configure.log' -Tail 15" 2>nul
    echo  ------------------------------------------------------------
    echo  Vollstaendiges Log: build_tool_configure.log
    echo.
    set BUILD_FAILED=1
    goto :eof
)

echo  Kompiliere...
cmake --build "build_tool" --config Release > "build_tool_build.log" 2>&1
if errorlevel 1 (
    call :show_errors "build_tool_build.log"
    set BUILD_FAILED=1
    goto :eof
)

mkdir "%OUTPUT%\tools" >nul 2>&1
for /R "build_tool" %%f in (xfbindump.exe) do (
    copy /Y "%%f" "%OUTPUT%\tools\xfbindump.exe" >nul 2>&1
)

if exist "%OUTPUT%\tools\xfbindump.exe" (
    echo  ERFOLGREICH! -^> output\tools\xfbindump.exe
    del /Q "build_tool_configure.log" >nul 2>&1
    del /Q "build_tool_build.log"     >nul 2>&1
) else (
    echo  FEHLER: xfbindump.exe nicht gefunden
    set BUILD_FAILED=1
)
echo.
goto :eof

REM ============================================================
:build_version
set VER=%~1
set "SDK=C:\Program Files\Autodesk\3ds Max %VER% SDK\maxsdk"

if not exist "%SDK%\include\max.h" (
    echo  %VER%: kein SDK installiert - uebersprungen
    goto :eof
)

echo ----------------------------------------
echo  Max %VER%
echo ----------------------------------------
echo  SDK: %SDK%
echo  Konfiguriere...

rmdir /s /q "build_%VER%" >nul 2>&1

REM MAX_VERSION steuert nur Kleinigkeiten in der CMakeLists -
REM /permissive- etwa gibt es fuer die Header von 2016 bis 2018
REM nicht, die stammen aus der Zeit davor.
cmake -B "build_%VER%" -G "%XFBIN_GENERATOR%" -A x64 ^
    -D3DSMAX_SDK_DIR="%SDK%" ^
    -DMAX_VERSION=%VER% ^
    -DXFBIN_BUILD_TOOL=OFF ^
    -DXFBIN_STRICT=%XFBIN_STRICT% > "build_%VER%_configure.log" 2>&1
if errorlevel 1 (
    echo.
    echo  FEHLER beim Konfigurieren. Die letzten Zeilen:
    echo  ------------------------------------------------------------
    powershell -NoProfile -Command "Get-Content 'build_%VER%_configure.log' -Tail 15" 2>nul
    echo  ------------------------------------------------------------
    echo  Vollstaendiges Log: build_%VER%_configure.log
    echo.
    set BUILD_FAILED=1
    goto :eof
)

echo  Kompiliere...
cmake --build "build_%VER%" --config Release > "build_%VER%_build.log" 2>&1
if errorlevel 1 (
    call :show_errors "build_%VER%_build.log"
    set BUILD_FAILED=1
    goto :eof
)

mkdir "%OUTPUT%\%VER%" >nul 2>&1
for /R "build_%VER%" %%f in (XfbinImport.dlu) do (
    copy /Y "%%f" "%OUTPUT%\%VER%\XfbinImport.dlu" >nul 2>&1
)

if exist "%OUTPUT%\%VER%\XfbinImport.dlu" (
    echo  ERFOLGREICH! -^> output\%VER%\XfbinImport.dlu
    del /Q "build_%VER%_configure.log" >nul 2>&1
    del /Q "build_%VER%_build.log"     >nul 2>&1
) else (
    echo  FEHLER: XfbinImport.dlu nicht gefunden
    set BUILD_FAILED=1
)
echo.
goto :eof

REM ============================================================
REM  Nur die Fehlerzeilen aus einem Build-Log zeigen, nicht alles
REM ============================================================
:show_errors
echo.
echo  FEHLER beim Kompilieren. Die ersten Meldungen:
echo  ------------------------------------------------------------
set FOUND_ERR=0
for /f "tokens=* delims=" %%L in ('findstr /R /C:" error " /C:"error C" /C:"error LNK" /C:"error MSB" %1 2^>nul') do (
    if !FOUND_ERR! LSS 12 (
        echo   %%L
        set /a FOUND_ERR+=1
    )
)
if !FOUND_ERR!==0 (
    echo   Keine Zeile mit "error" gefunden - letzte 15 Zeilen des Logs:
    powershell -NoProfile -Command "Get-Content %1 -Tail 15" 2>nul
)
echo  ------------------------------------------------------------
echo  Vollstaendiges Log: %~1
echo.
goto :eof

REM ============================================================
REM  Eine Zeile der Zusammenfassung
REM
REM  Als Unterprogramm und nicht in der Schleife: geschachtelte
REM  if-Bloecke mit Pfaden voller Leerzeichen sind in cmd eine
REM  bekannte Stolperstelle, hier gibt es die Schachtelung nicht.
REM ============================================================
:report
set VER=%~1
if exist "%OUTPUT%\%VER%\XfbinImport.dlu" (
    echo  %VER%: OK
    set /a FOUND+=1
    goto :eof
)
if exist "C:\Program Files\Autodesk\3ds Max %VER% SDK\maxsdk\include\max.h" (
    echo  %VER%: FEHLT
) else (
    echo  %VER%: kein SDK installiert
)
goto :eof
