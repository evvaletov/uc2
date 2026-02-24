@echo off
set UC2_PATH=d:\uc\source\uctmp.exe
copy tmps\ucd.dsk > nul
del *.swp > nul
del *.tmp > nul
\borlandc\bin\bc %1 %2 %3 %4 %5 %6 %7 %8 %9
copy ucd.dsk tmps > nul
copy ucd.exe \tools
