@echo off
echo off

REM *** U2_FLUSH.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved

rem This batch file is called by UC for flushing the harddisk cache


rem Notify the user

echo --------------------------------------------------------------------
echo U2_FLUSH: Calling multiple disk-cache (delayed write) flush commands
echo           (errors might occur for not installed caching programs)


echo *** Flushing Microsoft SmartDrive (if installed)

   smartdrv /c >nul


echo *** Flushing Norton Cache (if installed)

   ncache /dump > nul


echo *** Flushing ADCache (if installed)

   adcache -f > nul


echo *** Flushing PC-CACHE (if installed)

   pc-cache /flush > nul


echo *** Flushing Super PC KWIK (if installed)

   superpck /f > nul


echo *** Flushing HyperDISK (if installed)

   hyperdk e > nul


echo U2_FLUSH: Task completed
echo --------------------------------------------------------------------
