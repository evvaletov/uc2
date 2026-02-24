funs := err errx warn warnx setlinebuf fnmatch
funs += fopen access chmod chdir mkdir utime

ifeq "$(T)" "win"
X = i686-w64-mingw32-
CC = $(X)gcc
AR = $(X)ar
EXE = .exe
O ?= win/
funs := compat__utf8_console $(funs)
endif

d_err = vwarn
d_errx = vwarnx
d_verr = vwarn
d_verrx = vwarnx
d_warn = vwarn
d_warnx = vwarnx
ifeq "$T" "win"
d_vwarn = getprogname
d_vwarnx = getprogname
endif
d_access = compat__wpath
d_chmod = compat__wpath
d_chdir = compat__wpath
d_mkdir = compat__wpath
d_utime = compat__wpath
override funs := $(sort $(funs) $(foreach f,$(funs),$(d_$f)))
override funs := $(sort $(funs) $(foreach f,$(funs),$(d_$f)))
override funs := $(sort $(funs) $(foreach f,$(funs),$(d_$f)))

objs := $(addprefix $O,$(addsuffix .o,$(funs)))
lib := $(O)compat.a

CFLAGS = -Os -std=c99 -Iinclude

$(lib): $(objs)
	$(AR) rs $(lib) $(objs)

$(objs): compat.c | $O
	$(CC) -c $(CFLAGS) -Dg_$(*F) compat.c -o $@

$O:
	mkdir $O

.PHONY: clean
clean:
	rm -rf $(lib) $(objs) compat-test$(EXE)

.PHONY: test
test: compat-test$(EXE)

compat-test$(EXE): compat-test.c $(lib)
	$(CC) $(CFLAGS) -I. compat-test.c $(lib) $(LDFLAGS) -o $@
