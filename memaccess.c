/**
 * Module for physical memory access like in devmem
 *  
* This is for use in all memory-poking utilities. 
*   pa01 21-sep-2015 
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
#include <assert.h>

#include "memaccess.h" /* self */

#define _MEMACCESS_LIB_VER_MJ 0
#define _MEMACCESS_LIB_VER_MN 95

// This rebase thing is here to support shell scripts written for other system where the addresses are different.
// To avoid rewriting old scripts or doing math in sh (brrr)  you can now set env. variables for old and new base address.
// Old base ($DEVMEMREBASE) will be subtracted and new base ($DEVMEMBASE) will be added.
// Or even simpler, use only offsets (0-based) and set the base in environment. We'll do the math for you.

#define ENV_MBASE   "DEVMEMBASE"
#define ENV_REBASE  "DEVMEMREBASE"
#define ENV_PARAMS  "DEVMEMOPT"

// TODO define the (real) limits to access with this program. 
// Knowing the real limits will also let know whether the addresses are 32 or 64-bit.
//#define ENV_ABSSTART "DEVMEMWIN"
//#define ENV_ABSEND   "DEVMEMWINEND"
//uint64_t mabsstart = ~0U; // phys window start
//uint64_t mabssize = ~0U;  // phys window size

static int f_dbg = 0;
static FILE *dbgf = NULL;
static unsigned int pagesize;
static uint64_t mbase = UINT64_C(-1); // offset to add to the address parameter
static uint64_t mrebase = 0;     // address to rebase from (subtract this from given address)

struct mapping_s {
    int fd;          // 0 when not initialized
    mem_phys_address_t phys_addr;
    void *mmap_base;
    off_t mmap_size;
    int abs_mode;
    int offs_mode;
};

static struct mapping_s g_map; //  a singleton, duh. /me doesn't know any other patterns.

#define printerr(fmt,...) while(dbgf){ fprintf(dbgf, fmt, ## __VA_ARGS__); fflush(dbgf); break; } 

static int get_env_params(void);

// Rebase address (a phys address or offset)
static mem_phys_address_t  _transl_phys_addr(mem_phys_address_t a_from)
{
    mem_phys_address_t t2 = a_from;
    if (a_from >= mrebase) {
        t2 -= mrebase;
        t2 += mbase;
    }
    // else ???
    // TODO check upper limit ??
    return t2;
}


mem_mapping_hnd mem_create_mapping(mem_phys_address_t target, mem_mapping_size_t size, unsigned flags)
{
    struct mapping_s *mp = &g_map;
    mem_phys_address_t translated;

    errno = 0;
    if (mp->fd > 0) {
        printerr("Only one mapping supported and it is already in use.\n");
        errno = -ENOTSUP;
        return 0;
    }
    
    // MF_ABSOLUTE flag exists only for devmem 'classic' mode. Other utilities should not need it.
    mp->abs_mode = !!(flags & MF_ABSOLUTE);
    if ( !(flags & MF_ABSOLUTE) && (mbase == UINT64_C(-1))) {
        printerr("Env. parameter %s must be set, or use absolute mode switch -A\n", 
           ENV_MBASE);
        errno = -EINVAL;   
        return 0;
    }

    // MF_PHYSICAL flag exists for utilities that use "physical" addresses. New utilities should pass offsets (0-based) instead.
    if (flags & MF_PHYSICAL) {
        mp->offs_mode = 0;
        translated = target;
    } 
    else {
        translated = target + mbase;
        mp->offs_mode = 1;
        mp->abs_mode = 0;
        if ( translated < target ) {
            printerr("ERROR: rolling over end of memory\n");
            errno = -ERANGE;
            return 0;
        }
    }

    if (!mp->abs_mode) 
        translated = _transl_phys_addr(target);
    
    mp->phys_addr = translated & ~((typeof(translated))pagesize-1);
    mem_mapping_size_t offset = translated - mp->phys_addr;
    mp->mmap_size = (offset + size ) / pagesize + pagesize;
    if ( mp->phys_addr + mp->mmap_size <= mp->phys_addr ) {
        printerr("ERROR: rolling over end of memory\n");
        errno = -ERANGE;
        return 0;
    }

    if ( (sizeof(off_t) < sizeof(int64_t)) && (mp->phys_addr + mp->mmap_size) > UINT32_MAX ) {
        printerr("The address %#" PRIX64 " > 32 bits. Try to rebuild in 64-bit mode.\n", mp->phys_addr);
        // Consider mmap2() instead of this check
        errno = -ERANGE;
        return 0;
    }
    
    mp->fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mp->fd == -1) {
        printerr("Error opening /dev/mem (%d) : %s\n", errno, strerror(errno));
        return 0;
    }
    
    if (f_dbg) {
        printerr("/dev/mem opened.\n");
    }

    mp->mmap_base = mmap(0, mp->mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mp->fd, 
                    mp->phys_addr);
    if (mp->mmap_base == (void *) -1) {
        printerr("Error mapping (%d) : %s\n", errno, strerror(errno));
        exit(1);
    }
    
    if (f_dbg) {
        printerr("Memory mapped at virt address %p.\n", mp->mmap_base);
    }

    return (mem_mapping_hnd)mp;
}

/**
* Translate physical address to real physical address
* Use for debug.
*/
int mem_translate_address(mem_mapping_hnd mapping, mem_phys_address_t a_from, mem_phys_address_t *a_to)
{
    struct mapping_s *mp = (struct mapping_s *)mapping;
    if (!mp || !a_to) return -1;
    *a_to = mp->abs_mode ? a_from : _transl_phys_addr(a_from);
    return 0;
}

/**
* Translate offset to real absolute physical address.
* Use for debug.
*/
int mem_translate_offset(mem_mapping_hnd mapping, mem_mapping_size_t off, mem_phys_address_t *a_to)
{
    struct mapping_s *mp = (struct mapping_s *)mapping;
    if (!mp || !a_to) return -1;
    if (!mp->offs_mode) return -1; // use mem_translate_address()
    *a_to = _transl_phys_addr(mbase + off);
    return 0;
}

/**
 *  Get virtual address (pointer) for a phys address or offset, check that size is within mapping.
 */
char * mem_get_ptr(mem_mapping_hnd mapping, mem_phys_address_t a_from, mem_mapping_size_t size)
{
    struct mapping_s *mp = (struct mapping_s *)mapping;
    if ( !mp->mmap_base )
        return NULL;

    if (mp->offs_mode) {
        a_from += mbase;
    }
    else if (!mp->abs_mode) {
        a_from = _transl_phys_addr(a_from);
    }
    
    if (a_from < mp->phys_addr) {
        printerr("%s : overflow 1\n", __FUNCTION__);
        return 0;
    }
    
    mem_phys_address_t offset = a_from - mp->phys_addr;
    if ( (offset + size < offset) || mp->mmap_size < (offset + size) ) {
        printerr("%s : overflow 2\n", __FUNCTION__);
        return 0;
    }

    return mp->mmap_base + (size_t)offset;
}

int mem_mapping_close(mem_mapping_hnd mapping)
{
    struct mapping_s *mp = (struct mapping_s *)mapping;
    if (!mp) return -1;
    if (munmap(mp->mmap_base, mp->mmap_size) != 0) {
        printerr("ERROR munmap (%d) %s\n", errno, strerror(errno));
    }
    mp->mmap_base = (void*)(intptr_t)-1;
    
    if (mp->fd > 0)
        close(mp->fd);
    mp->fd = -1;    
    return 0;
}

int mem_init(void)
{
    int err = get_env_params();
    if (err)
        return err;
    pagesize = (unsigned)getpagesize(); /* or sysconf(_SC_PAGESIZE)  */
    return 0;
}

int mem_set_debug(int flags, FILE *dbgfile)
{
    f_dbg = !!(flags & 0x1);
    dbgf = dbgfile;
    return 0;
}

int mem_finalize(void)
{
    if (g_map.fd > 0)
        mem_mapping_close(&g_map);
    return 0;
}

int mem_get_version(void)
{
    return (_MEMACCESS_LIB_VER_MJ << 16) + _MEMACCESS_LIB_VER_MN;
}

static int get_env_params(void)
{
    char *p;
    uint64_t v64;
    char *endp = NULL;
    int errors = 0;
    int fPrint = f_dbg;
    
    p = getenv(ENV_PARAMS);
    if (p) {
        if (strstr(p, "+d")) { // Debug
            f_dbg++;
            fPrint++;
            if (!dbgf) {
                dbgf = stderr; // revise?
            }
        }

        if (strstr(p, "-NDM")) { // kill switch
            fprintf(stderr, 
                "ERROR: Env. parameter in %s forbids use of this memory access module\n", ENV_PARAMS);
            return -1;
        }
    }
    //else if (fPrint)
    //    printf("%s not set\n", ENV_PARAMS);
        
    if (fPrint)
        printf("\n\nEnvironment parameters: (v.%u.%u)\n",
            _MEMACCESS_LIB_VER_MJ, _MEMACCESS_LIB_VER_MN);

    if (fPrint && p)    
        printf("%s = \"%s\"\n", ENV_PARAMS, p);

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

    if (errors) {
        if ( fPrint ) {
            printerr("Errors found in environment parameters\n");
        }
        return -1;
    }

    return 0;
}
