@echo off
echo off

REM *** U2_XTRA.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved

rem This file is called after extraction while converting an archive. It
rem contains commands which are executed in the directory where the archive
rem has been expanded. This is useful for e.g. automatic addition of
rem banners.

rem This file can (still) reject the conversion process by deleting the
rem u$~chk1 file made by U2_EX???.

rem Example: adds TXT banner if it was not already in the archive

   rem    if exist u$~ban.txt goto skip
   rem    copy \banners\u$~ban.txt
   rem    :skip

rem *** Commands ***

   echo U2_XTRA.BAT execution (currently does nothing)
