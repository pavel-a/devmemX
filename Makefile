
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

CC ?= "gcc"

CFLAGS ?= -O2 -g -Wall
# _GNU_SOURCE for various glibc extensions
CFLAGS += -D_GNU_SOURCE

SRC=devmem2.c

devmem2: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $(SRC)

install:
	mkdir -p $(BINDIR)
	install devmem2 $(BINDIR)

clean:
	rm -f devmem2 *.o *~
