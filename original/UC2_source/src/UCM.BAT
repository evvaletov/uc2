copy ucd.exe uc.exe
tdstrip uc.exe
lzexe uc.exe
uctmp ###
del uci.exe
ren uc.exe uci.exe
copy uci.exe d:\tools
set uc2_path=
del uci?.uue
uuencode uci.exe
copy uci?.uue e:\mcs
