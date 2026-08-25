@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -fgnu89-inline -shared -I. -L. -lagena

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung llist.
gcc %gccopts% -o llist.dll llist.c

echo.
echo Slimming and moving `llist` package.
strip --strip-unneeded llist.dll
copy llist.dll ..\lib >> NUL

echo Installed `llist` library into Agena /lib folder.

del llist.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=