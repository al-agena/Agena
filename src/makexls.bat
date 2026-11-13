@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -shared -fgnu89-inline -I. -L. -lxlsreader -lxlsxio_read -lxlsxio_write -lminizip -lexpat -lz -lagena
set offs=-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Building xls...

rem Radikaler Clean im aktuellen Verzeichnis vor dem Start
if exist xls.o del /f /q xls.o
if exist xls_temp.dll del /f /q xls_temp.dll
if exist xls.dll del /f /q xls.dll

rem 1. Kompilieren des Quellcodes in die Objektdatei
gcc %offs% %gccopts% -c -o xls.o xls.c

rem 2. Linken zu einer TEMPORÄREN DLL, um "file truncated" zu verhindern
gcc -O3 -shared -static-libgcc -fgnu89-inline -fpic -o xls_temp.dll xls.o -L. -lagena -lxlsreader -lxlsxio_read -lxlsxio_write -lminizip -lexpat -lz

echo.
echo Slimming and moving xls package...

rem 3. Wenn die temporäre DLL gebaut wurde, verarbeiten wir sie
if exist xls_temp.dll (
  strip --strip-unneeded xls_temp.dll
  rem Kopieren in den lib-Ordner direkt unter dem finalen Namen
  copy xls_temp.dll ..\lib\xls.dll >> NUL
  rem Temporäre Datei aufräumen
  del /f /q xls_temp.dll
  if exist xls.o del /f /q xls.o
  echo SUCCESS: xls.dll successfully installed.
) else (
  echo ERROR: Kompilierung fehlgeschlagen! xls_temp.dll has not been created.
)

set gccopts=
set offs=
set PATH=%OLDPATH%
set OLDPATH=