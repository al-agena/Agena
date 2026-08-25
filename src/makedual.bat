@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

rem set gccopts=-Wall -O3 -shared -I. -L. -lagena -fgnu89-inline -Wno-attributes
set gccopts=-Wall -Wno-attributes -fgnu89-inline -O3 -shared -I. -L. -lagena

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung dual.
gcc %gccopts% -o dual.dll dual.c cephes.o

echo.
echo Slimming and moving `dual` package.
strip --strip-unneeded dual.dll
copy dual.dll ..\lib >> NUL

echo Installed `dual` library into Agena /lib folder.

del dual.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=