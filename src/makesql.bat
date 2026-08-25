@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccfileopts=-pipe -O3 -DLUA_BUILD_AS_DLL -fgnu89-inline -Wno-attributes
set gccfinalopts=-pipe -O3 -shared -static-libgcc -fpic -fgnu89-inline -Wno-attributes

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung sqlite.

gcc %gccfileopts% -c -o sqlite3.o sqlite3.c
gcc %gccfinalopts% -o sqlite.dll sqlite.c sqlite3.o -L. -lagena

echo.
echo Slimming and moving packages.
strip --strip-unneeded sqlite.dll
copy sqlite.dll ..\lib >> NUL

echo Installed library into Agena /lib folder.

del sqlite.dll sqlite.o sqlite3.o
set PATH=%OLDPATH%
set OLDPATH=
