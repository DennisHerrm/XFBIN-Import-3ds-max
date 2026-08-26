@echo off
REM ============================================================
REM  XFBIN Import 1.9.2 - Paket erstellen + installieren
REM
REM  1. Baut die Paketstruktur aus output\ und scripts\
REM  2. Kopiert das fertige Paket nach ApplicationPlugins
REM
REM  Zielordner (pro Benutzer, kein Admin noetig):
REM    %APPDATA%\Autodesk\ApplicationPlugins\XfbinImport
REM  ausgeschrieben:
REM    C:\Users\<du>\AppData\Roaming\Autodesk\ApplicationPlugins\XfbinImport
REM
REM  Voraussetzung: BAUE_ALLE.bat wurde bereits ausgefuehrt.
REM ============================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "SRC=%~dp0output"
set "SCRIPTS=%~dp0scripts"
set "PKG=%~dp0package\XfbinImport"
set "DEST=%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport"

echo.
echo ========================================
echo  XFBIN Import - Paket erstellen
echo ========================================
echo.

if not exist "%SRC%" (
    echo FEHLER: output\ Ordner nicht gefunden!
    echo Bitte zuerst BAUE_ALLE.bat ausfuehren.
    pause
    exit /b 1
)

if not exist "%PKG%\PackageContents.xml" (
    echo FEHLER: package\XfbinImport\PackageContents.xml fehlt!
    pause
    exit /b 1
)

REM ============================================================
REM  Ist eine bereits installierte DLU gesperrt?
REM
REM  Frueher stand hier eine tasklist-Abfrage auf 3dsmax.exe.
REM  Das war die falsche Pruefung: sie testet, ob ein Prozess
REM  laeuft, statt ob die Datei schreibbar ist - und lag damit
REM  auch dann daneben, wenn Max gar nicht lief.
REM
REM  Jetzt wird die echte Bedingung getestet: die vorhandene
REM  Datei wird zum Anhaengen geoeffnet (ohne etwas zu
REM  schreiben). Klappt das, ist sie frei.
REM
REM  Bei der ERSTEN Installation gibt es nichts zu pruefen -
REM  die Schleife laeuft dann einfach ins Leere.
REM
REM  Notfalls ueberspringen:  set XFBIN_SKIP_LOCKCHECK=1
REM ============================================================

set "LOCKED="

if defined XFBIN_SKIP_LOCKCHECK (
    echo Sperrpruefung uebersprungen ^(XFBIN_SKIP_LOCKCHECK^).
    echo.
) else (
    if exist "%DEST%" (
        for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
            if exist "%DEST%\Contents\%%V\XfbinImport.dlu" (
                (call ) 2>nul 1>>"%DEST%\Contents\%%V\XfbinImport.dlu" || set "LOCKED=1"
            )
        )
    )
)

if defined LOCKED (
    echo FEHLER: Eine bereits installierte XfbinImport.dlu ist gesperrt.
    echo.
    echo Das heisst fast immer: 3ds Max laeuft noch und hat das Plugin
    echo geladen. Bitte alle Max-Instanzen schliessen und erneut starten.
    echo.
    pause
    exit /b 1
)

REM ---- Alte Paketinhalte entfernen, damit keine Reste ----
REM ---- aus einem frueheren Build mitgeschleppt werden  ----
REM ---- PackageContents.xml selbst bleibt stehen.       ----
if exist "%PKG%\Contents" rmdir /s /q "%PKG%\Contents" >nul 2>&1

echo Kopiere Plugins in das Paket...
echo.

set FOUND=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%SRC%\%%V\XfbinImport.dlu" (
        mkdir "%PKG%\Contents\%%V" >nul 2>&1
        copy /Y "%SRC%\%%V\XfbinImport.dlu" "%PKG%\Contents\%%V\" >nul
        echo  %%V: OK
        set /a FOUND+=1
    ) else (
        echo  %%V: UEBERSPRUNGEN  [keine DLU in output\%%V\]
    )
)

echo.
if %FOUND%==0 (
    echo FEHLER: Keine kompilierten Plugins gefunden!
    echo Bitte zuerst BAUE_ALLE.bat ausfuehren.
    pause
    exit /b 1
)

REM ---- Skripte: Oberflaeche und Menueeintraege ----
REM Die liegen im Quellbaum unter scripts\ und werden bei jedem
REM Lauf frisch ins Paket kopiert. Deshalb ueberlebt ein
REM Bearbeiten von scripts\*.mcr das naechste INSTALLIERE.bat.
echo Kopiere Skripte in das Paket...

mkdir "%PKG%\Contents\MacroScripts"          >nul 2>&1
mkdir "%PKG%\Contents\Post-Start-Up_Scripts" >nul 2>&1

copy /Y "%SCRIPTS%\XfbinImport.mcr"          "%PKG%\Contents\MacroScripts\"          >nul
copy /Y "%SCRIPTS%\XfbinMenu_2016_2024.ms"   "%PKG%\Contents\Post-Start-Up_Scripts\" >nul
copy /Y "%SCRIPTS%\XfbinMenu_2025_2027.ms"   "%PKG%\Contents\Post-Start-Up_Scripts\" >nul
copy /Y "%SCRIPTS%\XFBIN_Import.ms"          "%PKG%\Contents\MacroScripts\"          >nul

if not exist "%PKG%\Contents\MacroScripts\XfbinImport.mcr" (
    echo FEHLER: XfbinImport.mcr konnte nicht kopiert werden.
    pause
    exit /b 1
)
echo  Skripte: OK
echo.

echo Installiere nach:
echo   %DEST%
echo.

if exist "%DEST%" rmdir /s /q "%DEST%" >nul 2>&1
mkdir "%DEST%" >nul 2>&1

if not exist "%DEST%" (
    echo FEHLER: Zielordner konnte nicht angelegt werden.
    echo   %DEST%
    pause
    exit /b 1
)

xcopy /E /I /Y "%PKG%\*" "%DEST%\" >nul
if errorlevel 1 (
    echo FEHLER beim Kopieren nach %DEST%
    pause
    exit /b 1
)

REM ---- Gegenpruefen, dass wirklich alles angekommen ist. ----
REM ---- xcopy meldet nicht jeden Teilausfall als Fehler.  ----
set "MISSING="
if not exist "%DEST%\PackageContents.xml"                          set "MISSING=PackageContents.xml"
if not exist "%DEST%\Contents\MacroScripts\XfbinImport.mcr"        set "MISSING=XfbinImport.mcr"
if not exist "%DEST%\Contents\Post-Start-Up_Scripts\XfbinMenu_2025_2027.ms" set "MISSING=XfbinMenu_2025_2027.ms"

if defined MISSING (
    echo FEHLER: Im Zielordner fehlt: !MISSING!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Installiert.
echo ========================================
echo.
echo 3ds Max starten. Danach:
echo.
echo   Hauptmenue  -^>  DH Tools  -^>  XFBIN Import
echo.
echo Erscheint das Menue nicht sofort, einmal Max neu starten -
echo der Menue-Callback greift erst beim naechsten Aufbau.
echo Der Eintrag steht ausserdem immer unter
echo   Customize -^> Customize User Interface -^> Category "DH Tools"
echo.
echo Test im Listener:
echo   XfbinCpp.version^(^)
echo.
pause
exit /b 0
