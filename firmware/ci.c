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

#include "asm.h"
#include "processor.h"
#include "dump.h"
#include "ci.h"
#include "uptime.h"
#include "mdio.h"
#include <net/microudp.h>
#include <net/tftp.h>
#include "ethernet.h"

#include "si1153.h"
#include "motor.h"
#include "plate.h"
#include "temperature.h"
#include "zap.h"

#include "gfxconf.h"
#include "gfx.h"

#ifdef LIBUIP
#include "telnet.h"
#include "etherbone.h"
#endif

#if 0
void oled_test(void) {
  coord_t width, fontheight;
  font_t font;

  width = gdispGetWidth();
  font = gdispOpenFont("UI2");
  fontheight = gdispGetFontMetric(font, fontHeight);

  gdispClear(Black);
  gdispDrawStringBox(0, fontheight, width, fontheight * 2,
                     "Zappy", font, White, justifyCenter);
  gdispDrawStringBox(0, fontheight * 2, width, fontheight * 3,
                     "EVT1", font, White, justifyCenter);
  gdispFlush();
  gdispCloseFont(font);
}
#endif

static void ci_help(void)
{
	wputs("help        - this command");
	wputs("reboot      - reboot CPU");
#ifdef CSR_ETHPHY_MDIO_W_ADDR
	wputs("mdio_dump   - dump mdio registers");
	wputs("mdio_status - show mdio status");
#endif
	wputs("uptime      - show uptime");
	wputs("upload      - upload data");
	wputs("plate       - plate [<lock/unlock>]");
	wputs("zap         - zap [row, col, voltage] - all args ints");
	wputs("");
	wputs("mr          - read address space");
	wputs("mw          - write address space");
	wputs("mc          - copy address space");
	wputs("");
}

static void help_debug(void) {
  wputs("If only...");
}

static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

#ifdef LIBUIP
	while(readchar_nonblock() || telnet_readchar_nonblock()) {
	  if( readchar_nonblock() ) {
	    c[0] = readchar();
	  } else {
	    c[0] = telnet_readchar(); // inject telnet commands directly here
	  }
	  // printf( "%02x ", c[0] );
#else
	if(readchar_nonblock()) {
	  c[0] = readchar();
#endif
		c[1] = 0;
		switch(c[0]) {
			case 0x7f:
			case 0x08:
				if(ptr > 0) {
					ptr--;
					wputsnonl("\x08 \x08");
				}
				break;
			case 0x07:
				break;
			case '\r':
			case '\n':
				s[ptr] = 0x00;
				wputsnonl("\n");
				ptr = 0;
				return s;
			default:
				if(ptr >= (sizeof(s) - 1))
					break;
				wputsnonl(c);
				s[ptr] = c[0];
				ptr++;
				break;
		}
	}

	return NULL;
}

static char *get_token_generic(char **str, char delimiter)
{
	char *c, *d;

	c = (char *)strchr(*str, delimiter);
	if(c == NULL) {
		d = *str;
		*str = *str+strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c+1;
	return d;
}

static char *get_token(char **str)
{
	return get_token_generic(str, ' ');
}

static void reboot(void)
{
	REBOOT;
}

static void status_service(void) {
  // put per-loop status printing infos here
}

void ci_prompt(void)
{
  wprintf("ZAPPY %s> ", uptime_str());
}

#define DEFAULT_TFTP_SERVER_PORT 69  /* IANA well known port: UDP/69 */
#ifndef TFTP_SERVER_PORT
#define TFTP_SERVER_PORT DEFAULT_TFTP_SERVER_PORT
#endif
void ci_service(void)
{
	char *str;
	char *token;
	char dummy[] = "dummy";
	int was_dummy = 0;
	int depth = 10000;  // this is used by the acquire & upload routine
	
	status_service();

	str = readstr();
	
	if(str == NULL) {
	  str = (char *) dummy;
	}

	token = get_token(&str);

	if(strncmp(token, "dummy", 5) == 0) {
	  was_dummy = 1;
	} else if(strcmp(token, "help") == 0) {
	  wputs("Available commands:");
	  token = get_token(&str);
	  ci_help();
	  wputs("");
	} else if(strcmp(token, "reboot") == 0) reboot();
	else if(strcmp(token, "mr") == 0) mr(get_token(&str), get_token(&str));
	else if(strcmp(token, "mw") == 0) mw(get_token(&str), get_token(&str), get_token(&str));
	else if(strcmp(token, "mc") == 0) mc(get_token(&str), get_token(&str), get_token(&str));
#ifdef CSR_ETHPHY_MDIO_W_ADDR
	else if(strcmp(token, "mdio_status") == 0)
		mdio_status();
	else if(strcmp(token, "mdio_dump") == 0)
		mdio_dump();
#endif
	else if(strcmp(token, "uptime") == 0)
	  uptime_print();
	else if(strcmp(token, "upload") == 0) {
	  // send up 1 megabyte of data to benchmark upload speed
	  unsigned int ip;
	  int i;
	  ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
	  // send a megabyte
	  int start, stop;
	  elapsed(&start, -1);
	  tftp_put(ip, DEFAULT_TFTP_SERVER_PORT, "zappy-log.1", (void *)MONITOR_BASE, depth*4);
	  elapsed(&stop, -1);
	  i = stop - start;
	  if( i < 0 ) i += timer0_reload_read();
	  printf("Elapsed ticks for log upload: %d, or %dms for %d bytes\n",
		 i, (i)*1000/SYSTEM_CLOCK_FREQUENCY, depth*4);
#if 0
	} else if(strcmp(token, "benchmark") == 0) {
	  // send up 1 megabyte of data to benchmark upload speed
	  unsigned int ip;
	  ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
	  int i;
	  // send a megabyte
	  int start, stop;
	  elapsed(&start, -1);
	  for( i = 0; i < 32; i++ ) {
	    tftp_put(ip, DEFAULT_TFTP_SERVER_PORT, "zappy-bwtest.1", (void *)MAIN_RAM_BASE, 4*1024);
	  }
	  elapsed(&stop, -1);
	  i = stop - start;
	  if( i < 0 ) i += timer0_reload_read();
	  printf("Elapsed ticks for 1MiB: %d, or %dms per megabyte\n",
		 i, (i)*1000/SYSTEM_CLOCK_FREQUENCY);
#endif
#if 0	   // for the memtester module, leave this around until we've validated the ADC capture memory module
	} else if(strcmp(token, "seed") == 0) {
	  unsigned int seed = strtoul(get_token(&str), NULL, 0);
	  printf("Setting memory with seed value %08x\n", seed);
	  memtest_seed_write(seed);
	  memtest_count_write(16384 - 1);
	  memtest_update_write(1);
	  while( memtest_done_read() == 0 )
	    ;
#endif
	} else if(strcmp(token, "acquire") == 0) {
	  int acq_timer, start_time;
	  printf("Testing acquisition with depth %d\n", depth);
	  monitor_period_write(SYSTEM_CLOCK_FREQUENCY / 1000000); // shoot for 1 microsecond period
	  monitor_depth_write(depth);
	  elapsed(&acq_timer, -1);
	  start_time = acq_timer;
	  monitor_acquire_write(1); // start acquisition
	  while( monitor_done_read() ) // wait for done to go 0
	    ;
	  while( monitor_done_read() == 0 ) // wait for done to go back to a 1
	    ;
	  elapsed(&acq_timer, -1);
	  int delta = acq_timer - start_time;
	  if( delta < 0 )
	    delta += timer0_reload_read();
	  printf("Acquisition finished in %d ticks or %d ms. Overrun status: %d\n", delta, (delta)*1000/SYSTEM_CLOCK_FREQUENCY,
		 monitor_overrun_read());
	  printf("Run 'upload' to get a copy of the data\n");
	} else if(strcmp(token, "zap") == 0) {
	  uint8_t row = strtoul(get_token(&str), NULL, 0);
	  uint8_t col = strtoul(get_token(&str), NULL, 0);
	  uint32_t voltage = strtoul(get_token(&str), NULL, 0);
	  do_zap(row, col, voltage, depth);
	} else if(strcmp(token, "temp") == 0) {
	  update_temperature();
	  print_temperature();
	} else if(strcmp(token, "debug") == 0) {
	  token = get_token(&str);
	  if(strcmp(token, "led") == 0) {
	    led_out_write( (unsigned char) strtoul(get_token(&str), NULL, 0) );
	  } else if(strcmp(token, "buzz") == 0) {
	    buzzpwm_enable_write( (unsigned char) strtoul(get_token(&str), NULL, 0) );
	  } else if(strcmp(token, "hvdac") == 0) {
	    zappio_hv_setting_write((unsigned short int) strtoul(get_token(&str), NULL, 0));
	    while( !zappio_hv_ready_read() )
	      ;
	    zappio_hv_update_write(1);
	  } else if(strcmp(token, "hvengage") == 0) {
	    zappio_hv_engage_write( (unsigned char) strtoul(get_token(&str), NULL, 0) );
	  } else if(strcmp(token, "vmon") == 0) {
	    vmon_acquire_write(1);
	    while( !vmon_valid_read() )
	      ;
	    printf( "vmon: %d\n", vmon_data_read() );
	  } else if(strcmp(token, "imon") == 0) {
	    imon_acquire_write(1);
	    while( !imon_valid_read() )
	      ;
	    printf( "imon: %d\n", imon_data_read() );
	  } else {
	    help_debug();
	  }
#ifdef MOTOR
	} else if(strcmp(token, "motor") == 0 ) {
	  token = get_token(&str);
	  do_motor(token);
#endif
	} else if(strcmp(token, "plate") == 0 ) {
	  token = get_token(&str);
	  do_plate(token);
	} else if(strcmp(token, "oprox") == 0 ) {
	  token = get_token(&str);
	  if(strcmp(token, "init") == 0) {
	    oproxInit();
	  } else if(strcmp(token, "prox") == 0) {
	    int i;
	    for( i = 0; i < 50; i++ ) {
	      printf("Prox %d counts\n\r", getSensorData());
	      delay_ms(50);
	    }
	  }
	} else {
	  if( strlen(token) > 0 )
	    printf("Command %s not recognized.\n", token);
	}
	if( !was_dummy ) {
	  ci_prompt();
	}
	
}

