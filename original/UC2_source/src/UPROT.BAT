@echo off
echo off

REM *** UPROT.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
echo ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ
echo  °ÛÛÛ   °ÛÛÛ  °ÛÛÛÛ      UltraProtect [1.0]
echo °Û  °Û   °Û   °Û  °Û     "Protect archive against deletion and basic updates"
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

uc ai %1 u$~nobas.lck u$~nodel.lck > nul

if errorlevel 1 goto errl

echo UPROT: %1 has been protected against deletion and basic updates

goto end

:ext
if not exist %1.UC2 goto noaerror

echo q > u$~nobas.lck
echo q > u$~nodel.lck

uc ai %1.uc2 u$~nobas.lck u$~nodel.lck > nul

if errorlevel 1 goto errl

echo UPROT: %1.UC2 has been protected against deletion and basic updates

goto end

:usage
echo Usage: UPROT arch
goto end

:noaerror
echo UPROT ERROR: Cannot find archive
goto end

:errl
echo UPROT ERROR: UC2 reported a problem
goto end

:end
if exist u$~nobas.lck del u$~nobas.lck > nul
if exist u$~nodel.lck del u$~nodel.lck > nul
echo ÿ
