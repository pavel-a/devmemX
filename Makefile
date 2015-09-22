
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

CC ?= "gcc"

CFLAGS ?= -O2 -g -Wall

SRC=devmem2.c

devmem2: $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -o $@ $(SRC)

# Install as "devmem2"
install:
	mkdir -p $(BINDIR)
	install devmem2 $(BINDIR) 

# Rename to "mem" :  we like it this way.
installmem:
	install -s -D devmem2 $(BINDIR)/mem
	
clean:
	rm -f devmem2 *.o *~
