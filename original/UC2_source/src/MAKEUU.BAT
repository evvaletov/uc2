@echo off
fd \uc\distuu\*.* /d01-01-94 /t2:00:00
del crysaf.uc2
uci a -tst crysaf \uc\distuu\
