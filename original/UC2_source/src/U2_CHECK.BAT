@echo off
echo off

REM *** U2_CHECK.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved
rem
rem This batch file is called to check the contents of an archive. The
rem current directory and all subdirectories can be checked for
rem viruses etc. If no problems are found U$~CHK3 should be created.
rem

rem *** McAfee 100 or higher ***

   scan . /sub

   if errorlevel 1 goto end


rem *** F-PROT ***

   f-prot .

   if errorlevel 1 goto end


echo check > u$~chk3
:end
