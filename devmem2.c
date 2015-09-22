/*
 * devmem.c: Simple program to read/write from/to any location in memory.
 *
 *  Copyright (C) 2000, Jan-Derk Bakker (jdb@lartmaker.nl)
 *
 *
 * This software has been developed for the LART computing board
 * (http://www.lart.tudelft.nl/). The development has been sponsored by
 * the Mobile MultiMedia Communications (http://www.mmc.tudelft.nl/)
 * and Ubiquitous Communications (http://www.ubicom.tudelft.nl/)
 * projects.
 *
 *   Copyright (C) 2015, Trego Ltd.
 *   
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <inttypes.h>

#define printerr(fmt,...) do { fprintf(stderr, fmt, ## __VA_ARGS__); fflush(stderr); } while(0)

#define VER_STR "devmem version T/C (http://git.io/vZ5iD) rev.0.91x"

// This rebase thing is here to support shell scripts written for other system where the addresses are different.
// To avoid rewriting old scripts or doing math in sh (brrr)  you can now set env. variables for old and new base address.
// Old base ($DEVMEMREBASE) will be subtracted and new base ($DEVMEMBASE) will be added.
// Or even simpler, use only offsets (0-based) and set the base in environment. We'll do the math for you.

#define ENV_MBASE   "DEVMEMBASE"
#define ENV_REBASE  "DEVMEMREBASE"
#define ENV_PARAMS  "DEVMEMPARAMS"

// Global flags
int f_readback = 0;       // flag to read back after write
int f_align_check = 1;    // flag to require alignment
int f_absolute = 0;       // legacy mode
int f_dbg = 0;
uint64_t mbase = UINT64_C(-1); // offset to add to the address parameter
uint64_t mrebase = 0;     // address to rebase from (subtract this from given address)

// TODO define the (real) limits to access with this program. 
// Knowing the real limits will also let know whether the addresses are 32 or 64-bit.
//#define ENV_ABSSTART "DEVMEMWIN"
//#define ENV_ABSEND   "DEVMEMWINEND"
//uint64_t mabsstart = ~0U; // phys window start
//uint64_t mabssize = ~0U;  // phys window size

static void get_env_params(int fPrint);

static void usage(const char *cmd)
{
    fprintf(stderr, "\nUsage:\t%s [-switches] address [ type [ data ] ]\n"
        "\taddress : memory address to act upon\n"
        "\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\n"
        "\tdata    : data to be written\n\n"
        "Switches:\n"
        "\t-r      : read back after write\n"
        "\t-a      : do not check alignment\n"
        "\t-A      : \"old\" mode - using absolute addresses\n"
        "\t--version | -V : print version\n"
        "\t-d      : debug spew on (use it to test the address rebasing)\n"
        "\n",
        cmd);

    get_env_params(1);
}

int main(int argc, char **argv)
{
    int fd;
    void *map_base, *virt_addr;
    unsigned long read_result = -1, writeval=-1;
    uint64_t target;
    int access_type = 'w';
    int access_size = 4;
    unsigned int pagesize = (unsigned)getpagesize(); /* or sysconf(_SC_PAGESIZE)  */
    unsigned int map_size = pagesize;
    unsigned offset;
    char *endp = NULL;
    const char *progname = argv[0];
    int opt;

    opterr = 0;
    while ((opt = getopt(argc, argv, "+raAdV")) != -1) {
        switch(opt) {
        case 'r':
            f_readback = 1;
            break;
        case 'a':
            f_align_check = 0;
            break;
        case 'A':    
            f_absolute = 1;
            //? f_align_check = 0;
            break;
        case 'd':
            f_dbg = 1;
            break;
        case 'V':    
            printf(VER_STR "\n");
            exit(0);
        default:   
            if ( (!argv[optind]) || 0 == strcmp(argv[optind], "--help")) {
                usage(progname);
                exit(1);
            }
            if (0 == strncmp(argv[optind], "--vers", 6)) {
                printf(VER_STR "\n");
                exit(0);
            }
            printerr("Unknown long option: %s\n", argv[optind]);
            exit(2);
        }
    }

    if (optind >= argc) {
        usage(progname);
        exit(1);
    }
    
    argc -= optind - 1;
    argv += optind - 1;
    
    get_env_params(0);
    
    if (argc > 2) {
        if (!isdigit(argv[1][0])) {
            // Allow access_type be 1st arg, then swap 1st and 2nd
            char *t = argv[2];
            argv[2] = argv[1];
            argv[1] = t;
        }

        access_type = tolower(argv[2][0]);
        if (argv[2][1] )
            access_type = '?';
    }

    switch (access_type) {
        case 'b':
            access_size = 1;
            break;
        case 'w':
            access_size = 4;
            break;
        case 'h':
            access_size = 2;
            break;
        default:
            printerr("Illegal data type: %s\n", argv[2]);
            exit(2);
    }

    errno = 0;
    target = strtoull(argv[1], &endp, 0);
    if (errno != 0 || (endp && 0 != *endp)) {
        printerr("Invalid memory address: %s\n", argv[1]);
        exit(2);
    }

    if ( ! f_absolute ) {
        // Subtract old offset and add new offset:
        uint64_t t2;
        if (target >= mrebase) {
            target -= mrebase;
            // TODO check upper limit ??
            
            t2 = target + mbase;
            
            if (f_dbg) {
                printerr("Address rebased: %#" PRIX64 "->%#" PRIX64 "\n", target, t2);
            }
            if (t2 < target) {
                printerr("ERROR: addr rollover after rebase! (64-bit)\n");
                exit(2);
            }
            
            target = t2;
        }
        // ELSE... target < rebase addr....  Warn or pass??
    }
    
    if ((target + access_size -1) < target) {
        printerr("ERROR: rolling over end of memory\n");
        exit(2);
    }

    if ( (sizeof(off_t) < sizeof(int64_t)) && (target > UINT32_MAX) ) {
        printerr("The address %#" PRIX64 " > 32 bits. Try to rebuild in 64-bit mode.\n", target);
        // Consider mmap2() instead of this check
        exit(2);
    }

    offset = (unsigned int)(target & (pagesize-1));
    if (offset + access_size > pagesize ) {
        // Access straddles page boundary:  add another page:
        map_size += pagesize;
    }

    if (f_dbg) {
        printerr("Address: %#" PRIX64 " op.size=%d\n", target, access_size);
    }

    if (f_align_check && offset & (access_size - 1)) {
        printerr("ERROR: address not aligned on %d!\n", access_size);
        exit(2);
    }

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printerr("Error opening /dev/mem (%d) : %s\n", errno, strerror(errno));
        exit(1);
    }
    //printf("/dev/mem opened.\n");
    //fflush(stdout);

    map_base = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 
                    target & ~((typeof(target))pagesize-1));
    if (map_base == (void *) -1) {
        printerr("Error mapping (%d) : %s\n", errno, strerror(errno));
        exit(1);
    }
    //printf("Memory mapped at address %p.\n", map_base);
    //fflush(stdout);

    virt_addr = map_base + offset;

    if (argc > 3) {
        errno = 0;
        writeval = strtoul(argv[3], &endp, 0);
        if (errno || (endp && 0 != *endp)) {
            printerr("Invalid data value: %s\n", argv[3]);
            exit(2);
        }

        if (access_size < sizeof(writeval) && 0 != (writeval >> (access_size * 8))) {
            printerr("ERROR: Data value %s does not fit in %d byte(s)\n", argv[3], access_size);
            exit(2);
        }

        switch (access_size) {
            case 1:
                *((volatile uint8_t *) virt_addr) = writeval;
                break;
            case 2:
                *((volatile uint16_t *) virt_addr) = writeval;
                break;
            case 4:
                *((volatile uint32_t *) virt_addr) = writeval;
                break;
        }

        if (f_dbg) {
            printerr("Address: %#" PRIX64 " Written: %#lX\n", target, writeval);
            fflush(stdout);
        }
    }
    
    if (argc <= 3 || f_readback) {
        switch (access_size) {
            case 1:
                read_result = *((volatile uint8_t *) virt_addr);
                break;
            case 2:
                read_result = *((volatile uint16_t *) virt_addr);
                break;
            case 4:
                read_result = *((volatile uint32_t *) virt_addr);
                break;
        }

        //printf("Value at address 0x%lld (%p): 0x%lu\n", (long long)target, virt_addr, read_result);
        //fflush(stdout);
        if (f_readback && argc > 3)
            printf("Written 0x%lx; readback 0x%lx\n", writeval, read_result);
        else
            printf("%08lX\n", read_result);
        fflush(stdout);
    }

    if (munmap(map_base, map_size) != 0) {
        printerr("ERROR munmap (%d) %s\n", errno, strerror(errno));
    }

    close(fd);

    if (f_dbg)
        printerr("done\n");

    return 0;
}

static void get_env_params(int a_print)
{
    char *p;
    uint64_t v64;
    char *endp = NULL;
    int errors = 0;
    int fPrint = f_dbg || a_print;
    
    if (fPrint)
        printf("\n\nEnvironment parameters:\n");
    
    p = getenv(ENV_PARAMS);
    if (p) {
        if(fPrint)
            printf("%s = \"%s\" (all ignored!)\n", ENV_PARAMS, p);
        
        // TODO parse params... can put here debug, alignment...
    }
    //else if (fPrint)
    //    printf("%s not set\n", ENV_PARAMS);
        
    p = getenv(ENV_MBASE);
    if (p) {
        errno = 0;
        v64 = strtoull(p, &endp, 16);
        if (errno != 0 || (endp && 0 != *endp)) {
            printerr("Error in %s value: %s\n", ENV_MBASE, p);
            errors++;
        }
        else {
            mbase = v64;
            if (fPrint)
                printf("%s = %#" PRIX64 "\n", ENV_MBASE, mbase );
        }
    }
    else if (fPrint)
        printf("%s not set\n", ENV_MBASE);
    
    p = getenv(ENV_REBASE);
    if (p) {
        errno = 0;
        v64 = strtoull(p, &endp, 16);
        if (errno != 0 || (endp && 0 != *endp)) {
            printerr("Error in %s value: %s\n", ENV_REBASE, p);
            errors++;
        }
        else {
            mrebase = v64;
            if (fPrint)
                printf("%s = %#" PRIX64 "\n", ENV_REBASE, mrebase );
        }
    }
    else if (fPrint)
        printf("%s not set\n", ENV_REBASE);
#if 0
    p = getenv(ENV_MSIZE);
    if (p) {
        errno = 0;
        v64 = strtoull(p, &endp, 16);
        if (errno != 0 || (endp && 0 != *endp)) {
            printerr("Error in %s value: %s\n", ENV_MSIZE, p);
            errors++;
        }
        else {
            msize = (uint32_t)v64;
            if (fPrint)
                printf("%s = %#" PRIX32 "\n", ENV_MSIZE, msize );
        }
    }
    else if (fPrint)
        printf("%s not set\n", ENV_MSIZE);

    p = getenv(ENV_RESIZE);
    if (p) {
        errno = 0;
        v64 = strtoull(p, &endp, 16);
        if (errno != 0 || (endp && 0 != *endp)) {
            printerr("Error in %s value: %s\n", ENV_RESIZE, p);
            errors++;
        }
        else {
            mresize = (uint32_t)v64;
            if (fPrint)
                printf("%s = %#" PRIX32 "\n", ENV_RESIZE, mresize );
        }
    }
    else if (fPrint)
        printf("%s not set\n", ENV_RESIZE);
#endif    
    if ( !a_print ) {
        if (errors) {
            printerr("Errors found in environment parameters\n");
            exit(2);
        }
        
        if ( !f_absolute && (mbase == UINT64_C(-1))) {
            printerr("Env. parameter %s must be set, or use absolute mode switch -A\n", 
               ENV_MBASE);
            exit(2);
        }
    }
}
