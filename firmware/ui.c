#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <time.h>
#include <generated/csr.h>
#include <generated/mem.h>
#include <hw/flags.h>
#include <console.h>
#include <system.h>

#include "gfxconf.h"
#include "gfx.h"
#include "ginkgo-logo.h"
#include "src/gdisp/gdisp_driver.h"
#include "uptime.h"

/*

font is 8 char wide = 32 char displayable

               1       2       3
0      7       5       3       1
--------------------------------
zappy-01        |255.255.255.255
###### closed   | [wave]
####V R# C##    | 
Errs: none      | #### V, ##.#ms
--------------------------------

UI elements:

 [hostname]  [ip address]  [uptime]
 [plate status] [oprox level] [cam position]    [last data run]
 [current voltage] [last row] [last col]
 [error message area]

 */

void oled_logo(void) {
  GDisplay *g = gdispGetDisplay(0);
  uint8_t *ram = g->priv;
  memcpy(ram, &ginkgo_logo[119], 8192);
  g->flags | (GDISP_FLG_DRIVER<<0);
  
  gdispFlush();
}

void oled_banner(void) {
  coord_t width, fontheight;
  font_t font;

  width = gdispGetWidth();
  font = gdispOpenFont("UI2");
  fontheight = gdispGetFontMetric(font, fontHeight);

  gdispClear(Black);
  gdispDrawStringBox(0, fontheight, width, fontheight * 2,
                     "Zappy", font, Gray, justifyCenter);
  gdispDrawStringBox(0, fontheight * 2, width, fontheight * 3,
                     "EVT1", font, Gray, justifyCenter);

  gdispCloseFont(font);
  
  gdispFlush();
}

