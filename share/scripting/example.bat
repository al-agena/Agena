:: Script for DOS

@echo off
set PARAMS=
:loop
if "%1"=="" goto run
set PARAMS=%PARAMS% %1
shift
goto loop
:run
agena example.agn %PARAMS%
