@echo off
echo off

rem *** U2_EXZIP.BAT ***

rem (C) Copyright 1994, Ad Infinitum Programs, all rights reserved

rem U2_EXZIP.BAT accepts a single parameter: an archive file. It
rem decompresses all files (including hidden ones, etc.) of this archive
rem completely into the current directory. If nothing goes wrong, the files
rem U$~CHK1 and U$~CHK2 are created by this batch file. UC tests for the
rem presence of those files.

rem The '%2 ~*' command checks if files/directories are present in the
rem current directory (to check if anything at all has been extracted
rem from the archive) and creates the u$~chk1 file if this is the
rem case. %2 contains the full path of UC.EXE.

rem 1. PK(UN)ZIP 2.0
rem 2. PK(UN)ZIP 1.x
rem 3. (INFO)ZIP

rem ***  PKUNZIP 2.0  ***

   rem If PKUNZIP 2.04ceg fails, (InfoZIP) UNZIP 5.x is often able to do
   rem the job instead.

   rem Expand archive. 'echo x' is to skip the help screen which is
   rem given in case of unknown options (errorlevel 16).

      echo x | PKUNZIP -3 -+ -- -) -Jrhs -o -d %1

   rem Test for correct expansion

      if errorlevel 16 goto zip110
      if errorlevel 1 goto error
      %2 ~*
      if not exist u$~chk1 goto zip110
      goto ok


:zip110
rem ***  PKUNZIP 1.x  ***

   rem Expand archive

      PKUNZIP -Jrhs -o -d %1

   rem Test for correct expansion

      if errorlevel 1 goto zip
      %2 ~*
      if not exist u$~chk1 goto zip
      goto ok


:zip
rem ***  InfoZIP  ***

   rem Expand archive

      UNZIP -o %1

   rem Test for correct expansion

      if errorlevel 1 goto error
      %2 ~*
      if not exist u$~chk1 goto error
      goto ok


:error
rem *** Error handling ***

   if exist u$~chk1 del u$~chk1
   if exist u$~chk2 del u$~chk2
   goto end


:ok
rem *** Expansion was successfull ***

   rem Create second check file to notify complete success to UC

      echo check > u$~chk2


:end
rem *** End of batchfile ***
