@echo off
REM ============================================================
REM  VERGLEICHE.bat - C++-Parser gegen die Python-Lib pruefen
REM
REM  Aufruf:
REM    VERGLEICHE.bat <datei.xfbin>
REM
REM  Voraussetzungen:
REM    - BAUE_ALLE.bat wurde ausgefuehrt (output\tools\xfbindump.exe)
REM    - Python 3 im PATH
REM    - Umgebungsvariable XFBIN_ADDON zeigt auf den entpackten
REM      Blender-XFBIN-Importer (den Ordner mit xfbin_lib\)
REM        set XFBIN_ADDON=D:\dev\Blender-XFBIN-Importer-2.5.2
REM
REM  Erwartetes Ergebnis: genau EINE Abweichung in page[0], weil
REM  die Python-Lib den doppelten nuccChunkNull der ersten Page
REM  verschluckt. Siehe Kommentar in pydump.py.
REM ============================================================

setlocal
cd /d "%~dp0"

if "%~1"=="" (
    echo Aufruf: VERGLEICHE.bat ^<datei.xfbin^>
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
    echo Bitte zuerst BAUE_ALLE.bat ausfuehren.
    exit /b 2
)

echo Erzeuge C++-Dump...
"%TOOL%" "%~1" -o "%TEMP%\xfbin_cpp.txt"
if errorlevel 1 exit /b 1

echo Erzeuge Python-Dump...
python "%~dp0pydump.py" "%XFBIN_ADDON%" "%~1" "%TEMP%\xfbin_py.txt"
if errorlevel 1 exit /b 1

echo.
echo ========================================
echo  Unterschiede
echo ========================================
fc /N "%TEMP%\xfbin_py.txt" "%TEMP%\xfbin_cpp.txt"

echo.
echo Dumps liegen in:
echo   %TEMP%\xfbin_py.txt
echo   %TEMP%\xfbin_cpp.txt
exit /b 0
