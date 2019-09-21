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
#include "uptime.h"
#include "mdio.h"
#include "version.h"

#include "ethernet.h"
#ifdef LIBUIP
#include "telnet.h"
#include "etherbone.h"
#endif

#include "libnet/microudp.h"
#include "libnet/tftp.h"

#include "gfxconf.h"
#include "gfx.h"

#include "ui.h"
#include "si1153.h"
#include "iqmotor.h"
#include "motor.h"
#include "plate.h"
#include "zappy-calibration.h"
#include "temperature.h"
#include "zap.h"

unsigned char mac_addr[6] = {0x13, 0x37, 0x32, 0x0d, 0xba, 0xbe};
unsigned char my_ip_addr[4] = {10, 0, 11, 2}; // my IP address
unsigned char host_ip_addr[4] = {10, 0, 11, 3}; // host IP address
const cal_record zappy_cal = {HOSTNAME, FAST_M, FAST_B, SLOW_M, SLOW_B, P5V_ADC, P5V_ADC_LOGIC, FS_DAC, HVDAC_M, HVDAC_B, CAPRES, ENERGY_COEFF, ONEJOULE};

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

#ifdef LIBUIP
int arp_mode = ARP_MICROUDP;
#endif

uint8_t telnet_tx = 0;
void telnet_hook(char c);
void telnet_hook(char c) {
#ifdef LIBUIP
  if(telnet_active && telnet_tx) {
    telnet_putchar(c);
  }
#endif
}
  
int main(void) {
#ifdef LIBUIP
  telnet_active = 0;
#endif
  irq_setmask(0);
  irq_setie(1);
  uart_init();

  puts("Zappy firmware booting...\n");

  print_version();

  eth_init(); // must call before time init
#ifdef LIBUIP
  clock_init();
#endif
  time_init();
  
  processor_init();
  processor_start();

  ci_prompt();

  // turn off LEDs
  led_out_write(0);

  // initialize buzzer PWM frequency and make sure it's off
  buzzpwm_enable_write(0);
  buzzpwm_period_write(CONFIG_CLOCK_FREQUENCY / 3100);
  buzzpwm_width_write( (CONFIG_CLOCK_FREQUENCY / 3100) / 2 );

  oproxInit();

  gfxInit();
  oled_logo();
  update_temperature();


  // fundamental hardware modes -- used by the global zap control system
  monitor_period_write(CONFIG_CLOCK_FREQUENCY / 1000000); // shoot for 1 microsecond period
  zappio_triggermode_write(0); // use hardware trigger
  zappio_override_safety_write(0); // set to 1 to bypass lockouts for testing

  
  // HV subsystem init: make sure the caps/voltages are safe
  zappio_col_write(0); // no row/col selected
  zappio_row_write(0);
  zappio_hv_engage_write(0); // disengage the supply
  zappio_cap_write(1); // engage the capacitor
  zappio_hv_setting_write(0);  // set supply to zero
  while( !zappio_hv_ready_read() )
    ;
  zappio_hv_update_write(1); 
  zappio_discharge_write(1); // turn on the capacitor discharge resistor
  wait_until_safe(); // full cycle down
  
  zappio_discharge_write(0); // turn off the capacitor discharge resistor
  zappio_cap_write(0); // disengage the capacitor
  

  // Setup the Ethernet
  unsigned int ip;
  microudp_start(mac_addr, my_ip_addr[0], my_ip_addr[1], my_ip_addr[2], my_ip_addr[3]);

  // resolve ARP for host
  ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
  printf("Resolving ARP for host at %d.%d.%d.%d...\n",
	 host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
  if(!microudp_arp_resolve(ip))
    printf("ARP resolve had unexpected value\n");
  else
    printf("Host resolved!\n");

  // set microudp callback for tftp service
  microudp_set_callback(rx_callback);
  printf( "TFTP service started.\n" );

#ifdef LIBUIP
  arp_mode = ARP_LIBUIP;
  telnet_init();
  etherbone_init();
#endif

  // hook in telnet
  console_set_write_hook(telnet_hook);
  
#ifdef MOTOR
  iqCreateMotor();
  snprintf(ui_notifications, sizeof(ui_notifications), "Homing the cams...\n");
  oled_ui();
  
  plate_home();
  
  snprintf(ui_notifications, sizeof(ui_notifications), "Plate cam homed.\n");
  oled_ui();
#endif
  status_led = LED_STATUS_GREEN;

  int interval;
  elapsed(&interval, -1);
  
  while(1) {
    if( elapsed(&interval, CONFIG_CLOCK_FREQUENCY / 2) ) { // twice a second update the temperature
      update_temperature();
    }
    uptime_service();
    processor_service();
    ci_service();
    microudp_service();
    oled_ui();
  }

  return 0;
}
