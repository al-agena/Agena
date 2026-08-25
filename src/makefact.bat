@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -shared -I. -L. -lagena -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung factory.
gcc %gccopts% -o factory.dll factory.c

echo.
echo Slimming and moving `factory` package.
strip --strip-unneeded factory.dll
copy factory.dll ..\lib >> NUL

echo Installed `factory` library into Agena /lib folder.

del factory.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=