// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#if defined(__XS3A__)

#include "xs1.h"
#include "l2_cache_default_config.h"

#if L2_CACHE_DEBUG_ON

.section .dp.data, "awd", @progbits

l2_cache_debug_stats:
  .L_total_request_count: .word 0
  .L_hit_count:           .word 0
  .L_miss_count:          .word 0
.global l2_cache_debug_stats

#endif // L2_CACHE_DEBUG_ON

#endif //defined(__XS3A__)
