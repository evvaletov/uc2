
X = i686-w64-mingw32-
O = win/
EXE = .exe
compat = compat
OBJ = $(O)unuc2.rc.o

CFLAGS += -march=i686
$(O)unuc2.o: CFLAGS += -I$(compat)/include
$(O)unuc2$(EXE): LDFLAGS += $(O)unuc2.rc.o -u _compat__utf8_console -L$(or $O,.) -l:compat.a

DEPS = $(O)unuc2.rc.o $(O)compat.a
CC = $(X)gcc
LD = $(X)ld
AR = $(X)ar

make_compat = make -C $(compat) T=win O=$(realpath $(or $(O),.))/ funs="compat__utf8_console err errx warn warnx access unlink mkdir chmod chdir mkdir fopen utime fnmatch"
CLEAN = $(RM) $(O)unuc2.rc.o; $(make_compat) clean

include Makefile

VERNUM != VERSION=$(VERSION); echo $${VERSION//[^0-9]/ }
VERNUM := $(or $(word 1, $(VERNUM)),0),$(or $(word 2, $(VERNUM)),0),$(or $(word 3, $(VERNUM)),0),$(or $(word 4, $(VERNUM)),0)

$(O)unuc2.rc.o: unuc2.rc cli.ico Makefile | $O
	$(X)windres $< -o $@ -DVERSION=$(VERSION) -DVERNUM=$(VERNUM)

unuc2.rc: ;

$(O)compat.a: $(compat)/compat.c
	$(make_compat)

ifneq "$O" ""
$(O)unuc2.o: | $O

$O:
	mkdir $O
endif
