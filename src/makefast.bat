@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -Wno-strict-aliasing -O3 -shared -I. -L. -lagena -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung fastmath.
gcc %gccopts% -o fastmath.dll fastmath.c

echo.
echo Slimming and moving packages.
strip --strip-unneeded *.dll
copy fastmath.dll ..\lib >> NUL

echo Installed all libraries into Agena /lib folder.

del fastmath.dll
set PATH=%OLDPATH%
set OLDPATH=
