@echo off
REM ============================================================
REM  XFBIN Import - Release-Dateien erstellen
REM
REM  Voraussetzung: BAUE_ALLE.bat ist gelaufen, output\ enthaelt
REM  die DLUs fuer alle zwoelf Max-Versionen.
REM
REM  Ergebnis in dist\:
REM    XfbinImport-<ver>-Setup.exe   Installer (Inno Setup)
REM    XfbinImport-<ver>.zip         Paket + Install.bat zum Entpacken
REM    SHA256SUMS.txt                Pruefsummen fuer die Release-Seite
REM
REM  Danach hochladen - den genauen Befehl gibt das Skript am
REM  Ende aus (gh release create ...).
REM
REM  Optional:
REM    set XFBIN_ALLOW_PARTIAL=1   fehlende Max-Versionen erlauben
REM    set XFBIN_ISCC=<pfad>       ISCC.exe, falls nicht gefunden
REM    set XFBIN_SIGNTOOL=<name>   Installer signieren; <name> ist ein
REM                                in Inno Setup eingerichtetes Sign
REM                                Tool (Tools -^> Configure Sign Tools)
REM ============================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "SRC=%~dp0output"
set "SCRIPTS=%~dp0scripts"
set "DIST=%~dp0dist"
set "STAGE=%DIST%\stage\XfbinImport"
set "ZIPDIR=%DIST%\zip"

REM ---- Version aus dem Plugin-Header - die eine Quelle ----
set "VER="
REM Zeile:  #define XFBINIMPORT_VERSION_STR  _T("2.1.6")
REM Trennzeichen ist das Anfuehrungszeichen; anders als ueber
REM diese Escape-Form laesst es sich for /f nicht uebergeben.
for /f tokens^=2^ delims^=^" %%A in ('findstr /C:"XFBINIMPORT_VERSION_STR" src\xfbinimport.h') do set "VER=%%A"
if not defined VER (
    echo FEHLER: Version in src\xfbinimport.h nicht gefunden.
    goto :fail
)

echo.
echo ========================================
echo  XFBIN Import %VER% - Release erstellen
echo ========================================
echo.

findstr /C:"AppVersion=\"%VER%\"" package\XfbinImport\PackageContents.xml >nul
if errorlevel 1 (
    echo FEHLER: PackageContents.xml steht nicht auf %VER%.
    echo Mit  python tools\VERSION_SETZEN.py %VER%  angleichen.
    goto :fail
)

REM ---- Inno Setup finden ----
if not defined XFBIN_ISCC (
    if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "XFBIN_ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
)
if not defined XFBIN_ISCC (
    if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "XFBIN_ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
)
if not defined XFBIN_ISCC (
    echo FEHLER: Inno Setup 6 nicht gefunden.
    echo Installieren:  winget install JRSoftware.InnoSetup
    goto :fail
)

REM ---- Paket zusammenstellen - dieselbe Form wie INSTALLIERE.bat ----
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%STAGE%\Contents\MacroScripts"          >nul 2>&1
mkdir "%STAGE%\Contents\Post-Start-Up_Scripts" >nul 2>&1

copy /Y package\XfbinImport\PackageContents.xml "%STAGE%\" >nul

set FOUND=0
set MISSING=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%SRC%\%%V\XfbinImport.dlu" (
        mkdir "%STAGE%\Contents\%%V" >nul 2>&1
        copy /Y "%SRC%\%%V\XfbinImport.dlu" "%STAGE%\Contents\%%V\" >nul
        echo  %%V: OK
        set /a FOUND+=1
    ) else (
        echo  %%V: FEHLT  [output\%%V\XfbinImport.dlu]
        set /a MISSING+=1
    )
)
echo.

if !FOUND!==0 (
    echo FEHLER: Keine DLUs in output\. Zuerst BAUE_ALLE.bat ausfuehren.
    goto :fail
)
if !MISSING! GTR 0 if not defined XFBIN_ALLOW_PARTIAL (
    echo FEHLER: !MISSING! Max-Version^(en^) fehlen. Ein Release soll alle
    echo zwoelf enthalten. Bewusst ohne:  set XFBIN_ALLOW_PARTIAL=1
    goto :fail
)

copy /Y "%SCRIPTS%\XfbinImport.mcr"        "%STAGE%\Contents\MacroScripts\"          >nul || goto :fail
copy /Y "%SCRIPTS%\XFBIN_Import.ms"        "%STAGE%\Contents\MacroScripts\"          >nul || goto :fail
copy /Y "%SCRIPTS%\XfbinMenu_2016_2024.ms" "%STAGE%\Contents\Post-Start-Up_Scripts\" >nul || goto :fail
copy /Y "%SCRIPTS%\XfbinMenu_2025_2027.ms" "%STAGE%\Contents\Post-Start-Up_Scripts\" >nul || goto :fail

REM ---- Installer ----
echo Baue Installer...
set "SIGNARG="
if defined XFBIN_SIGNTOOL set "SIGNARG=/DSignToolName=%XFBIN_SIGNTOOL%"

"%XFBIN_ISCC%" /Q /DAppVersion=%VER% "/DPackageDir=%STAGE%" %SIGNARG% installer\XfbinImport.iss
if errorlevel 1 (
    echo FEHLER: Inno Setup ist gescheitert.
    goto :fail
)
echo  OK: dist\XfbinImport-%VER%-Setup.exe
echo.

REM ---- ZIP ----
echo Baue ZIP...
mkdir "%ZIPDIR%" >nul 2>&1
xcopy /E /I /Y /Q "%STAGE%" "%ZIPDIR%\XfbinImport\" >nul
copy /Y installer\Install.bat   "%ZIPDIR%\" >nul
copy /Y installer\Uninstall.bat "%ZIPDIR%\" >nul
copy /Y installer\README.txt    "%ZIPDIR%\" >nul
copy /Y LICENSE                 "%ZIPDIR%\LICENSE.txt" >nul

REM tar ist seit Windows 10 1803 dabei und schreibt echte ZIPs
REM mit Vorwaertsschraegstrichen - anders als Compress-Archive
REM aus Windows PowerShell 5.
pushd "%ZIPDIR%"
tar -a -cf "%DIST%\XfbinImport-%VER%.zip" XfbinImport Install.bat Uninstall.bat README.txt LICENSE.txt
set "TARERR=!errorlevel!"
popd
if not "!TARERR!"=="0" (
    echo FEHLER: ZIP konnte nicht erstellt werden.
    goto :fail
)
echo  OK: dist\XfbinImport-%VER%.zip
echo.

REM ---- Pruefsummen ----
REM certutil statt Get-FileHash: aus PowerShell 7 heraus gestartet
REM findet Windows PowerShell 5 das Cmdlet nicht (geerbter
REM PSModulePath). Die Leerzeichen, die aeltere certutil-Versionen
REM zwischen die Bytes setzen, werden entfernt.
(for %%F in ("%DIST%\*.exe" "%DIST%\*.zip") do (
    set "H="
    for /f "skip=1 delims=" %%H in ('certutil -hashfile "%%~fF" SHA256') do if not defined H set "H=%%H"
    set "H=!H: =!"
    echo !H!  %%~nxF
)) > "%DIST%\SHA256SUMS.txt"
type "%DIST%\SHA256SUMS.txt"
echo.

rmdir /s /q "%DIST%\stage" >nul 2>&1
rmdir /s /q "%ZIPDIR%"     >nul 2>&1

echo ========================================
echo  Fertig. Dateien in dist\
echo ========================================
echo.
echo Hochladen:
echo   gh release create v%VER% --title "XFBIN Import %VER%" dist\XfbinImport-%VER%-Setup.exe dist\XfbinImport-%VER%.zip dist\SHA256SUMS.txt
echo.
if not defined XFBIN_NOPAUSE pause
exit /b 0

:fail
echo.
if not defined XFBIN_NOPAUSE pause
exit /b 1
