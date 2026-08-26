:: This batch file is for OS/2.

@echo off
setlocal
set PARAMS=

/* Collect all arguments into a single string */
:loop
if "%1"=="" goto run
set PARAMS=%PARAMS% %1
shift
goto loop

:run
/* Call the application with the collected parameters. Change example.agn
   to the name of your script. */
agena example.agn %PARAMS%

endlocal
