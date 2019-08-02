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
#include "zappy-calibration.h"
#include "temperature.h"

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

char ui_notifications[32];

// convert a desired voltage on the MK HV supply to a code suitable for the HV dac
uint16_t volts_to_hvdac_code(float voltage) {
  return (uint16_t) ( (voltage * zappy_cal.fs_dac / 1000.0 ) * zappy_cal.hvdac_m + zappy_cal.hvdac_b);
}

// convert ADC binary code to voltage for the MKHV vmon supply
float mk_code_to_voltage(uint16_t code) {
  float lsb = zappy_cal.p5v_adc_logic / 4096.0;
  
  float voltage = ((float)code) * lsb - (zappy_cal.p5v_adc_logic / 8192.0);

  return voltage * 200.0;  // 0-5V ADC corresponds to 0-1000V on supply
}

// convert the ADC binary code to a voltage
float convert_code(uint16_t code, uint8_t adc_path) {
  float lsb = zappy_cal.p5v_adc / 4096.0;
  
  float voltage = ((float)code) * lsb - (zappy_cal.p5v_adc / 8192.0);
  // subtract 0.5LSB as the 0->1 transition occurs at 0.5LSB, not 1.0LSB
  
  if( voltage < 0.0 )
    voltage = 0.0;

  float hv = 0.0;
  if( adc_path == ADC_SLOW ) {
    hv = voltage * zappy_cal.slow_m + zappy_cal.slow_b;
  } else {
    hv = voltage * zappy_cal.fast_m + zappy_cal.fast_b;
  }

  if( hv < 0.0 )
    hv = 0.0;

  return hv;
}

// convert a voltage into a *measurement* ADC binary code (not for HVDAC purposes)
// used to program a "SCRAM" target for "do not exceed" feedback levels (if desired)
// and for capacitor target voltage measurements
uint16_t convert_voltage_adc_code(float hv, uint8_t adc_path) {
  float voltage = 0.0;
  if( adc_path == ADC_SLOW ) {
    voltage = (hv - zappy_cal.slow_b) / zappy_cal.slow_m;
  } else {
    voltage = (hv - zappy_cal.fast_b) / zappy_cal.fast_m;
  }

  float lsb = zappy_cal.p5v_adc / 4096.0;
  uint16_t code = (uint16_t) ((voltage + (zappy_cal.p5v_adc / 8192.0)) / lsb); // slightly wrong as it rounds up

  return code;
}


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

  ///// data graph
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
  

  ///// status data
  gdispDrawStringBox(0, fontheight * line, width, fontheight * (line + 1),
                     ui_notifications, font, White, justifyLeft);
  line--;

  /////// TODO: add code to measure voltage, track last row/column requests
  snprintf(uiStr, sizeof(uiStr), "%4dV, Row %d Col %d", (int) convert_code(max, ADC_FAST), last_row, last_col );
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
  int32_t maxtemp;
  char name[8];
  max_temperature(&maxtemp, name);  // this doesn't update the reading, just fetches the max
  uint32_t remainder;
  if( maxtemp > 0 )
    remainder = maxtemp % 10000;
  else
    remainder = -maxtemp % 10000;
  
  snprintf(uiStr, sizeof(uiStr), "%s %d.%dC %s", zappy_cal.hostname, maxtemp / 10000, remainder / 1000, name);
  gdispDrawStringBox(0, fontheight * line, width, fontheight * (line + 1) + 3,
                     uiStr, font, Gray, justifyLeft);


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

