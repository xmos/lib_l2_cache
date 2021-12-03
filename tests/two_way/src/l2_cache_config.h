// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef L2_CACHE_CONFIG_H_
#define L2_CACHE_CONFIG_H_

#define ENABLE_L2_CACHE   (1)

#define L2_CACHE_LINE_SIZE_LOG2  (8)
#define L2_CACHE_LINE_COUNT      (64)

#ifndef L2_CACHE_DEBUG_ON
#define L2_CACHE_DEBUG_ON  (0)
#endif//L2_CACHE_DEBUG_ON

#ifndef FLASH_DEBUG_ON
#define FLASH_DEBUG_ON     (0)
#endif//FLASH_DEBUG_ON

#endif // L2_CACHE_CONFIG_
