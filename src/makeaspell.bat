@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set aspellopts=-Wall -O3 -fgnu89-inline -shared -I. -L.
set aspelllibs=-laspell -lagena
rem put -laspell -lagena AFTER source filename

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung aspell.
gcc %aspellopts% -o aspell.tmp.dll aspell.c %aspelllibs%
echo.
echo Slimming and moving `aspell` package.
strip --strip-unneeded aspell.tmp.dll
copy aspell.tmp.dll ..\lib\aspell.dll >> NUL

echo Installed `aspell` library into Agena /lib folder.

del aspell.tmp.dll

set aspellopts=
set aspelllibs=
set PATH=%OLDPATH%
set OLDPATH=