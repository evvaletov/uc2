@echo off
echo off

REM *** U2_PRINT.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved
rem
rem This file prints (ASCII) files.

echo ARCHIVE: %2  FILE: %3 > prn
echo ---------------------------------------------------------------------- > prn
type %1 > prn
echo  > prn
