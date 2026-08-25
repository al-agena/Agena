@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -shared -I. -L. -lagena -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung zx.
gcc %gccopts% -o zx.dll zx.c

echo.
echo Slimming and moving `zx` package.
strip --strip-unneeded zx.dll
copy zx.dll ..\lib >> NUL

echo Installed `zx` library into Agena /lib folder.

del zx.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=