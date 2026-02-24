@echo off
del test.zip
call f
mic pkzipa -a -ex test in
pkzip -v test
