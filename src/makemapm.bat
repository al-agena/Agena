@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set pkgname=mapm

set gccopts=-Wall -O3 -shared -I. -L. -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

if exist %pkgname%.o (
  del %pkgname%.o
)
if exist %pkgname%.dll (
  del %pkgname%.dll
)

echo Buildung %pkgname%.
gcc %gccopts% -o %pkgname%.1.dll %pkgname%.c -lagena -lmapm
rename %pkgname%.1.dll %pkgname%.dll

echo.
echo Slimming and moving packages.
strip --strip-unneeded mapm.dll
copy mapm.dll ..\lib >> NUL

echo Installed all libraries into Agena /lib folder.

del mapm.dll
set PATH=%OLDPATH%
set gccopts=
