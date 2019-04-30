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

#include "ci.h"
#include "processor.h"
#include "etherbone.h"
#include "ethernet.h"
#include "telnet.h"
#include "uptime.h"
#include "mdio.h"
#include "version.h"

unsigned char mac_addr[6] = {0x13, 0x37, 0x32, 0x0d, 0xba, 0xbe};
unsigned char ip_addr[4] = {10, 0, 11, 2};

int main(void) {
  telnet_active = 0;
  irq_setmask(0);
  irq_setie(1);
  uart_init();

  puts("Zappy firmware booting...\n");

  time_init();
  print_version();
  mdio_status();

  // Setup the Ethernet
  ethernet_init(mac_addr, ip_addr);
  etherbone_init();
  telnet_init();
  
  processor_init();
  processor_start();

  ci_prompt();
  
  while(1) {
    uptime_service();
    processor_service();
    ci_service();
    ethernet_service();
  }

  return 0;
}
