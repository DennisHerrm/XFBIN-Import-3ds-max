@echo off
REM ============================================================
REM  XFBIN Import - Deinstallieren
REM  Entfernt das Paket aus ApplicationPlugins - aus dem Ordner
REM  fuer alle Benutzer (%ProgramData%) und eine alte Kopie im
REM  Benutzerordner (%APPDATA%, bis 2.x).
REM ============================================================

setlocal enabledelayedexpansion
set "DEST=%ProgramData%\Autodesk\ApplicationPlugins\XfbinImport"
set "ALT=%APPDATA%\Autodesk\ApplicationPlugins\XfbinImport"

echo.
set "GEFUNDEN="
for %%D in ("%DEST%" "%ALT%") do (
    if exist "%%~D" (
        set "GEFUNDEN=1"
        echo Entferne: %%~D
        rmdir /s /q "%%~D" >nul 2>&1
        if exist "%%~D" (
            echo   FEHLER: gesperrt - 3ds Max laeuft vermutlich noch. Bitte schliessen und erneut starten.
        ) else (
            echo   entfernt.
        )
    )
)
if not defined GEFUNDEN echo Nichts zu tun - XFBIN Import ist nicht installiert.
echo.
pause
exit /b 0
