@echo off
REM ============================================================
REM  XFBIN Import 3.0.0 - Paket erstellen + installieren
REM
REM  1. Baut die Paketstruktur aus output\ und scripts\
REM  2. Kopiert das fertige Paket nach ApplicationPlugins
REM
REM  Zielordner (fuer alle Benutzer, wie die anderen Importer und
REM  WhiteoutDex; Benutzer duerfen dort Ordner anlegen, es braucht
REM  keine Adminrechte):
REM    %ProgramData%\Autodesk\ApplicationPlugins\XfbinImport
REM  ausgeschrieben:
REM    C:\ProgramData\Autodesk\ApplicationPlugins\XfbinImport
REM
REM  Eine alte Kopie im Benutzerordner (%APPDATA%, bis 2.x) wird
REM  entfernt - sonst laedt Max das Paket zweimal.
REM
REM  Laeuft ein Max-Jahrgang, ist SEINE .dlu gesperrt. Dann wird nur
REM  dieser Jahrgang uebersprungen und gemeldet; alle anderen und die
REM  Skripte werden ersetzt.
REM
REM  Voraussetzung: BAUE_ALLE.bat wurde bereits ausgefuehrt.
REM ============================================================

setlocal enabledelayedexpansion

cd /d "%~dp0"

set "SRC=%~dp0output"
set "SCRIPTS=%~dp0scripts"
set "PKG=%~dp0package\XfbinImport"
set "DEST=%ProgramData%\Autodesk\ApplicationPlugins\XfbinImport"
set "ALT=%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport"

echo.
echo ========================================
echo  XFBIN Import - installieren
echo ========================================
echo.

if not exist "%SRC%" (
    echo FEHLER: output\ Ordner nicht gefunden!
    echo Bitte zuerst BAUE_ALLE.bat ausfuehren.
    goto :fehler
)
if not exist "%PKG%\PackageContents.xml" (
    echo FEHLER: package\XfbinImport\PackageContents.xml fehlt!
    goto :fehler
)

mkdir "%DEST%\Contents\MacroScripts"          >nul 2>&1
mkdir "%DEST%\Contents\Post-Start-Up_Scripts" >nul 2>&1
mkdir "%DEST%\Contents\Pre-Start-Up_Scripts"  >nul 2>&1
if not exist "%DEST%\Contents" (
    echo FEHLER: Zielordner laesst sich nicht anlegen:
    echo   %DEST%
    goto :fehler
)

echo Plugins nach %DEST%
echo.
set ZAHL=0
set GESPERRT=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%SRC%\%%V\XfbinImport.dlu" (
        mkdir "%DEST%\Contents\%%V" >nul 2>&1
        copy /Y "%SRC%\%%V\XfbinImport.dlu" "%DEST%\Contents\%%V\" >nul 2>&1
        if errorlevel 1 (
            echo  %%V: GESPERRT - 3ds Max %%V laeuft, diese Fassung NICHT ersetzt
            set /a GESPERRT+=1
        ) else (
            echo  %%V: OK
            set /a ZAHL+=1
        )
    ) else (
        echo  %%V: uebersprungen  [keine DLU in output\%%V\]
    )
)
echo.

if !ZAHL!==0 if !GESPERRT!==0 (
    echo FEHLER: Keine kompilierten Plugins gefunden - zuerst BAUE_ALLE.bat.
    goto :fehler
)

REM ---- Manifest und Skripte ----
copy /Y "%PKG%\PackageContents.xml"           "%DEST%\" >nul
copy /Y "%SCRIPTS%\XfbinImport.mcr"           "%DEST%\Contents\MacroScripts\" >nul
copy /Y "%SCRIPTS%\XFBIN_Import.ms"           "%DEST%\Contents\MacroScripts\" >nul
copy /Y "%SCRIPTS%\XfbinMenu_2016_2024.ms"    "%DEST%\Contents\Post-Start-Up_Scripts\" >nul
copy /Y "%SCRIPTS%\XfbinMenu_2025_2027.ms"    "%DEST%\Contents\Pre-Start-Up_Scripts\" >nul
REM bis 3.0 lag das 2025er-Menue im Post-Start-up-Ordner
if exist "%DEST%\Contents\Post-Start-Up_Scripts\XfbinMenu_2025_2027.ms" del /Q "%DEST%\Contents\Post-Start-Up_Scripts\XfbinMenu_2025_2027.ms" >nul 2>&1

set "MISSING="
if not exist "%DEST%\PackageContents.xml"                                 set "MISSING=PackageContents.xml"
if not exist "%DEST%\Contents\MacroScripts\XfbinImport.mcr"               set "MISSING=XfbinImport.mcr"
if not exist "%DEST%\Contents\Pre-Start-Up_Scripts\XfbinMenu_2025_2027.ms" set "MISSING=XfbinMenu_2025_2027.ms"
if defined MISSING (
    echo FEHLER: Im Zielordner fehlt: !MISSING!
    goto :fehler
)

REM ---- alte Kopie im Benutzerordner entfernen ----
if exist "%ALT%" (
    rmdir /s /q "%ALT%" >nul 2>&1
    if exist "%ALT%" (
        echo ACHTUNG: Die alte Kopie in
        echo   %ALT%
        echo ist gesperrt ^(Max laeuft^). Max schliessen und INSTALLIERE.bat erneut ausfuehren,
        echo sonst laedt Max das Paket zweimal.
        echo.
    ) else (
        echo Alte Kopie im Benutzerordner entfernt.
        echo.
    )
)

echo ========================================
if !GESPERRT! GTR 0 (
    echo  Installiert - bis auf !GESPERRT! gesperrte Fassung^(en^).
    echo  Das betroffene Max schliessen und INSTALLIERE.bat erneut ausfuehren.
) else (
    echo  Installiert.
)
echo ========================================
echo.
echo 3ds Max starten:  DH Tools  -^>  XFBIN Import  /  XFBIN Animations
echo Test im Listener: XfbinCpp.version^(^)
echo.
if not defined XFBIN_NOPAUSE pause
exit /b 0

:fehler
echo.
if not defined XFBIN_NOPAUSE pause
exit /b 1
