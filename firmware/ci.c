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
#include "i2c.h"
#include "si1153.h"

static i2cSensorConfig_t sensI2C;			//Holds i2c information about the connected sensor
static Si115xSample_t samples;				//Stores the sample data from reading the sensor
static uint8_t initialized = SI11XX_NONE;	//Tracks which sensor demo is initialized

void getSensorData(void)
{
	// Start next measurement
	Si115xForce(&sensI2C);

	// Sensor data ready
	// Process measurement
	Si115xHandler(&sensI2C, &samples);
}

static void ci_help(void)
{
	wputs("help        - this command");
	wputs("reboot      - reboot CPU");
#ifdef CSR_ETHPHY_MDIO_W_ADDR
	wputs("mdio_dump   - dump mdio registers");
	wputs("mdio_status - show mdio status");
#endif
	wputs("uptime      - show uptime");
	wputs("upload      - do upload test");
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

	if(readchar_nonblock()) {
		c[0] = readchar();
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
	  ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
	  int i;
	  // send a megabyte
	  int start, stop;
	  elapsed(&start, -1);
	  for( i = 0; i < 32; i++ ) {
	    tftp_put(ip, DEFAULT_TFTP_SERVER_PORT, "zappy-log.1", (void *)0x40000000, 32*1024);
	  }
	  elapsed(&stop, -1);
	  i = stop - start;
	  if( i < 0 ) i += timer0_reload_read();
	  printf("Elapsed ticks for 1MiB: %d, or %dms per megabyte\n",
		 i, (i)*1000/SYSTEM_CLOCK_FREQUENCY);
	} else if(strcmp(token, "seed") == 0) {
	  unsigned int seed = strtoul(get_token(&str), NULL, 0);
	  printf("Setting memory with seed value %08x\n", seed);
	  memtest_seed_write(seed);
	  memtest_count_write(32768 - 1);
	  memtest_update_write(1);
	  while( memtest_done_read() == 0 )
	    ;

	} else if(strcmp(token, "i2c") == 0) {
	  token = get_token(&str);
	  if(strcmp(token, "test") == 0) {
	    uint8_t tx[3];
	    uint8_t rx[2];
	    int ret;

	    tx[0] = 0x1; // pointer
	    tx[1] = 0x60; // set 12 bits
	    tx[2] = 0x0;
	    ret = i2c_master(0x4C, tx, 3, NULL, 0, 50000000);
	    if( ret ) {
	      printf( "I2C call returned %d errors\n", ret );
	    }
	    
	    tx[0] = 0x0; // pointer
	    ret = i2c_master(0x4C, tx, 1, NULL, 0, 50000000);
	    if( ret ) {
	      printf( "I2C call returned %d errors\n", ret );
	    }

	    rx[0] = 0; rx[1] = 0;
	    ret = i2c_master(0x4C, tx, 0, rx, 2, 50000000);
	    if( ret ) {
	      printf( "I2C call returned %d errors\n", ret );
	    }
	    printf( "Temperature registers: 0x%02x 0x%02x\n", rx[0], rx[1] );
	    int16_t inttemp = ((int16_t) rx[0] << 8) | (int16_t)rx[1];
	    inttemp = inttemp >> 4;
	    int32_t longtemp = (int32_t) inttemp;
	    longtemp = longtemp * 625;
	    uint32_t remainder;
	    if( longtemp > 0 )
	      remainder = longtemp % 10000;
	    else
	      remainder = -longtemp % 10000;
	    printf( "Tempertaure: %d.%dC\n", longtemp / 10000, remainder / 1000 );
	  } else {
	    printf( "i2c subcommand not recognized\n" );
	  }
	} else if(strcmp(token, "debug") == 0) {
	  token = get_token(&str);
	  if(strcmp(token, "led") == 0) {
	    led_out_write( (unsigned char) strtoul(get_token(&str), NULL, 0) );
	  } else if(strcmp(token, "buzz") == 0) {
	    buzzpwm_enable_write( (unsigned char) strtoul(get_token(&str), NULL, 0) );
	  } else if(strcmp(token, "hvdac") == 0) {
	    hvdac_data_write((unsigned short int) strtoul(get_token(&str), NULL, 0));
	    while( !hvdac_ready_read() )
	      ;
	    hvdac_update_write(1);
	  } else if(strcmp(token, "hvengage") == 0) {
	    hvengage_out_write( (unsigned char) strtoul(get_token(&str), NULL, 0) );
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
	} else if(strcmp(token, "oprox") == 0 ) {
	  token = get_token(&str);
	  if(strcmp(token, "init") == 0) {
	    sensI2C.i2cAddress = SI1153_I2C_ADDR;
	    
	    Si115xInitProxAls(&sensI2C, false);
	    getSensorData(); // populate initial recrods
	  } else if(strcmp(token, "prox") == 0) {
	    int i;
	    Si115xInitProxAls(&sensI2C, true);
	    for( i = 0; i < 50; i++ ) {
	      getSensorData(); 
	      int32_t result = (int32_t) samples.ch0;
	      if(result < 0){
		result = 0;
	      }
	      printf("Prox %d counts\n\r", result);
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

