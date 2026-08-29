@echo off
REM ============================================================
REM  VERGLEICHE_MESHES.bat - Meshes gegen die Python-Lib pruefen
REM
REM  Aufruf:
REM    VERGLEICHE_MESHES.bat <datei.xfbin>
REM
REM  Voraussetzungen wie bei den anderen Vergleichsskripten:
REM    set XFBIN_ADDON=D:\dev\Blender-XFBIN-Importer-2.5.2
REM
REM  Der Dump enthaelt jeden Vertex und jedes Dreieck; bei
REM  1hakbod1.xfbin sind das rund 52.500 Zeilen je Seite.
REM  Erwartetes Ergebnis: KEINE Abweichung.
REM ============================================================

setlocal
cd /d "%~dp0"

if "%~1"=="" (
    echo Aufruf: VERGLEICHE_MESHES.bat ^<datei.xfbin^>
    exit /b 2
)
if not defined XFBIN_ADDON (
    echo FEHLER: Umgebungsvariable XFBIN_ADDON ist nicht gesetzt.
    exit /b 2
)

set "TOOL=%~dp0..\output\tools\xfbindump.exe"
if not exist "%TOOL%" (
    echo FEHLER: %TOOL% nicht gefunden.
    exit /b 2
)

echo Erzeuge C++-Meshdump...
"%TOOL%" "%~1" --meshes-o "%TEMP%\xfbin_mesh_cpp.txt"
if errorlevel 1 exit /b 1

echo Erzeuge Python-Meshdump...
python "%~dp0pydump_meshes.py" "%XFBIN_ADDON%" "%~1" "%TEMP%\xfbin_mesh_py.txt"
if errorlevel 1 exit /b 1

echo.
echo ========================================
echo  Unterschiede
echo ========================================
fc /N "%TEMP%\xfbin_mesh_py.txt" "%TEMP%\xfbin_mesh_cpp.txt"

echo.
echo Dumps liegen in:
echo   %TEMP%\xfbin_mesh_py.txt
echo   %TEMP%\xfbin_mesh_cpp.txt
exit /b 0
