// Copyright 2020-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#if defined(__XS3A__)

#include "xs1.h"
#include "l2_cache_default_config.h"

/*

Direct-mapped read-only L2 cache

void l2_cache_direct_map(void*);

*/


#define NSTACKVECTS     (0)
#define NSTACKWORDS     (10 + 8*(NSTACKVECTS))

#define FUNCTION_NAME   l2_cache_direct_map

#define tmpA        r0  // r0-r4 may get overwritten when flash_read_bytes() is called
#define tmpB        r1  // so keep stuff we don't super need in them.
#define old_tag     r2  // (unfortunately data_table we will need after the call)
#define tag         r3  //
#define fill_addr   r4
#define offset_mask r5
#define cache_dex   r6
#define swmem       r7
#define tmpC        r8
#define data_table  r9
#define tag_table   r10


.section .dp.data, "awd", @progbits

l2_cache_config_direct_map:
  .L_fill_handle: .word 0
  .L_index_bits:  .word 0
  .L_data_table:  .word 0
  .L_tag_table:   .word 0
  .L_read_func:   .word 0
  .L_offset_mask: .word 0
  .L_line_bytes:  .word 0
  .L_line_size:   .word 0

.global l2_cache_config_direct_map

.text
.issue_mode dual
.align 16

.cc_top FUNCTION_NAME.function,FUNCTION_NAME


// Fill Address bits:  01TT TTTT TTTT TTTT TTCC CCCC LLL0 0000
//  T:  Tag bits
//  C:  Cache line index (index into data_table and tag_table)
//  L:  Fill line index (indicates the 32-byte group within a 256-byte L2 cache line)

FUNCTION_NAME:
    dualentsp NSTACKWORDS
  // Never returns, so no need to save any registers

    ldap r11, _dp
    set dp, r11

    ldw offset_mask, dp[.L_offset_mask]
    ldw tag_table, dp[.L_tag_table]
    ldw data_table, dp[.L_data_table]
    ldw swmem, dp[.L_fill_handle]
    mkmsk tmpB, 32

  .L_loop_top:

    {                                       ; ldw r11, dp[.L_line_size]             }
    { mkmsk tmpB, 32                        ; ldw tmpA, dp[.L_index_bits]           }

    // Wait for the next fill address
    { in fill_addr, res[swmem]              ; shl tmpB, tmpB, r11                   }

  #if L2_CACHE_DEBUG_ON
    { mov r0, fill_addr                     ; mov r1, tag_table                     }
    { mov r2, data_table                    ;                                       }
      ldap r11, l2_cache_direct_map_debug
      bla r11
      ldw tmpA, dp[.L_index_bits]
      ldw r11, dp[.L_line_size]
      mkmsk tmpB, 32
      shl tmpB, tmpB, r11
  #endif // L2_CACHE_DEBUG_ON

    // Bottom 14 (6+3+5) bits are offset into the data table. bottom 5 bits are useless otherwise
    { shr cache_dex, fill_addr, r11         ; and r11, fill_addr, offset_mask       }

    // Next bits are the cache index (index into the tag_table and data_table)
    { shr tag, cache_dex, tmpA              ; zext cache_dex, tmpA                  }

    // Calculate address of fill in data table; Get the old tag to compare
    { add tmpC, data_table, r11             ; ldw old_tag, tag_table[cache_dex]     }

    // Compare old tag with current tag; load the data to be filled (in case of hit)
    { eq old_tag, tag, old_tag              ; vldd tmpC[0]                          }

#if L2_CACHE_DEBUG_ON
      ldaw data_table, dp[l2_cache_debug_stats]
      ldw swmem, data_table[0]
      add swmem, swmem, 1
      stw swmem, data_table[0]
      bf old_tag, .L_dbg_cache_miss
    .L_dbg_cache_hit:
      ldw swmem, data_table[1]
      add swmem, swmem, 1
      stw swmem, data_table[1]
      bu .L_dbg_end
    .L_dbg_cache_miss:
      ldw swmem, data_table[2]
      add swmem, swmem, 1
      stw swmem, data_table[2]
    .L_dbg_end:
      ldw swmem, dp[.L_fill_handle]
      ldw data_table, dp[.L_data_table]
#endif // L2_CACHE_DEBUG_ON

    // If old_tag == tag, we just need to fill the swmem line. Otherwise, we need to
    // actually load the new data into the L2 cache
    { and r11, r11, tmpB                    ; bt old_tag, .L_cache_hit              }
    .L_cache_miss:
      // Overwrite tag table value
        stw tag, tag_table[cache_dex]
      { add r0, data_table, r11               ; and r1, fill_addr, tmpB               }
        ldw r2, dp[.L_line_bytes]
        ldw r11, dp[.L_read_func]
        bla r11
        vldd tmpC[0]

    .L_cache_hit:

    // tmpC should now point to the correct data with which to fill the request
    // use the VPU to quickly move the data over
    { setc res[swmem], XS1_SETC_RUN_STARTR  ; vstd fill_addr[0]                     }
      bu .L_loop_top

  // Function never returns


.L_func_end:
.cc_bottom FUNCTION_NAME.function


.global FUNCTION_NAME
.type FUNCTION_NAME,@function
.weak _fptrgroup.l2_cache_swmem_read_fptr_grp.nstackwords.group
.max_reduce read_fn.nstackwords, _fptrgroup.l2_cache_swmem_read_fptr_grp.nstackwords.group, 0

#if L2_CACHE_DEBUG_ON
.add_to_set l2c_dm.children, read_fn.nstackwords
.add_to_set l2c_dm.children, l2_cache_direct_map_debug.nstackwords
.max_reduce l2c_dm.children.nstackwords, l2c_dm.children, 0

.set FUNCTION_NAME.nstackwords,NSTACKWORDS + l2c_dm.children.nstackwords;
#else
.set FUNCTION_NAME.nstackwords,NSTACKWORDS + read_fn.nstackwords;
#endif // L2_CACHE_DEBUG_ON
                                                .global FUNCTION_NAME.nstackwords
.set FUNCTION_NAME.maxcores,1;                  .global FUNCTION_NAME.maxcores
.set FUNCTION_NAME.maxtimers,0;                 .global FUNCTION_NAME.maxtimers
.set FUNCTION_NAME.maxchanends,0;               .global FUNCTION_NAME.maxchanends
.size FUNCTION_NAME, .L_func_end - FUNCTION_NAME

#endif //defined(__XS3A__)
