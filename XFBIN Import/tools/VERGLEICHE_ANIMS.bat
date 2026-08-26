@echo off
REM ============================================================
REM  VERGLEICHE_ANIMS.bat - Animationen gegen die Python-Lib
REM
REM  Aufruf:
REM    VERGLEICHE_ANIMS.bat <datei.xfbin>
REM
REM  set XFBIN_ADDON=D:\dev\Blender-XFBIN-Importer-2.5.2
REM
REM  Bei 1hakbod1c.xfbin sind das rund 100.400 Zeilen je Seite.
REM  Erwartetes Ergebnis: KEINE Abweichung.
REM ============================================================

setlocal
cd /d "%~dp0"

if "%~1"=="" (
    echo Aufruf: VERGLEICHE_ANIMS.bat ^<datei.xfbin^>
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

echo Erzeuge C++-Animdump...
"%TOOL%" "%~1" --anims-o "%TEMP%\xfbin_anm_cpp.txt"
if errorlevel 1 exit /b 1

echo Erzeuge Python-Animdump...
python "%~dp0pydump_anims.py" "%XFBIN_ADDON%" "%~1" "%TEMP%\xfbin_anm_py.txt"
if errorlevel 1 exit /b 1

echo.
echo ========================================
echo  Unterschiede
echo ========================================
fc /N "%TEMP%\xfbin_anm_py.txt" "%TEMP%\xfbin_anm_cpp.txt"
exit /b 0
