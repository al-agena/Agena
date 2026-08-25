@echo off
rem Start Agena via this batch script, for at least Windows 2000 and later.
rem For a desktop icon that beautifies this shortcut, choose one in the share\icons folder.
rem For more details check the readme.w32 file.

rem setlocal will assure that the absolute path (%CD%) is added only once to PATH.
setlocal
set PATH=%CD%\bin;%PATH%
rem the start statement will open a new window with Agena running in it.
start "Agena Portable" bin\agena.exe -i -p lib\
endlocal

