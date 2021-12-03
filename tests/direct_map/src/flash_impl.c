// Copyright 2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <stdint.h>
#include <platform.h>
#include <stdio.h>
#include <stdlib.h>

#include <xcore/port.h>

#include "flash_handler.h"
#include "l2_cache.h"

#if FLASH_DEBUG_ON
#include <xcore/hwtimer.h>
#endif /* FLASH_DEBUG_ON */

#define USE_XTC_LIB_QUADSPI 0

#if !USE_XTC_LIB_QUADSPI

#include "qspi_flash.h"

#define PORT_SQI_CS   XS1_PORT_1B
#define PORT_SQI_SCLK XS1_PORT_1C
#define PORT_SQI_SIO  XS1_PORT_4B

qspi_flash_ctx_t qspi_ctx;

void flash_setup(void) {
	/*******************************************/
	/***** Define ports and flash details ******/
	/*******************************************/
    qspi_ctx.custom_clock_setup = 1;
    qspi_ctx.source_clock = qspi_io_source_clock_xcore;

    /* 80 MHz SCLK when the system clock is 800 MHz */
    qspi_ctx.qspi_io_ctx.clock_block = XS1_CLKBLK_1,

    /* 80 MHz SCLK when the system clock is 800 MHz */
    qspi_ctx.qspi_io_ctx.full_speed_clk_divisor       = 5;
    qspi_ctx.qspi_io_ctx.full_speed_sclk_sample_delay = 1,
    qspi_ctx.qspi_io_ctx.full_speed_sclk_sample_edge  = qspi_io_sample_edge_rising;
    qspi_ctx.qspi_io_ctx.full_speed_sio_pad_delay     = 0;

    /* 33.3 MHz SCLK when the system clock is 800 MHz */
    qspi_ctx.qspi_io_ctx.spi_read_clk_divisor       = 12;
    qspi_ctx.qspi_io_ctx.spi_read_sclk_sample_delay = 0;
    qspi_ctx.qspi_io_ctx.spi_read_sclk_sample_edge  = qspi_io_sample_edge_falling;
    qspi_ctx.qspi_io_ctx.spi_read_sio_pad_delay     = 0;

    qspi_ctx.qspi_io_ctx.cs_port   = PORT_SQI_CS;
    qspi_ctx.qspi_io_ctx.sclk_port = PORT_SQI_SCLK;
    qspi_ctx.qspi_io_ctx.sio_port  = PORT_SQI_SIO;
    qspi_ctx.quad_page_program_cmd = qspi_flash_page_program_1_4_4;

    qspi_ctx.address_bytes = 3;
    qspi_ctx.busy_poll_bit = 0;
    qspi_ctx.busy_poll_ready_value = 0;

    /*******************************************/
    /*** Initialize the QSPI flash interface ***/
    /*******************************************/
    qspi_flash_init(&qspi_ctx);
}

L2_CACHE_SWMEM_READ_FN
void flash_read_bytes(
    void* dst_address,
    const void* src_address,
    const unsigned bytes)
{

#if FLASH_DEBUG_ON
    unsigned t1 = get_reference_time();
#endif /* FLASH_DEBUG_ON */

    qspi_flash_read(&qspi_ctx,
                   (uint8_t*) dst_address,
                   (uint32_t) src_address,
                   bytes);

#if FLASH_DEBUG_ON
    unsigned t2 = get_reference_time();
    flash_dbg_data.read_count++;
    flash_dbg_data.read_time += (t2-t1);
#endif /* FLASH_DEBUG_ON */
}

#else /* USE_XTC_LIB_QUADSPI */
#include <xcore/swmem_fill.h>
#include <xmos_flash.h>

#define BYTE_TO_WORD_ADDRESS(b) ((b) / sizeof(uint32_t))

static flash_ports_t flash_ports_0 = {PORT_SQI_CS, PORT_SQI_SCLK, PORT_SQI_SIO,
                               XS1_CLKBLK_5};

// use the flash clock config below to get 50MHz, ~23.8 MiB/s throughput
static flash_clock_config_t flash_clock_config = {
    flash_clock_reference,  0, 1, flash_clock_input_edge_plusone,
    flash_port_pad_delay_1,
};

static flash_qe_config_t flash_qe_config_0 = {flash_qe_location_status_reg_0,
                                       flash_qe_bit_6};

static flash_handle_t flash_handle;

#if FLASH_DEBUG_ON
flash_dbg_data_t flash_dbg_data = {0, 0};
#endif

void flash_setup(void) {
    flash_connect(&flash_handle, &flash_ports_0, flash_clock_config,
                flash_qe_config_0);
}

L2_CACHE_SWMEM_READ_FN
void flash_read_bytes(
    void* dst_addr,
    const void* src_addr,
    const size_t len)
{
    unsigned flash_word_address = BYTE_TO_WORD_ADDRESS(src_addr - (void *)XS1_SWMEM_BASE);

#if FLASH_DEBUG_ON
    unsigned t1 = get_reference_time();
#endif /* FLASH_DEBUG_ON */

    flash_read_quad(&flash_handle,
                  flash_word_address,
                  dst_addr, len >> 2);

#if FLASH_DEBUG_ON
    unsigned t2 = get_reference_time();
    flash_dbg_data.read_count++;
    flash_dbg_data.read_time += (t2-t1);
#endif
}

#endif /* !USE_XTC_LIB_QUADSPI */
