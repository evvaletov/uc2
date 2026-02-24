@echo off
del test.zip
call f
mic pkzipa -a %1 test in
pkzip -v test
