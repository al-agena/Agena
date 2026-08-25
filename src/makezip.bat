@echo off
rem To build and install additional libraries, execute this file in the \agena\src subfolder
rem in an NT shell, and not in msys/MinGW.

set gccfileopts=-pipe -O3 -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
set gccfinalopts=-pipe -O3 -shared -static-libgcc -fpic -fgnu89-inline -Wno-attributes

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung minizip.

gcc %gccfileopts% -c -o miniz.o miniz.c
gcc %gccfinalopts% -o minizip.dll minizip.c miniz.o -L. -lagena

echo.
echo Slimming and moving package.
strip --strip-unneeded minizip.dll
copy minizip.dll ..\lib >> NUL

echo Installed library into Agena /lib folder.

del minizip.dll miniz.o
set PATH=%OLDPATH%
set OLDPATH=