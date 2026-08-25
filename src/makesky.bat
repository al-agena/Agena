@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -shared -I. -L. -lagena -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung skycrane.
gcc %gccopts% -o skycrane.dll skycrane.c agncmpt.h

echo.
echo Slimming and moving `skycrane` package.
strip --strip-unneeded skycrane.dll
copy skycrane.dll ..\lib >> NUL

echo Installed `skycrane` library into Agena /lib folder.

del skycrane.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=