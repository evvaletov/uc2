@echo off
del out
del rin
uc $$$ COMP 12 in out
uc $$$ DCMP out rin
echo n | COMP in rin
dir /a-d *.
