@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gmpopts=-Wall -O3 -fgnu89-inline -shared -I. -L.
set gmplibs=-lgmp -lagena
rem put -lgmp -lagena AFTER source filename

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung gmp.
gcc %gmpopts% -o gmp.tmp.dll gmp.c %gmplibs%
echo.
echo Slimming and moving `gmp` package.
strip --strip-unneeded gmp.tmp.dll
copy gmp.tmp.dll ..\lib\gmp.dll >> NUL

echo Installed `gmp` library into Agena /lib folder.

del gmp.tmp.dll

set gmpopts=
set gmplibs=
set PATH=%OLDPATH%
set OLDPATH=