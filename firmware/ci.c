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
	  unsigned int ip;
	  ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
	  tftp_put(ip, DEFAULT_TFTP_SERVER_PORT, "zappy-log.1", (void *)0x40000000, 32*1024);
	} else if(strcmp(token, "debug") == 0) {
	  token = get_token(&str);
	  if(strcmp(token, "foo") == 0) {
	    wputs( "foo!" );
	  }
	  else if(strcmp(token, "bar") == 0) {
	    wprintf("bar!\r\n");
	  } else {
	    help_debug();
	  }
	} else {
	  
	}
	if( !was_dummy ) {
	  ci_prompt();
	}
}

