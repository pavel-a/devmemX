/**
 * Module for physical memory access like in devmem
 *  
* This is for use in all memory-poking utilities. 
*   pa01 21-sep-2015 
 */

#ifndef _memaccess_h_inc
#define _memaccess_h_inc

#include <stdlib.h>
#include <stdint.h> 
 
typedef uint64_t mem_phys_address_t;
typedef size_t   mem_mapping_size_t;
typedef void *   mem_mapping_hnd;

#ifdef __cplusplus
extern "C" {
#endif 
 
int mem_set_debug(int flags, FILE *output);

int mem_init(void);

int mem_get_version(void);

mem_mapping_hnd mem_create_mapping(mem_phys_address_t target, mem_mapping_size_t size, unsigned flags);

enum mem_mapping_flags {
 MF_ABSOLUTE = 0x1,
 MF_READONLY = 0x2,
 MF_PHYSICAL = 0x4, /* set for physical address, clear for 0-based offsets */
};

char *mem_get_ptr(mem_mapping_hnd mapping, mem_phys_address_t a_from, mem_mapping_size_t size);

int mem_translate_address(mem_mapping_hnd mapping, mem_phys_address_t a_from, mem_phys_address_t *a_to);

int mem_mapping_close(mem_mapping_hnd);

int mem_finalize(void);

#ifdef __cplusplus
}
#endif 

#endif /* _memaccess_h_inc*/
