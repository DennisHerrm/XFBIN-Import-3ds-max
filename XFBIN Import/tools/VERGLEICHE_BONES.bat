@echo off
REM ============================================================
REM  VERGLEICHE_BONES.bat - Skelett gegen die Python-Lib pruefen
REM
REM  Aufruf:
REM    VERGLEICHE_BONES.bat <datei.xfbin>
REM
REM  Voraussetzungen wie bei VERGLEICHE.bat:
REM    - BAUE_ALLE.bat gelaufen
REM    - Python 3 im PATH, numpy installiert
REM    - set XFBIN_ADDON=D:\dev\Blender-XFBIN-Importer-2.5.2
REM
REM  Erwartetes Ergebnis: KEINE Abweichung. Anders als beim
REM  Container-Dump gibt es hier keinen bekannten Unterschied -
REM  beide Seiten rechnen in double und geben sechs
REM  Nachkommastellen aus.
REM ============================================================

setlocal
cd /d "%~dp0"

if "%~1"=="" (
    echo Aufruf: VERGLEICHE_BONES.bat ^<datei.xfbin^>
    exit /b 2
)

if not defined XFBIN_ADDON (
    echo FEHLER: Umgebungsvariable XFBIN_ADDON ist nicht gesetzt.
    echo   set XFBIN_ADDON=D:\dev\Blender-XFBIN-Importer-2.5.2
    exit /b 2
)

set "TOOL=%~dp0..\output\tools\xfbindump.exe"
if not exist "%TOOL%" (
    echo FEHLER: %TOOL% nicht gefunden.
    exit /b 2
)

echo Erzeuge C++-Skelettdump...
"%TOOL%" "%~1" --bones-o "%TEMP%\xfbin_bones_cpp.txt"
if errorlevel 1 exit /b 1

echo Erzeuge Python-Skelettdump...
python "%~dp0pydump_bones.py" "%XFBIN_ADDON%" "%~1" "%TEMP%\xfbin_bones_py.txt"
if errorlevel 1 exit /b 1

echo.
echo ========================================
echo  Unterschiede
echo ========================================
fc /N "%TEMP%\xfbin_bones_py.txt" "%TEMP%\xfbin_bones_cpp.txt"

echo.
echo Dumps liegen in:
echo   %TEMP%\xfbin_bones_py.txt
echo   %TEMP%\xfbin_bones_cpp.txt
exit /b 0
