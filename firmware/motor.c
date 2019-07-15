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

#include "iqmotor.h"
#include "motor.h"
#include "delay.h"

void motor_init(void) {
  iqCreateMotor();
}

/*
 iqSetCoast: 55 02 3b 02 01 bf 2a (c++)
motor write: 55 02 3b 02 01 bf 2a (c)

Press enter to break.
 iqSetAngle: 55 03 3b 00 01 06 df 9e 55 06 3b 13 01 52 b8 96 42 a4 c8 55 06 3b 16 01 00 00 00 40 a7 d6 55 02 3b 0b 00 06 80 (c++ with 24 rotations)
 iqSetAngle: 55 03 3b 00 01 06 df 9e 55 06 3b 13 01 c3 f5 c8 40 81 2a 55 06 3b 16 01 00 00 00 40 a7 d6 55 02 3b 0b 00 06 80 (c++ with 2 rotations)
motor write: 55 03 3b 00 01 06 df 9e 55 06 3b 13 01 c3 f5 c8 40 81 2a 55 06 3b 16 01 00 00 00 40 a7 d6 55 02 3b 0b 00 06 80 (c)
                                                    ^^ ^^ ^^ ^^ ^^ ^^

 iqSetAngle: 55 03 3b 00 01 06 df 9e 55 06 3b 13 01 00 00 00 00 62 dd 55 06 3b 16 01 00 00 00 40 a7 d6 55 02 3b 0b 00 06 80 (c++)
motor write: 55 03 3b 00 01 06 df 9e 55 06 3b 13 01 00 00 00 00 62 dd 55 06 3b 16 01 00 00 00 40 a7 d6 55 02 3b 0b 00 06 80 (c)

 */
void do_motor(char *token) {
  if( strcmp(token, "test") == 0 ) {
    iqSetAngle(3.14 * 2, 2000);  // 24 is full stroke, but 2 is safe for testing
    delay(2000);
    delay(10);
    iqSetAngle(0, 2000);
    delay(2000);
  } else if( strcmp(token, "init") == 0 ) {
    printf( "creating motor object\n" );
    iqCreateMotor();
  } else {
    printf("Motor command %s unrecognized\n", token);
  }
}
