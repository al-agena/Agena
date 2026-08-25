@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set pkgname=uintmap

set gccfileopts=-O3 -fno-builtin -ffloat-store -fno-unsafe-math-optimizations -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
set gccfinalopts=-O3 -shared -static-libgcc -fpic -fgnu89-inline -Wno-attributes

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

if exist %pkgname%.o (
  del %pkgname%.o
)
if exist %pkgname%.dll (
  del %pkgname%.dll
)

echo Buildung %pkgname%.
gcc %gccfileopts% -c -o %pkgname%.o %pkgname%.c
gcc %gccfinalopts% -o %pkgname%.dll %pkgname%.o -L. -lm -lagena

echo.
echo Slimming and moving `%pkgname%` package.
strip --strip-unneeded %pkgname%.dll
copy %pkgname%.dll ..\lib >> NUL

echo Installed `%pkgname%` library into Agena /lib folder.

del %pkgname%.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=