@echo off
rem set gccopts=-Wall -O3 -shared -I. -L. -lagena
set gccopts=-Wall -O3 -fgnu89-inline -shared -I. -L. -lagena

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung avl.
gcc %gccopts% -o avl.dll avl.c

strip --strip-unneeded avl.dll
copy avl.dll ..\lib >> NUL
del avl.dll

set gccopts=
set PATH=%OLDPATH%
set OLDPATH=