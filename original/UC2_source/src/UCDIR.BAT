@echo off
echo off

REM *** UCDIR.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
cls
echo ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
echo  °ÛÛÛ   °ÛÛÛ  °ÛÛÛÛ      UCDIR, Ultra Compress Directory [1.0]
echo °Û  °Û   °Û   °Û  °Û     'Compresses a complete directory structure into
echo °ÛÛÛÛÛ   °Û   °ÛÛÛÛ  -NL  an UC2 archive to save diskspace'
echo °Û  °Û   °Û   °Û ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
echo °Û  °Û  °ÛÛÛ  °Û (C) Copyright 1994, Ad Infinitum Programs, all rights reserved
echo ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ

if exist ucdir.bat goto ucderror

if exist ucdirdat.u~k goto ukerror

if not q%1q==qq goto paerror

echo ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
set UC2_WIN=xxxxxxxxxxxxxxx > nul
if not q%UC2_WIN%==qxxxxxxxxxxxxxxx goto smallenv
set UC2_WIN=003 009 078 020
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ³                                                                             ³
echo ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
:smallenv
UC A -P -S -I !SYSHID=ON !SOS2EA=ON UCDIRDAT
echo ÿ
echo ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
set uc2_win=

if errorlevel 1 goto archerror

ren ucdirdat.uc2 ucdirdat.u~k > nul
if not exist ucdirdat.u~k goto renerror

echo UCDIR: Moving files...
UC ~K .
if not exist u$~reslt.ok goto kerror
del u$~reslt.ok > nul

:lastren
ren ucdirdat.u~k ucdirdat.uc2 > nul
if not exist ucdirdat.uc2 goto ren2error
attrib +r ucdirdat.uc2 > nul

echo UCDIR: All files/directories in the current directory and below have been
echo        moved to UCDIRDAT.UC2

goto end

:paerror
echo UCDIR ERROR: No parameters allowed.
goto end

:ucderror
echo UCDIR ERROR: UCDIR.BAT should not exist in the current directory.
goto end

:ukerror
echo UCDIR ERROR: The file UCDIRDAT.U~K should be removed to allow UCDIR to work.
goto end

:archerror
echo UCDIR ERROR: Something went wrong during the compression of your files.
echo              None of your files has been moved by UCDIR.
goto end

:renerror
echo UCDIR ERROR: UCDIR was unable to rename UCDIRDAT.UC2 to UCDIRDAT.U~K
goto end

:ren2error
echo UCDIR ERROR: UCDIR was unable to rename UCDIRDAT.U~K to UCDIRDAT.UC2
echo              You MUST correct this !!!
goto end

:kerror
echo UCDIR ERROR: Not all files have been moved, some remain uncompressed.
goto lastren

:end
