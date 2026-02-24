@echo off
echo off

REM *** U2_EDIT.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved

rem This batch file is called by UC for viewing/editing (comment) files
rem Default editor is the DOS 5.0 standard editor

   edit %1
