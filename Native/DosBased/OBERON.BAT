@echo off
rem Run this under DOS or in MS-DOS mode under Windows 95
rem Does not work under Windows 3.11 or NT, rather install stand-alone.

if not exist native.par goto usage

rem Show boot status at top left
cls
echo . - Native Oberon booting...

rem Try noboot with -p switch if it displays "D - Native Oberon booting ..."
noboot native.bin native.par

rem We should never get back here
goto end

:usage
echo Please change to the directory containing the installed Native Oberon

:end
