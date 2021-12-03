// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef L2_CACHE_CONFIG_CHECKS_H_
#define L2_CACHE_CONFIG_CHECKS_H_

#if (L2_CACHE_LINE_SIZE_LOG2 > 30)
#error L2_CACHE_LINE_SIZE_LOG2 can be at most 30!
#endif

#if (L2_CACHE_LINE_SIZE_LOG2 < 6)
#error L2_CACHE_LINE_SIZE_LOG2 must be at least 6!
#endif

#endif /* L2_CACHE_CONFIG_CHECKS_H_ */
