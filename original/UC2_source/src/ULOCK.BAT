@echo off
echo off

REM *** ULOCK.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
echo ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
echo  °ÛÛÛ   °ÛÛÛ  °ÛÛÛÛ      UltraLock [1.0]
echo °Û  °Û   °Û   °Û  °Û     "Protect archive against all manipulative commands"
echo °ÛÛÛÛÛ   °Û   °ÛÛÛÛ  -NL
echo °Û  °Û   °Û   °Û ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ
echo °Û  °Û  °ÛÛÛ  °Û (C) Copyright 1994, Ad Infinitum Programs, all rights reserved
echo ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
echo ÿ

if q%1==q goto usage
if not exist %1 goto ext

REM no BASIC updates
echo q > u$~nobas.lck

REM no DELETE
echo q > u$~nodel.lck

REM no ADD
echo q > u$~noadd.lck

REM no OPTIMIZE
echo q > u$~noopt.lck

REM no UNPROTECT
echo q > u$~nounp.lck

REM no REVISE COMMENT
echo q > u$~norev.lck

uc ai %1 u$~nobas.lck u$~nodel.lck u$~noadd.lck u$~noopt.lck u$~nounp.lck u$~norev.lck > nul

if errorlevel 1 goto errl

echo ULOCK: %1 has been protected against all manipulative commands

goto end

:ext
if not exist %1.UC2 goto noaerror

echo q > u$~nobas.lck
echo q > u$~nodel.lck
echo q > u$~noadd.lck
echo q > u$~noopt.lck
echo q > u$~nounp.lck
echo q > u$~norev.lck

uc ai %1.uc2 u$~nobas.lck u$~nodel.lck u$~noadd.lck u$~noopt.lck u$~nounp.lck u$~norev.lck > nul

if errorlevel 1 goto errl

echo ULOCK: %1.UC2 has been protected against all manipulative commands

goto end

:usage
echo Usage: ULOCK arch
goto end

:noaerror
echo ULOCK ERROR: Cannot find archive
goto end

:errl
echo ULOCK ERROR: UC2 reported a problem
goto end

:end
if exist u$~nobas.lck del u$~nobas.lck > nul
if exist u$~nodel.lck del u$~nodel.lck > nul
if exist u$~noadd.lck del u$~noadd.lck > nul
if exist u$~noopt.lck del u$~noopt.lck > nul
if exist u$~nounp.lck del u$~nounp.lck > nul
if exist u$~norev.lck del u$~norev.lck > nul
echo ÿ
