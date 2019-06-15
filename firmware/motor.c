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

#include <irq.h>

#include "motor.h"

void motor_init(void) {
  iqCreateMotor();
}

void delay(int ms) {
  int loops, i;
  int remainder;

  loops = ms / 1000;
  remainder = ms % 1000;

  for( i = 0; i < loops; i++ ) {
    delay_ms(1000);
  }
  delay_ms(remainder);
}


void do_motor(char *token) {
  if( strcmp(token, "test") == 0 ) {
    iqSetAngle(3.14 * 2, 2000);  // 24 is full stroke, but 2 is safe for testing
    delay_ms(2000);
    iqSetAngle(0, 2000);
    delay_ms(2000);
  } else {
    printf("Motor command %s unrecognized\n", token);
  }
}
