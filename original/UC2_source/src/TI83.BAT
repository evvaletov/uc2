@echo off
del test.uc2
uc a test 8\*.*
del test.uc2
call f
mic uc a -t3 test 8\*.*
