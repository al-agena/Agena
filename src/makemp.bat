@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gmpopts=-Wall -O3 -fgnu89-inline -shared -I. -L.
set gmplibs=-lgmp -lagena
rem put -lgmp -lagena AFTER source filename

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung mp.
gcc %gmpopts% -o mp.dll mp.c %gmplibs%
echo.
echo Slimming and moving `mp` package.
strip --strip-unneeded mp.dll
copy mp.dll ..\lib >> NUL

echo Installed `mp` library into Agena /lib folder.

del mp.dll

set gmpopts=
set gmplibs=
set PATH=%OLDPATH%
set OLDPATH=