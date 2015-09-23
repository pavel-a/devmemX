
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

CC ?= "gcc"

CFLAGS ?= -O2 -g -Wall

SRC=devmemx.c memaccess.c

devmemx: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $(SRC)

# Install as "devmem2"
install2: devmemx
	mkdir -p $(BINDIR)
	install -s -D devmem $(BINDIR)/devmem2

# Rename to "mem" :  we like it this way.
install: devmemx
	install -s -D devmemx $(BINDIR)/mem
	
clean:
	rm -f devmemx *.o *~
