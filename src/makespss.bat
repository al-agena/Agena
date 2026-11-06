@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -shared -fgnu89-inline -I. -L. -lagena -lreadstat
set offs=-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Building readstat...

rem Radikaler Clean im aktuellen Verzeichnis vor dem Start
if exist readstat.o del /f /q readstat.o
if exist readstat_temp.dll del /f /q readstat_temp.dll
if exist readstat.dll del /f /q readstat.dll

rem 1. Kompilieren des Quellcodes in die Objektdatei
gcc %offs% %gccopts% -c -o readstat.o readstat.c

rem 2. Linken zu einer TEMPORÄREN DLL, um "file truncated" zu verhindern
gcc -O3 -shared -static-libgcc -fgnu89-inline -fpic -o readstat_temp.dll readstat.o -L. -lagena -lreadstat

echo.
echo Slimming and moving `readstat` package...

rem 3. Wenn die temporäre DLL gebaut wurde, verarbeiten wir sie
if exist readstat_temp.dll (
  strip --strip-unneeded readstat_temp.dll
  rem Kopieren in den lib-Ordner direkt unter dem finalen Namen
  copy readstat_temp.dll ..\lib\readstat.dll >> NUL
  rem Temporäre Datei aufräumen
  del /f /q readstat_temp.dll
  if exist readstat.o del /f /q readstat.o
  echo SUCCESS: readstat.dll erfolgreich in Agena/lib installiert!
) else (
  echo ERROR: Kompilierung fehlgeschlagen! readstat_temp.dll wurde nicht erzeugt.
)

set gccopts=
set offs=
set PATH=%OLDPATH%
set OLDPATH=