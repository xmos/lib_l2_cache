// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef L2_CACHE_DEBUG_H_
#define L2_CACHE_DEBUG_H_

#if L2_CACHE_DEBUG_ON
#include <stdint.h>

extern struct {
    volatile uint32_t fill_request_count;
    volatile uint32_t hit_count;
    volatile uint32_t miss_count;
} l2_cache_debug_stats;

static inline void l2_cache_debug_stats_reset(void)
{
    l2_cache_debug_stats.fill_request_count = 0;
    l2_cache_debug_stats.hit_count = 0;
    l2_cache_debug_stats.miss_count = 0;
}

static inline uint32_t get_fill_request_count(void)
{
    return l2_cache_debug_stats.fill_request_count;
}

static inline uint32_t get_hit_count(void)
{
    return l2_cache_debug_stats.hit_count;
}

static inline uint32_t get_miss_count(void)
{
    return l2_cache_debug_stats.miss_count;
}

#if L2_CACHE_DEBUG_FLOAT_ON
static inline float l2_cache_debug_hit_rate(void)
{
    return ((float)l2_cache_debug_stats.hit_count)/l2_cache_debug_stats.fill_request_count;
}
#else
static inline uint32_t l2_cache_debug_hit_rate(void)
{
    return (uint32_t)((uint64_t)(l2_cache_debug_stats.hit_count*100)/l2_cache_debug_stats.fill_request_count);
}
#endif /* L2_CACHE_DEBUG_FLOAT_ON */

#endif /* L2_CACHE_DEBUG_ON */

#endif /* L2_CACHE_DEBUG_H_ */
