@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -fgnu89-inline -shared -I. -L. -lagena

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung numarray.
gcc %gccopts% -o numarray.dll numarray.c
echo.
echo Slimming and moving `numarray` package.
strip --strip-unneeded numarray.dll
copy numarray.dll ..\lib >> NUL

echo Installed `numarray` library into Agena /lib folder.

del numarray.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=