
X = i686-w64-mingw32-
CC = $(X)gcc
CFLAGS = -Os

unuc2.wcx unuc2.wcx.def &: unuc2-wcx.c ../win/libunuc2.a
	$(CC) -I.. $(CFLAGS) -march=i686 -shared -Wl,--dll -Wl,--kill-at -Wl,--output-def=unuc2.wcx.def $^ -o unuc2.wcx
	$(X)strip unuc2.wcx

../win/libunuc2.a:
	make -C .. -f win.mk win/libunuc2.a
