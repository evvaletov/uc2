@echo off
echo off

REM *** U2_SHOW.BAT ***

rem speedup execution under 4DOS
if "%@eval[2+2]" == "4" loadbtm on
rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved

rem This batch file is called by UC to play/show the multimedia banners.
rem If you prefer to use other software, you can modify this batch file.


   rem Test if MOD banner is present

      if not exist u$~ban.mod goto nomod

   rem Play MOD music file using MODPLAY

      mp u$~ban.mod


:nomod
   rem Test if GIF banner is present

      if not exist u$~ban.gif goto nogif

   rem View GIF using VPIC

      vpic u$~ban.gif


:nogif
   rem Test if JPG banner is present

      if not exist u$~ban.jpg goto nojpg

   rem View JPG using FullView

      fv u$~ban.jpg


:nojpg
   rem Test if TXT banner is present

      if not exist u$~ban.txt goto notxt

   rem View TXT using DOS TYPE

      type u$~ban.txt

      goto end


:notxt
   rem Clear screen if no TXT banner was present

      cls

:end
