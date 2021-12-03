// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef L2_CACHE_DEFAULT_CONFIG_H_
#define L2_CACHE_DEFAULT_CONFIG_H_

/* Allow the application to override any defaults */
#if defined(L2_CACHE_USER_CONFIG_FILE)
#include L2_CACHE_CONFIG_FILE
#endif

/**
 * The line size log2 for the L2 cache.
 *
 * This determines the number of bytes read from flash for each cache miss
 *
 * NOTE: Must be at least 6
 * NOTE: For the two-way set-associative cache, this is actually the slot size
 */
#ifndef L2_CACHE_LINE_SIZE_LOG2
#define L2_CACHE_LINE_SIZE_LOG2   (8)
#endif

/**
 * The line size for the L2 cache.
 *
 * This is the number of bytes read from flash for each cache miss
 *
 * NOTE: Must be a power of 2
 * NOTE: 32 <= L2_CACHE_LINE_SIZE_BYTES
 * NOTE: For the two-way set-associative cache, this is actually the slot size
 */
#define L2_CACHE_LINE_SIZE_BYTES  (1 << L2_CACHE_LINE_SIZE_LOG2)

/**
 * Number of L2 cache lines.
 *
 * NOTE: This affects the direct-map and two-way set-associative caches differently.
 *       The two-way set-associative cache has two slots for each line
 */
#ifndef L2_CACHE_LINE_COUNT
#define L2_CACHE_LINE_COUNT       (64)
#endif

/**
 * Flags to enable debug
 */
#ifndef L2_CACHE_DEBUG_ON
#define L2_CACHE_DEBUG_ON  (0)
#endif /* L2_CACHE_DEBUG_ON */

#ifndef L2_CACHE_DEBUG_FLOAT_ON
#define L2_CACHE_DEBUG_FLOAT_ON  (0)
#endif /* L2_CACHE_DEBUG_FLOAT_ON */

#ifndef FLASH_DEBUG_ON
#define FLASH_DEBUG_ON     (0)
#endif /* FLASH_DEBUG_ON */

#ifndef PRINT_TIMING_INFO
/* NOTE: The SwMem timing is invalid if L2 cache debug is enabled, so
 *       there's no point in outputing it, including for SRAM
 */
#define PRINT_TIMING_INFO   (!(L2_CACHE_DEBUG_ON))
#endif

/* Verify Config */
#include "l2_cache_config_checks.h"

#endif /* L2_CACHE_DEFAULT_CONFIG_H_ */
