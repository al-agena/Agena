:: Script for DOS (agena.exe and agena.dll must be in your PATH)

@echo off
set PARAMS=
:loop
if "%1"=="" goto run
set PARAMS=%PARAMS% %1
shift
goto loop
:run
agena whereis.agn %PARAMS%
