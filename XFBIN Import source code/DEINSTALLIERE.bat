@echo off
REM ============================================================
REM  XFBIN Import - Deinstallieren
REM  Entfernt das Paket aus ApplicationPlugins.
REM ============================================================

setlocal enabledelayedexpansion
set "DEST=%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport"

echo.
echo Entferne: %DEST%
echo.

if not exist "%DEST%" (
    echo Nichts zu tun - Paket ist nicht installiert.
    pause
    exit /b 0
)

REM Echter Sperrtest statt einer tasklist-Abfrage: die vorhandene
REM Datei zum Anhaengen oeffnen, ohne etwas zu schreiben. Klappt
REM das nicht, haelt ein laufendes Max sie offen.
set "LOCKED="
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%DEST%\Contents\%%V\XfbinImport.dlu" (
        (call ) 2>nul 1>>"%DEST%\Contents\%%V\XfbinImport.dlu" || set "LOCKED=1"
    )
)

if defined LOCKED (
    echo FEHLER: Die Plugin-Datei ist gesperrt.
    echo 3ds Max laeuft vermutlich noch. Bitte schliessen und erneut starten.
    pause
    exit /b 1
)

rmdir /s /q "%DEST%"
if exist "%DEST%" (
    echo FEHLER: Ordner konnte nicht entfernt werden.
    pause
    exit /b 1
)

echo Entfernt.
pause
exit /b 0
