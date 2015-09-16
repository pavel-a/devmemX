/*
 * devmem2.c: Simple program to read/write from/to any location in memory.
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
#include <stdint.h>

#define printerr(fmt,...) do { fprintf(stderr, fmt, ## __VA_ARGS__); fflush(stderr); } while(0)

int main(int argc, char **argv)
{
    int fd;
    void *map_base, *virt_addr;
    unsigned long read_result = -1, writeval;
    uint64_t target;
    int access_type = 'w';
    int access_size = 4;
    unsigned int pagesize = (unsigned)getpagesize(); /* or sysconf(_SC_PAGESIZE)  */
    unsigned int map_size = pagesize;
    unsigned offset;
    char *endp = NULL;
    int readback = 0; // flag to read back after write

    if (argc < 2) {
        fprintf(stderr, "\nUsage:\t%s { address } [ type [ data ] ]\n"
            "\taddress : memory address to act upon\n"
            "\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\n"
            "\tdata    : data to be written\n\n",
            argv[0]);
        exit(1);
    }

    if (strcasestr(argv[1], "-v")) {
        printf("devmem version T/C x.1\n");
        exit(0);
    }

    errno = 0;
    target = strtoull(argv[1], &endp, 0);
    if (errno != 0 || (endp && 0 != *endp)) {
        printerr("Invalid memory address: %s\n", argv[1]);
        exit(2);
    }

    if (argc > 2) {
        access_type = tolower(argv[2][0]);
        if (argv[2][1] )
            access_type = '?';
    }

    switch(access_type) {
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

    if ((target + access_size -1) < target) {
        printerr("ERROR: rolling over end of memory\n");
        exit(2);
    }

    if ( (sizeof(off_t) < sizeof(int64_t)) && (target > UINT32_MAX) ) {
        printerr("The address %s is too large. Try to rebuild in 64-bit mode.\n", argv[1]);
        // consider mmap2() instead of this check
        exit(2);
    }

    offset = (unsigned int)(target & (pagesize-1));
    if (offset + access_size > pagesize ) {
        // Access straddles page boundary:  add another page:
        map_size += pagesize;
    }

    if (offset & (access_size - 1)) {
        printerr("WARNING: address not aligned on %d!\n", access_size);
        //exit(2);
    }

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printerr("Error opening /dev/mem (%d) : %s\n", errno, strerror(errno));
        exit(1);
    }
    //printf("/dev/mem opened.\n");
    //fflush(stdout);

    map_base = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
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

        switch (access_type) {
            case 'b':
                *((uint8_t *) virt_addr) = writeval;
                if (readback)
                    read_result = *((uint8_t *) virt_addr);
                break;
            case 'h':
                *((uint16_t *) virt_addr) = writeval;
                if (readback)
                    read_result = *((uint16_t *) virt_addr);
                break;
            case 'w':
                *((uint32_t *) virt_addr) = writeval;
                if (readback)
                    read_result = *((uint32_t *) virt_addr);
                break;
        }

        if (readback)
            printf("Written 0x%lu; readback 0x%lu\n", writeval, read_result);
        //else
        //    printf("Written 0x%lu\n", writeval);
        //fflush(stdout);
    }
    else
    {
        switch (access_type) {
            case 'b':
                read_result = *((uint8_t *) virt_addr);
                break;
            case 'h':
                read_result = *((uint16_t *) virt_addr);
                break;
            case 'w':
                read_result = *((uint32_t *) virt_addr);
                break;
        }
        //printf("Value at address 0x%lld (%p): 0x%lu\n", (long long)target, virt_addr, read_result);
        //fflush(stdout);
        printf("%08lX\n", read_result);
        fflush(stdout);
    }

    if (munmap(map_base, map_size) != 0) {
        printerr("ERROR munmap (%d) %s\n", errno, strerror(errno));
    }

    close(fd);

    return 0;
}
