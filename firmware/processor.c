#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_wrap.h"
#include <irq.h>


#include <generated/csr.h>
#include <generated/mem.h>
#include <time.h>

#include "processor.h"
#include "i2c.h"

void processor_init(void) {
  i2c_init();
#ifdef MOTOR
  motor_comm_init();
#endif
}

void processor_start(void) {
  
}

void processor_update(void) {

}

void processor_service(void) {
	// call service routines to various peripherals here
	processor_update();

}
