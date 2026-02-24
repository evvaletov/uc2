@echo off
echo off
rem
rem GENERAL
rem    This is an archive expansion batch file for the
rem    UltraCompressor II (automated) archive conversion
rem    option. The batch file accepts a single parameter,
rem    the full path of the file to be extracted. The batch
rem    expands the archive completely, including system/
rem    hidden/readonly files and the complete diretory structure.
rem    The batch file should also create two files, U$~CHK1 and
rem    U$~CHK2, if EVERYTHING went completely OK. In case of
rem    problems, errors, etc., they should not be made.
rem
rem SPECIFIC
rem    Options are such that the software operates as safe as possible.
rem    Some options as adviced by Douglas Hay, PKWARE support.
rem
echo check > u$~chk1
PKZIP -3 -+ -- -) %1 u$~chk1
del u$~chk1
PKUNZIP -3 -+ -- -) -Jrhs -o -d %1
if errorlevel 1 goto error
goto ok
:error
del u$~chk1
:ok
echo check > u$~chk2
