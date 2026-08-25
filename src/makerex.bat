@echo off
rem To build and install the additional libraries, execute this file in the \agena\src subfolder
rem in a plain NT shell, and not in MinGW.

set pkgname=regex

set gccopts=-Wall -O3 -DPCRE2_CODE_UNIT_WIDTH=8 -shared -I. -L. -fgnu89-inline

set OLDPATH=%PATH%
set PATH=%PATH%;c:\mingw\bin

if exist %pkgname%.o (
  del %pkgname%.o
)
if exist %pkgname%.dll (
  del %pkgname%.dll
)

echo Buildung %pkgname%.
gcc %gccopts% -o %pkgname%.dll %pkgname%.c regex_f.c regex.h regexcom.c regexcom.h regexalg.h -lagena -lpcre2-posix -lpcre2-8

echo.
echo Slimming and moving packages.
strip --strip-unneeded *.dll
copy %pkgname%.dll ..\lib >> NUL

echo Installed library into Agena /lib folder.

del %pkgname%.dll
set PATH=%OLDPATH%
set OLDPATH=
