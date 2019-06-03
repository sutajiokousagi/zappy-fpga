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
#include "uptime.h"

void Raw32OSInit(void);

systemticks_t gfxSystemTicks(void) {
  return (systemticks_t) uptime_ms();
}

systemticks_t gfxMillisecondsToTicks(delaytime_t ms) {
  return (systemticks_t) ms;
}

void Raw32OSInit(void) {
  // nothing to init, already handled in main.c
}
