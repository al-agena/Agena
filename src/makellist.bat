@echo off
set gccopts=-Wall -O3 -fgnu89-inline -shared -I. -L. -lagena

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung llist.
gcc %gccopts% -o llist.dll llist.c

strip --strip-unneeded llist.dll
copy llist.dll ..\lib >> NUL
del llist.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=