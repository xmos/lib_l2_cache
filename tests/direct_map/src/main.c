// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <platform.h> // for PLATFORM_REFERENCE_MHZ
#include <xcore/hwtimer.h>
#include <xcore/thread.h>
#include <xscope.h>

#include "app_common.h"
#include "benchmark_data.h"
#include "flash_handler.h"
#include "l2_cache.h"
#include "debug_print.h"

#define L2_CACHE_STACK_WORDS_DIRECT_MAP    (32)

#define L2_CACHE_SETUP         l2_cache_setup_direct_map
#define L2_CACHE_BUFFER_SIZE   L2_CACHE_BUFFER_WORDS_DIRECT_MAP
#define SWMEM_THREAD           l2_cache_direct_map
#define SWMEM_STACK_WORDS      L2_CACHE_STACK_WORDS_DIRECT_MAP

#define L2_CACHE_BUFFER_ELMS L2_CACHE_BUFFER_SIZE(L2_CACHE_LINE_COUNT, L2_CACHE_LINE_SIZE_BYTES)

DWORD_ALIGNED
static int l2_cache_buffer[L2_CACHE_BUFFER_ELMS];

DWORD_ALIGNED
static int swmem_stack[SWMEM_STACK_WORDS];


// Used for verifying whether hits/misses are treated correctly.
// This will be set to one quarter of the time it takes to read
// a cache line from flash.
unsigned flash_read_threshold = 0;


static void verify_flashed_data()
{
  const unsigned page_words = 64;
  const unsigned page_bytes = sizeof(int) * page_words;
  const unsigned pages = data_array_size / page_bytes;

  int buff[page_words];

  char* flash_addr = (char*) 0x40000000;

  int exp = 0;
  for(int page = 0; page < pages; page++){
    flash_read_bytes(buff, flash_addr, page_bytes);
    for(int k = 0; k < page_words; k++, exp++){
      assert(buff[k] == exp);
    }
    flash_addr = &flash_addr[page_bytes];
  }

  // Also set a value for flash_read_threshold
  char buff2[L2_CACHE_LINE_SIZE_BYTES];
  flash_read_threshold = get_reference_time();
  flash_read_bytes(buff2, (void*) 0x40000000, L2_CACHE_LINE_SIZE_BYTES);
  flash_read_threshold = get_reference_time() - flash_read_threshold;
  flash_read_threshold >>= 2;

  debug_printf("\nVerified flashed data.\n\n");
  debug_printf("flash_read_threshold: %u\n", flash_read_threshold);
}


static void check_element(
    const unsigned iter,
    const unsigned index,
    const unsigned element_address,
    const int element_value,
    const l2_cache_direct_map_addr_dbg_t dbg_info,
    const unsigned timing)
{

  if(element_value != index) {
    debug_printf("  Iteration: %u\n", iter);
    debug_printf("  Element Value:    data[%u] = 0x%08X (%d)\n", index, (unsigned) element_value,
                                                                             element_value );
    debug_printf("  Element Address: &data[%u] = 0x%08X\n", index, element_address );
    debug_printf("  Entry Index: %u\n", dbg_info.entry_index );
    debug_printf("  Tag: 0x%08X\n", dbg_info.tag );
    debug_printf("  Entry Offset: 0x%08X (%u)\n", dbg_info.entry_offset, dbg_info.entry_offset );
    debug_printf("  [%s]\n", dbg_info.is_hit? "Hit" : "Miss" );
    debug_printf("    Old Tag: 0x%08X\n", dbg_info.entry.tag);

    if( !dbg_info.is_hit ){
      debug_printf("    flash_read_bytes( 0x%08X, 0x%08X, %u )\n",
                              (unsigned) dbg_info.miss.cache_dst,
                              (unsigned) dbg_info.miss.flash_src,
                              dbg_info.miss.bytes );
    }

    debug_printf("  Cache Address: 0x%08X\n", (unsigned) &((int*)dbg_info.cache_address)[0]);
    debug_printf("  Cache Value: 0x%08X (%d)\n", (unsigned) ((int*)dbg_info.cache_address)[0],
                                          ((int*)dbg_info.cache_address)[0]);
    debug_printf("\n\n");
  }

  // Assert that value is correct
  assert(element_value == index);

  // Also use the latency info to check whether hits and misses correctly triggered
  // flash reads or not
  unsigned timing_right = dbg_info.is_hit? (timing < flash_read_threshold) : (timing > flash_read_threshold);

  if(!timing_right){

    debug_printf("  Iteration: %u\n", iter);
    debug_printf("  Element Value:    data[%u] = 0x%08X (%d)\n", index, (unsigned) element_value,
                                                                             element_value );
    debug_printf("  Element Address: &data[%u] = 0x%08X\n", index, element_address );
    debug_printf("  Entry Index: %u\n", dbg_info.entry_index );
    debug_printf("  Tag: 0x%08X\n", dbg_info.tag );
    debug_printf("  Entry Offset: 0x%08X (%u)\n", dbg_info.entry_offset, dbg_info.entry_offset );
    debug_printf("  [%s]\n", dbg_info.is_hit? "Hit" : "Miss" );
    debug_printf("    Old Tag: 0x%08X\n", dbg_info.entry.tag);
    debug_printf("  Read Time: %u\n", timing);
    debug_printf("  Flash Read Threshold: %u\n", flash_read_threshold);
    debug_printf("\n\n");
  }

  assert( timing_right );

}


int main(int argc, char *argv[]) {

  // Without xScope enabled, the debug_printf()'s below can interfere with the flash reads
  // (because to do a JTAG-based debug_printf() requires that we basically pause everything
  // that's happening)
  xscope_config_io(XSCOPE_IO_BASIC);

  // Initialize flash driver
  flash_setup();

  // Before using any SwMem stuff, make sure the right data is in flash. This is to
  // avoid headaches.
  verify_flashed_data();


  // Initialize L2 cache
  L2_CACHE_SETUP( L2_CACHE_LINE_COUNT,
                  L2_CACHE_LINE_SIZE_BYTES,
                  l2_cache_buffer,
                  flash_read_bytes  );

  // Start SwMem thread
  run_async(SWMEM_THREAD, NULL, STACK_BASE(swmem_stack, SWMEM_STACK_WORDS));

  //////////// TEST FOR CORRECTNESS ///////////////////
  debug_printf("\n\n");

#define data data_array

  // const unsigned data_base_address = (unsigned) &data[0];

  l2_cache_direct_map_addr_dbg_t dbg_info;
  unsigned timing;

  // First, go through the benchmark data array sequentially and verify that
  // every element is read to be what it is supposed to be.
  debug_printf("Sequential lookup...\n");
  for (int i = 0; i < data_array_len; i++) {
    unsigned index = i;
    const unsigned element_address = (unsigned) &data[index];
    dbg_info = l2_cache_direct_map_get_addr_info(&data[index]);
    timing = get_reference_time();
    int element = data[index];
    timing = get_reference_time() - timing;
    check_element(i, index, element_address, element, dbg_info, timing);
  }


  // Then, go through the benchmark data randomly and make sure things keep coming
  // out correctly
  debug_printf("Random lookup...\n");

  // Because the minicache keeps making this way harder than it needs to be
  //  (and the "flush" instruction just doesn't work at all)
  struct {
    unsigned lines[8];
  } minicache = {{0}};

  srand(0);
  for (int i = 0; i < data_array_len; i++) {
    unsigned index = ((unsigned)rand()) % ((unsigned)data_array_len);
    const unsigned element_address = (unsigned) &data[index];

    //Check if it's in the minicache, and skip it if it is, because things being
    // in the minicache is causing me nightmares.
    unsigned mc_addr = element_address & 0xFFFFFFE0;
    unsigned skip = 0;
    for(int k = 0; k < 8; k++){
      if(minicache.lines[k] == mc_addr){
        skip = 1;
        break;
      }
    }
    if(skip)
      continue;

    //Otherwise, update our minicache model
    memmove(&minicache.lines[0], &minicache.lines[1], 7*sizeof(unsigned));
    minicache.lines[7] = mc_addr;


    dbg_info = l2_cache_direct_map_get_addr_info(&data[index]);
    timing = get_reference_time();
    int element = data[index];
    timing = get_reference_time() - timing;
    check_element(i, index, element_address, element, dbg_info, timing);
  }

// If L2_CACHE_DEBUG_ON is enabled, then also check this hit/miss stats
#if L2_CACHE_DEBUG_ON


  debug_printf("Debug test...\n");
  l2_cache_debug_stats_reset();

  assert(l2_cache_debug_stats.fill_request_count == 0);
  assert(l2_cache_debug_stats.hit_count          == 0);
  assert(l2_cache_debug_stats.miss_count         == 0);

  assert( data_array[1024] == 1024 );

  assert(l2_cache_debug_stats.fill_request_count == 1);
  assert(l2_cache_debug_stats.hit_count          == 0);
  assert(l2_cache_debug_stats.miss_count         == 1);

  assert( data_array[1025] == 1025 );

  assert(l2_cache_debug_stats.fill_request_count == 1);
  assert(l2_cache_debug_stats.hit_count          == 0);
  assert(l2_cache_debug_stats.miss_count         == 1);

  assert( data_array[1032] == 1032 );

  assert(l2_cache_debug_stats.fill_request_count == 2);
  assert(l2_cache_debug_stats.hit_count          == 1);
  assert(l2_cache_debug_stats.miss_count         == 1);

#endif

  debug_printf("SUCCESS\n\n");

}
