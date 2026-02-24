@echo off
del test.zip
pkzipa -es test 8\*.*
del test.zip
call f
mic pkzipa -ex test 8\*.*
