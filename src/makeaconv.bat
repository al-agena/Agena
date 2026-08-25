@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set pkgname=aconv

set gccopts=-Wall -O3 -shared -I. -L. -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

echo Buildung %pkgname%.
gcc %gccopts% -o %pkgname%.dll %pkgname%.c -lagena -liconv

echo.
echo Slimming and moving packages.
rem strip --strip-unneeded *.dll
copy %pkgname%.dll ..\lib >> NUL

echo Installed all libraries into Agena /lib folder.

del %pkgname%.dll
set PATH=%OLDPATH%
set OLDPATH=
