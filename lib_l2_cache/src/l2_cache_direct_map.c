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

// Tag table initialized to this to signal that the tag is dirty
#define DIRTY_TAG_VALUE  (0xFFFFFFFF)

static inline unsigned zext(const unsigned value, const unsigned bits)
{
    unsigned mask = (1<<bits)-1;
    return value & mask;
}

/**
 * This struct is allocated in l2_cache.S.  Because there can only be one SwMem handler,
 * there's no reason not to make the cache config global.
 */
extern struct {
    swmem_fill_t swmem_fill_handle; /// resource handle for SwMem fills
    unsigned index_bits;      /// log2() of the number of L2 cache entries
    void* data_table;         /// data table
    int* tag_table;           /// tag table
    L2_CACHE_SWMEM_READ_FN
    l2_cache_swmem_read_fn read_func;  /// function which populates the data table on a cache miss
    uint32_t offset_mask;     /// mask to extract the data table offset from a fill address
    unsigned line_size_bytes; /// Size of an L2 cache line in bytes
    unsigned line_size;       /// log2() of line_size_bytes
} l2_cache_config_direct_map;

#define l2_cache_config l2_cache_config_direct_map

L2_CACHE_SETUP_FN_ATTR
void l2_cache_setup_direct_map(
    const unsigned line_count,
    const unsigned line_size_bytes,
    void* cache_buffer,
    l2_cache_swmem_read_fn read_func)
{
    int* tag_table = cache_buffer;
    void* data_table = &((int*)cache_buffer)[line_count];

    const unsigned cache_index_bits = 31 - clz(line_count);
    const unsigned line_bits = 31 - clz(line_size_bytes);

    uint32_t offset_mask = (1 << (line_bits + cache_index_bits)) - 1;

    DEBUG_ASSERT( line_size_bytes >= 32 ); // minimum line size is 32 bytes
    DEBUG_ASSERT( (1<<line_bits) == line_size_bytes); // line_size_bytes is a power of 2
    DEBUG_ASSERT( (1<<cache_index_bits) == line_count ); // line_count is a power of 2
    DEBUG_ASSERT( (((unsigned)data_table) & 0x3) == 0); // data_table is word-aligned
    DEBUG_ASSERT( (((unsigned)tag_table) & 0x1) == 0); // tag_table is short-aligned

    l2_cache_config.swmem_fill_handle = swmem_fill_get();
    l2_cache_config.index_bits = cache_index_bits;
    l2_cache_config.data_table = data_table;
    l2_cache_config.tag_table = tag_table;
    l2_cache_config.read_func = read_func;
    l2_cache_config.offset_mask = offset_mask;
    l2_cache_config.line_size_bytes = line_size_bytes;
    l2_cache_config.line_size = line_bits;

    #if L2_CACHE_DEBUG_ON
        // First two address bits for SwMem are always 01  (0x40000000 - 0x7FFFFFFF), so
        // they needn't be included in the tag.

        const unsigned data_table_end = ((unsigned)data_table) + line_count*line_size_bytes - 1;
        const unsigned tag_table_end = ((unsigned)tag_table) + line_count*sizeof(uint16_t) - 1;

        DEBUG_PRINT("%s","Cache Type: Direct Mapped (read-only)\n");
        DEBUG_PRINT("SwMem Fill Handle: %u\n", l2_cache_config.swmem_fill_handle);
        DEBUG_PRINT("Line Size:   %u bytes (%u LSb's)\n", l2_cache_config.line_size_bytes,
                                                                l2_cache_config.line_size);
        DEBUG_PRINT("Index Bits:  %u\n", l2_cache_config.index_bits);
        DEBUG_PRINT("Data Table:  0x%08X - 0x%08X\n", (unsigned) l2_cache_config.data_table, data_table_end);
        DEBUG_PRINT("Tag Table:   0x%08X - 0x%08X\n", (unsigned) l2_cache_config.tag_table, tag_table_end);
        DEBUG_PRINT("Read Func:   0x%08X\n", (unsigned) read_func);
        DEBUG_PRINT("Offset Mask: 0x%08X\n", (unsigned) l2_cache_config.offset_mask);
        DEBUG_PRINT("Cache Size: %u B\n", line_count * line_size_bytes);
        DEBUG_PRINT("Tag Table Size: %u B\n", line_count * sizeof(int));
    #endif // L2_CACHE_DEBUG_ON

    for(int k = 0; k < line_count; k++) {
        l2_cache_config.tag_table[k] = DIRTY_TAG_VALUE;
    }

}


#if L2_CACHE_DEBUG_ON
void l2_cache_direct_map_debug(
    const void* fill_address,
    const int* tag_table,
    const int* data_table)
{
    // DEBUG_PRINT("Rx Fill Request: 0x%08X\n", (unsigned) fill_address);
}
#endif


// Really for debugging purposes, but can't be hidden by L2_CACHE_DEBUG_ON because it's needed
// for testing for correct behavior

l2_cache_direct_map_addr_dbg_t l2_cache_direct_map_get_addr_info(
    const void* address)
{
    unsigned addr = (unsigned) address;

    l2_cache_direct_map_addr_dbg_t x;
    x.flash_address = (void*) address;
    x.fill_request_address = (void*) (addr & 0xFFFFFFE0);

    x.entry_offset = zext(addr, l2_cache_config.line_size);
    addr >>= l2_cache_config.line_size;
    x.entry_index = zext(addr, l2_cache_config.index_bits);
    addr >>= l2_cache_config.index_bits;
    x.tag = addr;
    x.is_hit = 0;

    x.entry.tag = l2_cache_config.tag_table[x.entry_index];
    x.entry.slot = (int*) (((unsigned)l2_cache_config.data_table)
                        + (x.entry_index * l2_cache_config.line_size_bytes));

    x.is_hit = (x.tag == x.entry.tag);

    if( !x.is_hit ) {
        x.miss.flash_src = (void*) (((unsigned)address) & ~(l2_cache_config.line_size_bytes-1));
        x.miss.cache_dst = (void*) ((unsigned)x.entry.slot);
        x.miss.bytes = l2_cache_config.line_size_bytes;
    }

    x.cache_address = (void*) (((unsigned) &x.entry.slot) + x.entry_offset);

    return x;
}
