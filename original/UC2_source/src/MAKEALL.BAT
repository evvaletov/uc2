del all.uc2
fd \uc\dist\*.* /d07-01-94 /t2:20:00
del \uc\dist\*.log
uci a -tst all \uc\dist\*.*
uci otst all
