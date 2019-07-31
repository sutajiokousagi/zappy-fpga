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
#include "ui.h"
#include "plate.h"
#include "si1153.h"
#include "zap.h"

#define HOSTNAME  "zappy-01"

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

Calibration curve measurements: 

At temperature: 29.5C

0.0027V
slow: 0.715mV
fast: 0.957mV

4.9967 V
slow: 21.77mV
fast: 23.40mV

9.9903 V
slow: 43.49mV
fast: 46.52mV

14.986 V
slow: 65.22 mV  (+/-0.01mV)
fast: 69.67 mV  (+/-0.01mV)

19.980 V
slow: 86.94mV
fast: 92.83mV

24.977 V
slow: 0.10866V
fast: 0.11597V

34.966 V
slow: 0.15212V
fast: 0.16229V
 */

char ui_notifications[32];

void oled_ui(void) {
  coord_t width, fontheight;
  coord_t height;
  font_t font;
  char uiStr[32];
  int line = 3;
  int i = 0;

  width = gdispGetWidth();
  height = gdispGetHeight();
  font = gdispOpenFont("UI2");
  fontheight = gdispGetFontMetric(font, fontHeight);

  gdispClear(Black);

  gdispDrawStringBox(0, fontheight * line, width, fontheight * (line + 1),
                     ui_notifications, font, White, justifyLeft);
  line--;

  /////// TODO: add code to measure voltage, track last row/column requests
  snprintf(uiStr, sizeof(uiStr), "____V, Row x Col yy" );
  gdispDrawStringBox(0, fontheight * line, width, fontheight * (line + 1),
                     uiStr, font, Gray, justifyLeft);
  line--;


  // plate state
  plate_state pstate = get_platestate();
  
  i = snprintf(uiStr, sizeof(uiStr), "prox: %5d / ", getSensorData() );
  switch(pstate) {
  case platestate_unlocked:
    snprintf(&(uiStr[i]), sizeof(uiStr)-i, "%s", "unlocked");
    break;
  case platestate_locked:
    snprintf(&(uiStr[i]), sizeof(uiStr)-i, "%s", "locked");
    break;
  case platestate_warning:
    snprintf(&(uiStr[i]), sizeof(uiStr)-i, "%s", "warning");
    break;
  default:
    snprintf(&(uiStr[i]), sizeof(uiStr)-i, "%s", "JAM!");
    break;
  }
  gdispDrawStringBox(0, fontheight * line, width, fontheight * (line + 1),
                     uiStr, font, Gray, justifyLeft);
  line--;


  /// hostname
  snprintf(uiStr, sizeof(uiStr), "Hostname: %s", HOSTNAME);
  gdispDrawStringBox(0, fontheight * line, width, fontheight * (line + 1) + 3,
                     uiStr, font, Gray, justifyLeft);


  // vertical axis
  gdispDrawLine(width/2, 0, width/2, height, Gray);

  // horizontal axis
  gdispDrawLine(width/2, height-1, width, height-1, Gray);

  uint16_t *y = (uint16_t *)MONITOR_BASE;
  uint16_t stride = SAMPLEDEPTH / (width/2);
  uint16_t data[width/2];

  for( i = 0; i < width/2; i++ ) {
    // + 0 (even) => slow path (measures cap directly, isolated from cell by 24 ohm resistor)
    // + 1 (odd)  => fast path (measures hv_main/electroporation cell directly -- 24 ohm resistor to capacitor)
    data[i] = y[ (i * stride) * 2 + 1 ];  // x2 because every 32-bit record is a voltage/current pair
  }
  // find max
  uint16_t max = 0;
  for( i = 0; i < width/2; i++ ) {
    if( data[i] > max )
      max = data[i];
  }
  
  // if greater than max val, clip & rescale
  if( max >= height ) {
    for( i = 0; i < width/2; i++ ) {
      data[i] = (uint16_t) ((float) data[i] * (float) (height - 1.0) / (float) max);
      if( data[i] > height-1 )
	data[i] = height-1;
    }
  }
  
  // now flip axis
  for( i = 0; i < width / 2; i++ ) {
    data[i] = (height-1) - data[i];
  }
  
  for( i = 0; i < width / 2; i++ ) {
    gdispDrawPixel(width/2 + i, data[i], White);
  }
  
  gdispCloseFont(font);
  
  gdispFlush();
}


void oled_logo(void) {
  GDisplay *g = gdispGetDisplay(0);
  uint8_t *ram = g->priv;
  memcpy(ram, &ginkgo_logo[119], 8192);
  g->flags |= (GDISP_FLG_DRIVER<<0);
  
  gdispFlush();
}

static void oled_banner(void) {
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

