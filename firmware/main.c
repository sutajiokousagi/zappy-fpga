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

#include <net/microudp.h>
#include <net/tftp.h>

unsigned char mac_addr[6] = {0x13, 0x37, 0x32, 0x0d, 0xba, 0xbe};
unsigned char my_ip_addr[4] = {10, 0, 11, 2}; // my IP address
unsigned char host_ip_addr[4] = {10, 0, 11, 3}; // host IP address

/* Local TFTP client port (arbitrary) */
#define PORT_IN		7642

enum {
	TFTP_RRQ	= 1,	/* Read request */
	TFTP_WRQ	= 2, 	/* Write request */
	TFTP_DATA	= 3,	/* Data */
	TFTP_ACK	= 4,	/* Acknowledgment */
	TFTP_ERROR	= 5,	/* Error */
};

#define	BLOCK_SIZE	512	/* block size in bytes */

static uint8_t *packet_data;
static int total_length;
static int transfer_finished;
static uint8_t *dst_buffer;
static int last_ack; /* signed, so we can use -1 */
static uint16_t data_port;
static void rx_callback(uint32_t src_ip, uint16_t src_port,
    uint16_t dst_port, void *_data, unsigned int length)
{
	uint8_t *data = _data;
	uint16_t opcode;
	uint16_t block;
	int i;
	int offset;

	if(length < 4) return;
	if(dst_port != PORT_IN) return;
	opcode = data[0] << 8 | data[1];
	block = data[2] << 8 | data[3];
	if(opcode == TFTP_ACK) { /* Acknowledgement */
		data_port = src_port;
		last_ack = block;
		return;
	}
	if(block < 1) return;
	if(opcode == TFTP_DATA) { /* Data */
		length -= 4;
		offset = (block-1)*BLOCK_SIZE;
		for(i=0;i<length;i++)
			dst_buffer[offset+i] = data[i+4];
		total_length += length;
		if(length < BLOCK_SIZE)
			transfer_finished = 1;

		packet_data = microudp_get_tx_buffer();
		//length = format_ack(packet_data, block);
		microudp_send(PORT_IN, src_port, length);
	}
	if(opcode == TFTP_ERROR) { /* Error */
		total_length = -1;
		transfer_finished = 1;
	}
}

int main(void) {
  //  telnet_active = 0;
  irq_setmask(0);
  irq_setie(1);
  uart_init();

  puts("Zappy firmware booting...\n");

  print_version();

  eth_init(); // must call before time init
  time_init();
  
  // Setup the Ethernet
  //  ethernet_init(mac_addr, my_ip_addr);
  //  etherbone_init();
  //  telnet_init();
  unsigned int ip;
  microudp_start(mac_addr, IPTOINT(my_ip_addr[0], my_ip_addr[1], my_ip_addr[2], my_ip_addr[3]));
  
  ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
  printf("Resolving ARP for host at %d.%d.%d.%d...\n",
	 host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
  if(!microudp_arp_resolve(ip))
    printf("ARP resolve had unexpected value\n");
  else
    printf("Host resolved!\n");
  microudp_set_callback(rx_callback);
  printf( "TFTP service started.\n" );
  
  processor_init();
  processor_start();

  ci_prompt();
  
  while(1) {
    uptime_service();
    processor_service();
    ci_service();
    //    ethernet_service();
    microudp_service();
  }

  return 0;
}
