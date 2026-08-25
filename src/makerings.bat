@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set pkgname=rings

set gccopts=-Wall -O3 -shared -I. -L. -lagena -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

if exist %pkgname%.o (
  del %pkgname%.o
)
if exist %pkgname%.dll (
  del %pkgname%.dll
)

echo Buildung %pkgname%.
gcc %gccopts% -o %pkgname%.dll %pkgname%.c

echo.
echo Slimming and moving packages.
strip --strip-unneeded %pkgname%.dll
copy %pkgname%.dll ..\lib >> NUL

echo Installed all libraries into Agena /lib folder.

del %pkgname%.dll
set PATH=%OLDPATH%
set OLDPATH=
