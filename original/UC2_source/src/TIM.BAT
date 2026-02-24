@echo off
call f
del out
del rin
mic uc $$$ COMP %1 in out
dir /a-d *.
