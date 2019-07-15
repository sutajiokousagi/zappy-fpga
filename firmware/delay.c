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
#include "delay.h"

void delay_ms(int ms) {
  int timer;

  elapsed(&timer, -1); // initialize the timer
  while( !elapsed(&timer, (SYSTEM_CLOCK_FREQUENCY / 1000) * ms) )
    ;
}

void delay(int ms) {
  int loops, i;
  int remainder;

  loops = ms / 500;
  remainder = ms % 500;

  for( i = 0; i < loops; i++ ) {
    delay_ms(500);
  }
  delay_ms(remainder);
}

