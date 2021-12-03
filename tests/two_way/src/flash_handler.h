// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef FLASH_HANDLER_H_
#define FLASH_HANDLER_H_

#include "l2_cache.h"

#ifndef FLASH_PAGE_SIZE_BYTES_LOG2
#define FLASH_PAGE_SIZE_BYTES_LOG2  (8)
#endif

#define FLASH_PAGE_SIZE_BYTES (1<<FLASH_PAGE_SIZE_BYTES_LOG2)

/**
 * Perform a flash read
 *
 * \param dst_addr  Pointer to the buffer to read data into
 * \param src_addr  The byte address in the flash to begin reading at
 * \param len       The number of bytes to read
 */
L2_CACHE_SWMEM_READ_FN
void flash_read_bytes(
    void* dst_addr,
    const void* src_addr,
    const size_t len);

/**
 * Initialize flash access
 */
void flash_setup(void);

#if FLASH_DEBUG_ON

typedef struct {
    uint32_t read_count;
    uint32_t read_time;
} flash_dbg_data_t;

extern flash_dbg_data_t flash_dbg_data;

static inline void flash_dbg_data_reset()
{
    flash_dbg_data.read_count = 0;
    flash_dbg_data.read_time = 0;
}

#if L2_CACHE_DEBUG_FLOAT_ON
static inline float flash_dbg_read_time_avg_us() { return flash_dbg_data.read_time /  (100.0f * flash_dbg_data.read_count); }
static inline float flash_dbg_read_time_total_us() { return flash_dbg_data.read_time / 100.0f; }
#else
static inline uint32_t flash_dbg_read_time_avg_us()
{
    return flash_dbg_data.read_count > 0 ? (flash_dbg_data.read_time / (100 * flash_dbg_data.read_count)) : 0;
}
static inline uint32_t flash_dbg_read_time_total_us() { return flash_dbg_data.read_time / 100; }
#endif /* L2_CACHE_DEBUG_FLOAT_ON */

#endif /* FLASH_DEBUG_ON */

#endif /* FLASH_HANDLER_H_ */
