@echo off
echo off

rem This file is called after extraction of an archive (during
rem conversion) and allows 'extra' commands. This is usefull for
rem automating addition of banners.

rem Please notice this program 'starts' in the directory where the
rem archive has been expanded.

rem EXAMPLE, adds TXT banner if it is not already in the archive.

rem IF EXIST U$~BAN.TXT GOTO SKIP
rem COPY \BANNERS\U$~BAN.TXT
rem :SKIP

ECHO !!! EXTRA !!!
