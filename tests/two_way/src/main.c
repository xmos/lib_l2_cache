// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <platform.h> // for PLATFORM_REFERENCE_MHZ
#include <xcore/hwtimer.h>
#include <xcore/thread.h>
#include <xscope.h>
#include <xcore/minicache.h>

#include "app_common.h"
#include "benchmark_data.h"
#include "flash_handler.h"
#include "l2_cache.h"
#include "debug_print.h"

#define L2_CACHE_STACK_WORDS_TWO_WAY       (1000)

#define L2_CACHE_SETUP         l2_cache_setup_two_way
#define L2_CACHE_BUFFER_SIZE   L2_CACHE_BUFFER_WORDS_TWO_WAY
#define SWMEM_THREAD           l2_cache_two_way
#define SWMEM_STACK_WORDS      L2_CACHE_STACK_WORDS_TWO_WAY

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
    const l2_cache_two_way_addr_dbg_t dbg_info,
    const unsigned timing)
{

  if(element_value != index) {
    debug_printf("  Iteration: %u\n", iter);
    debug_printf("  Element Value:    data[%u] = 0x%08X (%d)\n", index, (unsigned) element_value,
                                                                             element_value );
    debug_printf("  Element Address: &data[%u] = 0x%08X\n", index, element_address );
    debug_printf("  Entry Index: %u\n", dbg_info.entry_index );
    debug_printf("  Tag: 0x%08X\n", dbg_info.tag );
    debug_printf("  Slot Offset: 0x%08X (%u)\n", dbg_info.slot_offset, dbg_info.slot_offset );
    debug_printf("  [%s]\n", dbg_info.is_hit? "Hit" : "Miss" );

    if( dbg_info.is_hit ){
      debug_printf("    Slot: %u\n", dbg_info.hit.slot );
    } else {
      debug_printf("    Evict Slot: %u\n", dbg_info.miss.evict_slot );
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
  if( dbg_info.is_hit )
    assert(timing < flash_read_threshold);
  else
    assert(timing > flash_read_threshold);

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

  l2_cache_two_way_addr_dbg_t dbg_info;
  unsigned timing;

  // First, go through the benchmark data array sequentially and verify that
  // every element is read to be what it is supposed to be.
  debug_printf("Sequential lookup...\n");
  for (int i = 0; i < data_array_len; i++) {
    unsigned index = i;
    const unsigned element_address = (unsigned) &data[index];
    dbg_info = l2_cache_two_way_get_addr_info(&data[index]);
    timing = get_reference_time();
    int element = data[index];
    timing = get_reference_time() - timing;
    check_element(i, index, element_address, element, dbg_info, timing);
  }

  // Then, go through the benchmark data randomly and make sure things keep coming
  // out correctly
  debug_printf("Random lookup...\n");

  srand(0);
  for (int i = 0; i < data_array_len; i++) {
    unsigned index = ((unsigned)rand()) % ((unsigned)data_array_len);
    const unsigned element_address = (unsigned) &data[index];
    dbg_info = l2_cache_two_way_get_addr_info(&data[index]);
    timing = get_reference_time();
    int element = data[index];
    timing = get_reference_time() - timing;
    check_element(i, index, element_address, element, dbg_info, timing);
  }

  // Finally, pick 3 elements that should collide in the cache, and make sure they
  // behave correctly.
  debug_printf("Collision test...\n");

  // The L2 cache line size multiplied by the number of cache lines is the spacing between addresses
  // that should collide in the cache.
  const unsigned collision_spacing_words = (L2_CACHE_LINE_COUNT * L2_CACHE_LINE_SIZE_BYTES) / sizeof(int);

  const unsigned elm_start = 0;

  int indexA = elm_start + 0 * collision_spacing_words;
  int indexB = elm_start + 1 * collision_spacing_words;
  int indexC = elm_start + 2 * collision_spacing_words;

  volatile int* itemA = (int*) &data_array[indexA];
  volatile int* itemB = (int*) &data_array[indexB];
  volatile int* itemC = (int*) &data_array[indexC];

  uint32_t tagA = l2_cache_two_way_get_addr_info((void*)itemA).tag;
  uint32_t tagB = l2_cache_two_way_get_addr_info((void*)itemB).tag;
  uint32_t tagC = l2_cache_two_way_get_addr_info((void*)itemC).tag;

  // If the cache is so large that itemC isn't in swmem_data_array, print a warning
  if( data_array_len <= elm_start + 2 * collision_spacing_words )
    debug_printf("WARNING: itemC outside of data_array[]\n");

  // Verify that we're not wrong about these items colliding...
  const unsigned entry_index = l2_cache_two_way_get_addr_info((void*)itemA).entry_index;
  assert( entry_index == l2_cache_two_way_get_addr_info((void*)itemB).entry_index );
  assert( entry_index == l2_cache_two_way_get_addr_info((void*)itemC).entry_index );

  // Get pointers to the tags and last_hit for the entry
  unsigned cache_entry = ((unsigned)l2_cache_buffer)
          + entry_index * (4 * sizeof(int) + 2 * (L2_CACHE_LINE_SIZE_BYTES));

  volatile uint32_t* tag = (uint32_t*) (cache_entry);
  volatile unsigned* last_hit = (unsigned*) (cache_entry + 2 * sizeof(int));

  // Make sure the values are what we expect...
  assert( *itemA == indexA );
  assert( *itemB == indexB );
  assert( *itemC == indexC );

  // Empty the minicache  // TODO:  WHY DOESN"T THIS ACTUALLY WORK???
  asm volatile("flush");
#define FLUSH_MINICACHE  minicache_invalidate()

  // Use this to flush since the flush instruction doesn't work.
//   unsigned entry2_start = (L2_CACHE_LINE_SIZE_BYTES/sizeof(int));
// #define FLUSH_MINICACHE   do {for(int k = 1; k <= 8; k++) assert(-1 != data_array[entry2_start + 8*k]);} while(0)

  FLUSH_MINICACHE;

  // Pollute the cache entry so that we can control what goes where when.
  tag[0] = 0xFFFFFFFF;
  tag[1] = 0xFFFFFFFF;
  *last_hit = 1;

  // Here goes..
  assert( *last_hit == 1        );
  assert( tag[0] == 0xFFFFFFFF  );
  assert( tag[1] == 0xFFFFFFFF  );
  FLUSH_MINICACHE;
  assert( *itemA == indexA      );
  assert( *last_hit == 0        );
  assert( tag[0] == tagA        );
  assert( tag[1] == 0xFFFFFFFF  );
  FLUSH_MINICACHE;
  assert( *itemB == indexB      );
  assert( *last_hit == 1        );
  assert( tag[0] == tagA        );
  assert( tag[1] == tagB        );
  FLUSH_MINICACHE;
  assert( *itemA == indexA      );
  assert( *last_hit == 0        );
  assert( tag[0] == tagA        );
  assert( tag[1] == tagB        );
  FLUSH_MINICACHE;
  assert( *itemA == indexA      );
  assert( *last_hit == 0        );
  assert( tag[0] == tagA        );
  assert( tag[1] == tagB        );
  FLUSH_MINICACHE;
  assert( *itemB == indexB      );
  assert( *last_hit == 1        );
  assert( tag[0] == tagA        );
  assert( tag[1] == tagB        );
  FLUSH_MINICACHE;
  assert( *itemA == indexA      );
  assert( *last_hit == 0        );
  FLUSH_MINICACHE;
  assert( *itemC == indexC      );
  assert( *last_hit == 1        );
  assert( tag[0] == tagA        );
  assert( tag[1] == tagC        );

// If L2_CACHE_DEBUG_ON is enabled, then also check this hit/miss stats
#if L2_CACHE_DEBUG_ON
  debug_printf("Debug test...\n");
  l2_cache_debug_stats_reset();

  assert(l2_cache_debug_stats.fill_request_count == 0);
  assert(l2_cache_debug_stats.hit_count          == 0);
  assert(l2_cache_debug_stats.miss_count         == 0);

  FLUSH_MINICACHE;

  assert(l2_cache_debug_stats.fill_request_count == 8);
  assert(l2_cache_debug_stats.hit_count          == 8);
  assert(l2_cache_debug_stats.miss_count         == 0);

  assert( data_array[1024] == 1024 );

  assert(l2_cache_debug_stats.fill_request_count == 9);
  assert(l2_cache_debug_stats.hit_count          == 8);
  assert(l2_cache_debug_stats.miss_count         == 1);

  assert( data_array[1025] == 1025 );

  assert(l2_cache_debug_stats.fill_request_count == 9);
  assert(l2_cache_debug_stats.hit_count          == 8);
  assert(l2_cache_debug_stats.miss_count         == 1);

  assert( data_array[1032] == 1032 );

  assert(l2_cache_debug_stats.fill_request_count == 10);
  assert(l2_cache_debug_stats.hit_count          == 9);
  assert(l2_cache_debug_stats.miss_count         == 1);

#endif

  debug_printf("SUCCESS\n\n");

}
