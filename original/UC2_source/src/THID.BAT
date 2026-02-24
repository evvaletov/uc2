@echo off
echo 1 - one
echo 2 - two
ucd ~m
if errorlevel 3 goto error
if errorlevel 2 goto el2
if errorlevel 1 goto el1
if errorlevel 0 goto error
goto error

:el1
echo one
goto end

:el2
echo two
goto end

:error
echo error
goto end

:end
