#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <time.h>
#include <generated/csr.h>
#include <generated/mem.h>
#include "flags.h"
#include <console.h>
#include <system.h>

#include "ci.h"
#include "processor.h"

void __stack_chk_fail(void);
void * __stack_chk_guard = (void *) (0xDEADBEEF);
void __stack_chk_fail(void) {
  printf( "stack fail\n" );
}

int main(void)
{
	irq_setmask(0);
	irq_setie(1);
	uart_init();

	puts("\nZappy software built "__DATE__" "__TIME__);

	time_init();

	processor_init();
	processor_update();
	processor_start();

	ci_prompt();

	int wait_event;
	elapsed(&wait_event, SYSTEM_CLOCK_FREQUENCY);
	
	while(1) {
	  processor_service();
	  ci_service();
	}

	return 0;
}
