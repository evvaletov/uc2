del *.obj
make -f ucd.mak
copy ucd.exe uc.exe
call makeall
tdstrip uc.exe
lzexe uc.exe
dam uc.exe 28 32
uctmp ###
uctmp @@@
del uc2ins.exe
ren uc.exe uc2ins.exe
set uc2_path=
