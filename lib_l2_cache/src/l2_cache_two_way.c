// Copyright 2020-2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <assert.h>
#include <stdio.h>

#include <xs1.h>
#include <xclib.h>
#include <xcore/swmem_fill.h>
#include <xcore/hwtimer.h>

#include "l2_cache.h"
#include "xcore_utils.h"


// =============== Debugging Stuff =============== //
#define DEBUG_PRINT(FMT, ...) do { if(L2_CACHE_DEBUG_ON) debug_printf( "[L2 Cache] "FMT, __VA_ARGS__); } while(0)
#define DEBUG_ASSERT( CONDITION ) do{ if(L2_CACHE_DEBUG_ON) assert( CONDITION ); } while(0)

#define PRINT_ADDR(VAR)   debug_printf("------ 0x%08X\n", (unsigned) &(VAR))


// =============== Misc =============== //

// Tag table initialized to this to signal that the tag is dirty
#define DIRTY_TAG_VALUE  (0xFFFFFFFF)

// This N-way associative cache has N fixed at 2.
#define N_WAY     (2)

static inline unsigned zext(const unsigned value, const unsigned bits)
{
    unsigned mask = (1<<bits)-1;
    return value & mask;
}

/*

Memory layout is a table:

  Index || Tag[0] | Tag[1] | Last | Dummy | Data[0] | Data[1]
  -----------------------------------------------------------
     0  ||  ...   |  ...   | ...  |  ...  |   ...   |   ...
     1  ||  ...   |  ...   | ...  |  ...  |   ...   |   ...
    ... ||  ...   |  ...   | ...  |  ...  |   ...   |   ...

  Index: Row of the table above (see fill address bits below)
         (Note: this isn't actually *in* the table)
  Tag[X]: The tag associated with Data[X]  (1 word each)
  Last: Indicates whether 0/1 had the most recent hit (1 word)
  Dummy: [does nothing, but required to ensure 8-byte alignment] (1 word)
  Data[X]: The actual cached data (256 bytes)

  Notes:
    - Tag[0] and Tag[1] are next to each other so that `ldd` can be used when checking for a hit
      - This is also why the dummy field exists, and why the buffer must be 8-byte-aligned
      - If I find that the `ldd` instruction doesn't actually speed anything up, those can be removed

==============================================



// line_count = 64; line_size_bytes = 256 bytes
//  TTTT TTTT TTTT TTTT TTCC CCCC LLL0 0000
//
// L2 cache lines = 128; L2 cache line size = 256 bytes
//  TTTT TTTT TTTT TTTT TCCC CCCC LLL0 0000
//
// L2 cache lines = 64; L2 cache line size = 64 bytes
//  TTTT TTTT TTTT TTTT TTTT CCCC CCL0 0000
//
// etc

 T:  Tag bits
 C:  Cache line index (index into the table above)
 L:  Fill line index (indicates the 32-byte group within a 256-byte L2 cache line)

 Notes:
  - The actual tag in a cache entry will be the tag bits above (T) zero-extended to 32 bits
  - Minimum number of index bits (C) is 1.
    - There's no theoretical problem with 0 index bits, which would basically mean that the
      L2 cache is a thin layer over the minicache; however, where the `zext(reg, bits)`
      instruction treats `zext(reg, 1)` as `reg <-- reg & 0x00000001` and `zext(reg, 2)` as
      `reg <-- reg & 0x00000003`,  `zext(reg, 0)` is NOT treated as `reg <-- reg & 0x00000000`,
      but instead it is treated as `reg <-- reg`, which is the same as `zext(reg, 32)`.
    - So, supporting 0 index bits would require either a conditioned branch in the critical
      path (for hits and misses), or a second whole implementation of the loop
  - Whatever the L2 cache line size (line_size_bytes), the L1 cache always wants a 32-byte fill.
    The fill line index (L) with the 5 zeros appended to the end (a total of log2(line_size_bytes)
    bits) are the byte offset (into a cache line's data) for the requested fill.

*/


// =============== Types =============== //

typedef uint32_t tag_t;

typedef union {
    uint8_t u8[32];
    uint32_t u32[8];
} l1_cache_line_t;

typedef union {
    l1_cache_line_t line[(L2_CACHE_LINE_SIZE_BYTES)/32];
} l2_cache_line_t;

typedef struct {
    tag_t tag[N_WAY];
    uint32_t last_hit;
    uint32_t dummy;
    l2_cache_line_t slot[N_WAY];
} l2_cache_entry_t;


extern struct {
    swmem_fill_t swmem_fill_handle; /// resource handle for SwMem fills
    l2_cache_entry_t* entries;
    unsigned index_bits;      /// log2() of the number of L2 cache entries
    L2_CACHE_SWMEM_READ_FN
    l2_cache_swmem_read_fn read_func;  /// function which populates the data table on a cache miss
    struct {
        unsigned bytes; /// Size of an L2 cache line in bytes
        unsigned bits;  /// log2() of line_size.bytes
    } line_size;
    unsigned entry_bytes;
} l2_cache_config_two_way;

#define cache_config l2_cache_config_two_way

L2_CACHE_SETUP_FN_ATTR
void l2_cache_setup_two_way(
    const unsigned line_count,
    const unsigned line_size_bytes,
    void* cache_buffer,
    l2_cache_swmem_read_fn read_func)
{
    const unsigned cache_index_bits = 31 - clz(line_count);
    const unsigned line_bits = 31 - clz(line_size_bytes);

    // bytes
    const unsigned entry_size = N_WAY * (line_size_bytes + sizeof(tag_t) + sizeof(uint32_t));

    DEBUG_ASSERT( line_size_bytes >= 32 ); // minimum line size is 32 bytes
    DEBUG_ASSERT( (1<<line_bits) == line_size_bytes); // line_size_bytes is a power of 2
    DEBUG_ASSERT( (1<<cache_index_bits) == line_count ); // line_count is a power of 2
    DEBUG_ASSERT( (((unsigned)cache_buffer) & 0x7) == 0); // buffer is 8-byte-aligned

    cache_config.swmem_fill_handle = swmem_fill_get();
    cache_config.entries = (l2_cache_entry_t*) cache_buffer;

    cache_config.index_bits = cache_index_bits;
    cache_config.read_func = read_func;
    cache_config.line_size.bytes = line_size_bytes;
    cache_config.line_size.bits = line_bits;
    cache_config.entry_bytes = entry_size;

    #if L2_CACHE_DEBUG_ON
        // bytes
        const unsigned cache_size = entry_size * line_count;

        const unsigned buffer_end = ((unsigned)cache_buffer) + cache_size - 1;

        DEBUG_PRINT("%s","Cache Type: 2-Way Set Associative (read-only)\n");
        DEBUG_PRINT("SwMem Fill Handle: %u\n", cache_config.swmem_fill_handle);
        DEBUG_PRINT("Line Size:   %u bytes (%u LSb's)\n", cache_config.line_size.bytes,
                                                          cache_config.line_size.bits);
        DEBUG_PRINT("Index Bits:  %u\n", cache_config.index_bits);
        DEBUG_PRINT("Buffer:      0x%08X - 0x%08X\n", (unsigned) cache_config.entries, buffer_end);
        DEBUG_PRINT("Read Func:   0x%08X\n", (unsigned) read_func);
        DEBUG_PRINT("Cache Size: %u B\n", line_count * 2*line_size_bytes);
        DEBUG_PRINT("Cache Entry Size: %u B\n", cache_config.entry_bytes);
    #endif // L2_CACHE_DEBUG_ON

    for(int k = 0; k < line_count; k++){
        for(int a = 0; a < N_WAY; a++) {
            cache_config.entries[k].tag[a] = DIRTY_TAG_VALUE;
        }

        cache_config.entries[k].dummy = 0;
        cache_config.entries[k].last_hit = 0;
    }
}


// Really for debugging purposes, but can't be hidden by L2_CACHE_DEBUG_ON because it's needed
// for testing for correct behavior

l2_cache_two_way_addr_dbg_t l2_cache_two_way_get_addr_info(
    const void* address)
{
    unsigned addr = (unsigned) address;

    l2_cache_two_way_addr_dbg_t x;
    x.flash_address = (void*) address;
    x.fill_request_address = (void*) (addr & 0xFFFFFFE0);

    x.slot_offset = zext(addr, cache_config.line_size.bits);
    addr >>= cache_config.line_size.bits;
    x.entry_index = zext(addr, cache_config.index_bits);
    addr >>= cache_config.index_bits;
    x.tag = addr;
    x.is_hit = 0;

    unsigned slot;

    l2_cache_entry_t* entry = &cache_config.entries[x.entry_index];
    x.entry.last_hit = entry->last_hit;

    for(int k = 0; k < 2; k++) {
        x.entry.tag[k] = entry->tag[k];
        x.entry.slot[k] = (int*) &entry->slot[k];

        if(x.tag == entry->tag[k]) {
            x.hit.slot = k;
            x.is_hit = 1;
            slot = x.hit.slot;
        }
    }

    if( !x.is_hit ) {
        x.miss.evict_slot = 1 - entry->last_hit;
        x.miss.flash_src = (void*) (((unsigned)address) & ~(cache_config.line_size.bytes-1));
        x.miss.cache_dst = (void*) ((unsigned)x.entry.slot[x.miss.evict_slot]);
        x.miss.bytes = cache_config.line_size.bytes;
        slot = x.miss.evict_slot;
    }

    x.cache_address = (void*) (((unsigned) &x.entry.slot[slot]) + x.slot_offset);

    return x;
}
