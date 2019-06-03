/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_wrap.h"

#include <generated/csr.h>
#include <generated/mem.h>
#include <time.h>
#include <console.h>
#include <hw/flags.h>

#include "gfx.h"

#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

//Optional
//#define SSD1322_USE_DMA

#ifndef SSD1322_USE_DMA
	#define SSD1322_USE_DMA			GFXOFF
#endif

static GFXINLINE void init_board(GDisplay *g) {
	(void) g;
	oled_spi_length_write(8); // spi bus is 8 bits long
	
}

static GFXINLINE void post_init_board(GDisplay *g) {
	(void) g;
}

static GFXINLINE void setpin_reset(GDisplay *g, gBool state) {
	(void) g;
	
	unsigned char dc = oled_gpio_out_read() & 0x2;

	if( state == gTrue )
	  oled_gpio_out_write(dc | 0);  // reset is active low
	else
	  oled_gpio_out_write(dc | 1);
}

static GFXINLINE void acquire_bus(GDisplay *g) {
	(void) g;
}

static GFXINLINE void release_bus(GDisplay *g) {
	(void) g;
}


static GFXINLINE void write_cmd(GDisplay *g, gU8 cmd) {
	(void) g;

	unsigned char res = oled_gpio_out_read() & 0x1;

	// if a previous transaction is running, wait until it's done
	while( oled_spi_status_read() == 0 )
	  ;
	
	// first assert d/c_n = L
	oled_gpio_out_write(res | 0);
	oled_spi_mosi_write(cmd);  // set the data

	oled_spi_ctrl_write(1); // start the transaction

	// code exec continues, even while the transaction runs in the background
}

static GFXINLINE void write_data(GDisplay *g, gU8 data) {
	(void) g;

	unsigned char res = oled_gpio_out_read() & 0x1;

	// if a previous transaction is running, wait until it's done
	while( oled_spi_status_read() == 0 )
	  ;
	
	// first assert d/c_n = H
	oled_gpio_out_write(res | 0x2);
	oled_spi_mosi_write(data);  // set the data

	oled_spi_ctrl_write(1); // start the transaction

	// code exec continues, even while the transaction runs in the background
}

#if SSD1322_USE_DMA
	static GFXINLINE void write_data_DMA(GDisplay *g, gU8* data) {
		(void) g;
		(void) data;
	}
#endif	// Use DMA

#endif /* _GDISP_LLD_BOARD_H */
