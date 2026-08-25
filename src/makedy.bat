@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

rem set offs=-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

set gccfileopts=-O3 -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
set gccfinalopts=-O3 -shared -static-libgcc -fpic -fgnu89-inline -Wno-attributes

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung dyvec.
gcc %gccfileopts% -c -o dyvec.o dyvec.c
gcc %gccfileopts% -c -o vector.o vector.c
gcc %gccfinalopts% -o dyvec.dll dyvec.o vector.o -L. -lagena

echo.
echo Slimming and moving `dyvec` package.
strip --strip-unneeded dyvec.dll
copy dyvec.dll ..\lib >> NUL

echo Installed `dyvec` library into Agena /lib folder.

del dyvec.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=