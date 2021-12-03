// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef L2_CACHE_H_
#define L2_CACHE_H_

#include <stdint.h>
#include <stddef.h>
#include <xcore/swmem_fill.h>

#include "l2_cache_default_config.h"

#if L2_CACHE_DEBUG_ON
#include "l2_cache_debug.h"
#endif /* L2_CACHE_DEBUG_ON */

#define L2_CACHE_BUFFER_WORDS_DIRECT_MAP(LINE_COUNT, LINE_SIZE_BYTES)       \
            (LINE_COUNT * (((LINE_SIZE_BYTES) + sizeof(int)))/sizeof(int))

#define L2_CACHE_BUFFER_WORDS_TWO_WAY(LINE_COUNT, LINE_SIZE_BYTES)          \
            (LINE_COUNT * 2*(((LINE_SIZE_BYTES) + 2*sizeof(int)))/sizeof(int))

#define L2_CACHE_SWMEM_READ_FN  __attribute__((fptrgroup("l2_cache_swmem_read_fptr_grp")))
typedef void (*l2_cache_swmem_read_fn)(void*, const void*, const size_t);

#define L2_CACHE_SETUP_FN_ATTR  __attribute__((fptrgroup("l2_cache_setup_fptr_grp")))
typedef void (*l2_cache_setup_fn)(const unsigned, const unsigned, void*, l2_cache_swmem_read_fn);

#define L2_CACHE_THREAD_FN_ATTR  __attribute__((fptrgroup("l2_cache_thread_fptr_grp")))
typedef void (*l2_cache_thread_fn)(void*);

/**
 * Initialize for two-way set associative read-only L2 cache.
 */

void l2_cache_setup_two_way(
    const unsigned line_count,
    const unsigned line_size_bytes,
    void* cache_buffer,
    l2_cache_swmem_read_fn read_func);

/**
 * Initialize for direct-mapped L2 read-only cache.
 */
void l2_cache_setup_direct_map(
    const unsigned line_count,
    const unsigned line_size_bytes,
    void* cache_buffer,
    l2_cache_swmem_read_fn read_func);

/**
 * Direct-mapped L2 read-only cache.
 *
 * Line size and entry count is configurable.
 */
void l2_cache_direct_map(void*);

/**
 * Two-way set associative read-only L2 cache.
 *
 * Entry count is configurable.
 */
void l2_cache_two_way(void*);


/// The following are basically for debugging purposes, but must be visible when L2_CACHE_DEBUG_ON is
/// not enabled because they're required to test for correctness.

typedef struct {
  void* flash_address; // flash address
  void* fill_request_address; // flash address masked to 32-byte alignment
  void* cache_address; // address at which data should be found (after access, before eviction)

  unsigned tag;
  unsigned entry_index;
  unsigned slot_offset;
  unsigned is_hit;

  struct {
    unsigned tag[2];
    unsigned last_hit;
    int* slot[2];
  } entry;

  struct {
    unsigned slot;
  } hit;

  struct {
    unsigned evict_slot;
    void* flash_src;
    void* cache_dst;
    unsigned bytes;
  } miss;
} l2_cache_two_way_addr_dbg_t;

l2_cache_two_way_addr_dbg_t l2_cache_two_way_get_addr_info(
    const void* address);

typedef struct {
  void* flash_address; // flash address
  void* fill_request_address; // flash address masked to 32-byte alignment
  void* cache_address; // address at which data should be found (after access, before eviction)

  unsigned tag;
  unsigned entry_index;
  unsigned entry_offset;
  unsigned is_hit;

  struct {
    unsigned tag;
    int* slot;
  } entry;

  struct {
    void* flash_src;
    void* cache_dst;
    unsigned bytes;
  } miss;
} l2_cache_direct_map_addr_dbg_t;

l2_cache_direct_map_addr_dbg_t l2_cache_direct_map_get_addr_info(
    const void* address);

#endif // L2_CACHE_H_
