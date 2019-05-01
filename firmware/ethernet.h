// This file is Copyright (c) 2015 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#ifndef __ETHERNET_H
#define __ETHERNET_H

#include "contiki.h"
#include "contiki-net.h"

//#define LIBUIP  // include the UIP-specific hooks on top of microudp (define in Makefile)
//#define UIP_DEBUG

#define max(a,b) ((a>b)?a:b)
#define min(a,b) ((a<b)?a:b)

void uip_log(char *msg);

extern unsigned char mac_addr[6];
extern unsigned char my_ip_addr[];
extern unsigned char host_ip_addr[];

#ifdef LIBUIP
extern int arp_mode;
#define ARP_MICROUDP 0
#define ARP_LIBUIP   1
#endif

#endif
