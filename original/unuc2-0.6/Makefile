# gmake

NAME = unuc2
VERSION = 0.6
BINDIR = /usr/local/bin

CFLAGS += -DNDEBUG -Os
CFLAGS += -g -Wall
$(O)libunuc2.o $(O)libunuc2.so: CFLAGS += -std=c99
$(O)unuc2.o: CFLAGS += -std=gnu99 -DVERSION=$(VERSION)

.PHONY: all
all: $(O)unuc2$(EXE)

$(O)libunuc2.a: $(O)libunuc2.o $(O)super.o
	$(AR) rs $@ $^

$(O)libunuc2.so: libunuc2.c $(O)super.o
	$(CC) -fpic -shared $(CFLAGS) $^ $(LDFLAGS) -o $@

$(O)libunuc2.o: libunuc2.c Makefile list.h libunuc2.h $(DEPS)
	$(CC) -c $(CFLAGS) $< -o $@

$(O)super.o: super.S super.bin
	$(CC) -c $(CFLAGS) $< -o $@

$(O)unuc2.o: unuc2.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(O)unuc2$(EXE): $(O)unuc2.o $(O)libunuc2.a
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: install
install: $(O)unuc2$(EXE)
	install -s $^ $(BINDIR)

.PHONY: clean
clean:
	$(RM) $(O)unuc2$(EXE) $(O)unuc2.o $(O)libunuc2.a $(O)libunuc2.so $(O)libunuc2.o $(O)super.o
	@$(CLEAN)

PKG = $(NAME)-$(VERSION)

FILES = Makefile libunuc2.h libunuc2.c list.h super.bin super.S unuc2.c

FILES += win.mk unuc2.rc cli.ico
FILES += misc/mc.ext misc/uuc2 misc/unuc2-wcx.c misc/unuc2-wcx.mk
FILES += $(addprefix compat/,Makefile compat.c \
 include/unistd.h include/err.h include/fnmatch.h include/sys/types.h include/sys/stat.h include/sys/utime.h)

.PHONY: dist
dist:
	tar czf $(PKG).tar.gz -h --group=root --owner=root --transform 's!^!$(PKG)/!' $(FILES)
