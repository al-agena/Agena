@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

rem DYNAMIC LINKING with MPFR
set mpfropts=-Wall -O3 -fgnu89-inline -shared

rem STATIC LINKING with MPFR
rem set mpfropts=-Wall -pipe -O3 -DLUA_BUILD_AS_DLL -fgnu89-inline -I../ports/mpfr-4.2.1/src -L../ports/mpfr-4.2.1/src/.libs -I. -L.

set mpfrlibs=-I../ports/mpfr-4.2.2/src -L../ports/mpfr-4.2.2/src/.libs -lmpfr -I. -L. -lgmp -I. -L. -lagena
rem put -lmpfr -lgmp -lagena AFTER source filename

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung mpfr.
gcc %mpfropts% -o mpfr.tmp.dll mpfr.c %mpfrlibs%
echo.
echo Slimming and moving `mpfr` package.
strip --strip-unneeded mpfr.tmp.dll
copy mpfr.tmp.dll ..\lib\mpfr.dll >> NUL

echo Installed `mpfr` library into Agena /lib folder.

del mpfr.tmp.dll

set mpfropts=
set mpfrlibs=
set PATH=%OLDPATH%
set OLDPATH=