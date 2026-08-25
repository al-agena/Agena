@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set gccopts=-Wall -O3 -shared -fgnu89-inline -I. -L. -lagena
set offs=-Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung ads.
gcc -O2 %gccopts% -DLUA_BUILD_AS_DLL -c -o vecoff64.o vecoff64.c
gcc %offs% %gccopts% -c -o ads.o ads.c
gcc -O3 -shared -static-libgcc -fgnu89-inline -fpic -o ads.dll ads.o vecoff64.o -L. -lagena

echo.
echo Slimming and moving `ads` package.
strip --strip-unneeded ads.dll
copy ads.dll ..\lib >> NUL

echo Installed `ads` library into Agena /lib folder.

del ads.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=