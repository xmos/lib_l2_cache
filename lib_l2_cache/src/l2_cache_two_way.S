// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#if defined(__XS3A__)

#include "xs1.h"
#include "l2_cache_default_config.h"

/*
  Two-way set associative read-only L2 cache.

void l2_cache_two_way(void*);

==============================================

Memory layout is a table:

  Index || TagA | TagB | Last | Dummy | DataA | DataB
  ---------------------------------------------------
     0  || ...  | ...  | ...  |  ...  |  ...  |  ...
     1  || ...  | ...  | ...  |  ...  |  ...  |  ...
    ... || ...  | ...  | ...  |  ...  |  ...  |  ...

  Index: Row of the table above (see fill address bits below)
         (Note: this isn't actually *in* the table)
  TagA/B: The tag associated with DataA/B  (1 word each)
  Last: Indicates whether A/B had the most recent hit (1 word)
  Dummy: [does nothing, but required to ensure 8-byte alignment] (1 word)
  DataA/B: The actual cached data (size is configurable)

==============================================

Fill Address bits:  TTTT TTTT TTTT TTTT TTCC CCCC LLL0 0000
 T:  Tag bits
 C:  Cache line index (index into the table above)
 L:  Fill line index (indicates the 32-byte group within a 256-byte L2 cache line)

==============================================



*/

#define DEBUG_ON  0


#define NSTACKVECTS     (0)
#define NSTACKWORDS     (10 + 8*(NSTACKVECTS))

#define FUNCTION_NAME   l2_cache_two_way

#define tmpA        r0
#define tmpB        r1
#define index_bits  r2
#define tag         r3
#define fill_addr   r4
#define cache_dex   r5
#define slot_offset r6
#define entry       r7
#define entry_bytes r8
#define line_bits   r9
#define _16         r10
#define swmem       r11

.section .dp.data, "awd", @progbits

#define DP_FILL_HANDLE    0
#define DP_ENTRY_TABLE    1
#define DP_INDEX_BITS     2
#define DP_READ_FUNC      3
#define DP_LINE_BYTES     4
#define DP_LINE_BITS      5
#define DP_ENTRY_BYTES    6


l2_cache_config_two_way:
  .L_fill_handle: .word 0
  .L_entry_table: .word 0
  .L_index_bits:  .word 0
  .L_read_func:   .word 0
  .L_line_bytes:  .word 0
  .L_line_bits:   .word 0
  .L_entry_bytes: .word 0

.global l2_cache_config_two_way

.text
.issue_mode dual
.align 16

.cc_top FUNCTION_NAME.function,FUNCTION_NAME

FUNCTION_NAME:
    dualentsp NSTACKWORDS
  // Never returns, so no need to save any registers

    ldap r11, l2_cache_config_two_way
    set dp, r11

    ldw swmem, dp[DP_FILL_HANDLE]
    ldw line_bits, dp[DP_LINE_BITS]
    ldw index_bits, dp[DP_INDEX_BITS]
    ldw entry_bytes, dp[DP_ENTRY_BYTES]
    ldc _16, 16


  .L_loop_top:

    // Preload entry with the address of the entry table.
      ldw entry, dp[DP_ENTRY_TABLE]

    // Get fill address
    { in fill_addr, res[swmem]              ; mkmsk tmpA, line_bits                 }

    // Get the data offset
    { shr cache_dex, fill_addr, line_bits   ; and slot_offset, fill_addr, tmpA      }

    // Get the cache line index and the tag
    { shr tag, cache_dex, index_bits        ; zext cache_dex, index_bits            }

    // Find the correct table entry  (NOTE: tmpA doesn't matter here, even if result could overflow (it can't))
      maccu tmpA, entry, cache_dex, entry_bytes

    // Check for a hit
      ldd tmpB, tmpA, entry[0]
    { eq tmpA, tmpA, tag                    ; eq tmpB, tmpB, tag                    }
    { add slot_offset, slot_offset, _16     ; bt tmpA, .L_cache_hit0                }
    { ldc tmpA, 1                           ; bt tmpB, .L_cache_hit1                }
    {                                       ; bu .L_cache_miss                      }

    .align 16
    .L_cache_hit0:
#if L2_CACHE_DEBUG_ON
      ldap r11, l2_cache_debug_stats
      mov _16, r11
      ldw swmem, _16[0]
      add swmem, swmem, 1
      stw swmem, _16[0]
      ldw swmem, _16[1]
      add swmem, swmem, 1
      stw swmem, _16[1]
      ldw swmem, dp[DP_FILL_HANDLE]
      ldc _16, 16
#endif // L2_CACHE_DEBUG_ON
      { add entry, entry, slot_offset         ; stw tmpB, entry[2]                    }
      {                                       ; vldd entry[0]                         }
      { setc res[swmem], XS1_SETC_RUN_STARTR  ; vstd fill_addr[0]                     }
      {                                       ; bu .L_loop_top                        }

    .align 16
    .L_cache_hit1:
#if L2_CACHE_DEBUG_ON
      ldap r11, l2_cache_debug_stats
      mov _16, r11
      ldw swmem, _16[0]
      add swmem, swmem, 1
      stw swmem, _16[0]
      ldw swmem, _16[1]
      add swmem, swmem, 1
      stw swmem, _16[1]
      ldw swmem, dp[DP_FILL_HANDLE]
      ldc _16, 16
#endif // L2_CACHE_DEBUG_ON
        ldw tmpB, dp[DP_LINE_BYTES]
      { add slot_offset, entry, slot_offset   ;                                       }
      { add entry, slot_offset, tmpB          ; stw tmpA, entry[2]                    }
      {                                       ; vldd entry[0]                         }
      { setc res[swmem], XS1_SETC_RUN_STARTR  ; vstd fill_addr[0]                     }
      {                                       ; bu .L_loop_top                        }


    .L_cache_miss:
#if L2_CACHE_DEBUG_ON
      ldap r11, l2_cache_debug_stats
      mov _16, r11
      ldw swmem, _16[0]
      add swmem, swmem, 1
      stw swmem, _16[0]
      ldw swmem, _16[2]
      add swmem, swmem, 1
      stw swmem, _16[2]
      ldw swmem, dp[DP_FILL_HANDLE]
      ldc _16, 16
#endif // L2_CACHE_DEBUG_ON
      //// It was a miss. Figure out what to evict and fetch new data

      // Get the last hit from the entry. We'll fill the other slot.
      { ldc tmpB, 1                           ; ldw tmpA, entry[2]                    }
      { sub tmpA, tmpB, tmpA                  ; mkmsk tmpB, line_bits                 }

      // Update last_hit and tag
      { not tmpB, tmpB                        ; stw tmpA, entry[2]                    }
        stw tag, entry[tmpA]
      { sub slot_offset, slot_offset, _16     ; ldw r2, dp[DP_LINE_BYTES]             } //third arg to flash_read_bytes
      { add entry, entry, _16                 ; bf tmpA, .L_miss_slot0                }
      .L_miss_slot1:
        // Move over to second slot
        { add entry, entry, r2                  ;                                       }
      .L_miss_slot0:

      // Call read function    void foo(void* dst, void* src, unsigned)
      { and tmpB, fill_addr, tmpB             ; ldw tmpA, dp[DP_READ_FUNC]             }
        ldap r11, _dp
        set dp, r11 // gotta set dp to point to the right place..
        mov r11, tmpA
      { mov tmpA, entry                       ; bla r11                               }

        ldap r11, l2_cache_config_two_way
        set dp, r11


      // Fix index_bits and swmem which was clobbered
      // Add slot offset
      { add entry, entry, slot_offset         ; ldw index_bits, dp[DP_INDEX_BITS]     }
      {                                       ; ldw swmem, dp[DP_FILL_HANDLE]         }

      // We've updated the cache with the new data, now go fill the SwMem request
      {                                       ; vldd entry[0]                         }
      { setc res[swmem], XS1_SETC_RUN_STARTR  ; vstd fill_addr[0]                     }
      {                                       ; bu .L_loop_top                        }






  // Function never returns


.L_func_end:
.cc_bottom FUNCTION_NAME.function


.global FUNCTION_NAME
.type FUNCTION_NAME,@function
.weak _fptrgroup.l2_cache_swmem_read_fptr_grp.nstackwords.group
.max_reduce read_fn.nstackwords, _fptrgroup.l2_cache_swmem_read_fptr_grp.nstackwords.group, 0

.set FUNCTION_NAME.nstackwords,NSTACKWORDS + read_fn.nstackwords;
    .global FUNCTION_NAME.nstackwords
.set FUNCTION_NAME.maxcores,1;                  .global FUNCTION_NAME.maxcores
.set FUNCTION_NAME.maxtimers,0;                 .global FUNCTION_NAME.maxtimers
.set FUNCTION_NAME.maxchanends,0;               .global FUNCTION_NAME.maxchanends
.size FUNCTION_NAME, .L_func_end - FUNCTION_NAME

#endif //defined(__XS3A__)
