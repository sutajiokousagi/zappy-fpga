#include <generated/csr.h>
#include <generated/mem.h>

#ifdef CSR_ETHMAC_BASE

#include <stdio.h>
#include <inet.h>
#include <system.h>
#include <crc.h>
#include <hw/flags.h>

#include "microudp.h"
#include "../ethernet.h"
#include "tftp.h"

#ifdef LIBUIP
#include <time.h>
#endif

//#define DEBUG_MICROUDP_TX
//#define DEBUG_MICROUDP_RX
//#define DEBUG_LIBUIP_RX

#define ETHERTYPE_ARP 0x0806
#define ETHERTYPE_IP  0x0800

#ifdef CSR_ETHMAC_PREAMBLE_CRC_ADDR
#define HW_PREAMBLE_CRC
#endif


struct ethernet_header {
#ifndef HW_PREAMBLE_CRC
	unsigned char preamble[8];
#endif
	unsigned char destmac[6];
	unsigned char srcmac[6];
	unsigned short ethertype;
} __attribute__((packed));

static void fill_eth_header(struct ethernet_header *h, const unsigned char *destmac, const unsigned char *srcmac, unsigned short ethertype)
{
	int i;

#ifndef HW_PREAMBLE_CRC
	for(i=0;i<7;i++)
		h->preamble[i] = 0x55;
	h->preamble[7] = 0xd5;
#endif
	for(i=0;i<6;i++)
		h->destmac[i] = destmac[i];
	for(i=0;i<6;i++)
		h->srcmac[i] = srcmac[i];
	h->ethertype = htons(ethertype);
}

#define ARP_HWTYPE_ETHERNET 0x0001
#define ARP_PROTO_IP        0x0800
#ifndef HW_PREAMBLE_CRC
#define ARP_PACKET_LENGTH 68
#else
#define ARP_PACKET_LENGTH 60
#endif

#define ARP_OPCODE_REQUEST  0x0001
#define ARP_OPCODE_REPLY    0x0002

struct arp_frame {
	unsigned short hwtype;
	unsigned short proto;
	unsigned char hwsize;
	unsigned char protosize;
	unsigned short opcode;
	unsigned char sender_mac[6];
	unsigned int sender_ip;
	unsigned char target_mac[6];
	unsigned int target_ip;
	unsigned char padding[18];
} __attribute__((packed));

#define IP_IPV4			0x45
#define IP_DONT_FRAGMENT	0x4000
#define IP_TTL			64
#define IP_PROTO_UDP		0x11
#define IP_PROTO_ICMP           0x1
#define IP_PROTO_TCP            0x6

struct ip_header {
	unsigned char version;
	unsigned char diff_services;
	unsigned short total_length;
	unsigned short identification;
	unsigned short fragment_offset;
	unsigned char ttl;
	unsigned char proto;
	unsigned short checksum;
	unsigned int src_ip;
	unsigned int dst_ip;
} __attribute__((packed));

#define ICMP_OVERHEAD 28
struct icmp_header {
  unsigned char type;		/* message type */
  unsigned char code;		/* type sub-code */
  unsigned short checksum;
  union
  {
    struct
    {
      unsigned short	id;
      unsigned short	sequence;
    } echo;			/* echo datagram */
    unsigned int	gateway;	/* gateway address */
    struct
    {
      unsigned short	__unused;
      unsigned short	mtu;
    } frag;			/* path mtu discovery */
  } un;
} __attribute__((packed));


#define ICMP_ECHOREPLY		0	/* Echo Reply			*/
#define ICMP_DEST_UNREACH	3	/* Destination Unreachable	*/
#define ICMP_SOURCE_QUENCH	4	/* Source Quench		*/
#define ICMP_REDIRECT		5	/* Redirect (change route)	*/
#define ICMP_ECHO		8	/* Echo Request			*/
#define ICMP_TIME_EXCEEDED	11	/* Time Exceeded		*/
#define ICMP_PARAMETERPROB	12	/* Parameter Problem		*/
#define ICMP_TIMESTAMP		13	/* Timestamp Request		*/
#define ICMP_TIMESTAMPREPLY	14	/* Timestamp Reply		*/
#define ICMP_INFO_REQUEST	15	/* Information Request		*/
#define ICMP_INFO_REPLY		16	/* Information Reply		*/
#define ICMP_ADDRESS		17	/* Address Mask Request		*/
#define ICMP_ADDRESSREPLY	18	/* Address Mask Reply		*/
#define NR_ICMP_TYPES		18

struct udp_header {
	unsigned short src_port;
	unsigned short dst_port;
	unsigned short length;
	unsigned short checksum;
} __attribute__((packed));

struct udp_frame {
	struct ip_header ip;
	struct udp_header udp;
	char payload[];
} __attribute__((packed));

struct icmp_frame {
  struct ip_header ip;
  struct icmp_header icmp;
  char payload[];
} __attribute__((packed));

struct ethernet_frame {
	struct ethernet_header eth_header;
	union {
	  struct arp_frame arp;
	  struct udp_frame udp;
	  struct icmp_frame icmp; 
	} contents;
} __attribute__((packed));

typedef union {
	struct ethernet_frame frame;
	unsigned char raw[ETHMAC_SLOT_SIZE];
} ethernet_buffer;

static unsigned int rxslot;
static unsigned int rxlen;
static ethernet_buffer *rxbuffer;

static unsigned int txslot;
static unsigned int txlen;
static ethernet_buffer *txbuffer;


#ifdef LIBUIP
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

static int uip_periodic_event;
static int uip_periodic_period;

static int uip_arp_event;
static int uip_arp_period;

static ethernet_buffer *rxbuffer0;
static ethernet_buffer *rxbuffer1;
static ethernet_buffer *txbuffer0;
static ethernet_buffer *txbuffer1;

void uip_log(char *msg)
{
#ifdef UIP_DEBUG
    puts(msg);
#endif
}

#endif // LIBUIP

static void send_packet(void)
{
	/* wait buffer to be available */
	while(!(ethmac_sram_reader_ready_read()));

	/* fill txbuffer */
#ifndef HW_PREAMBLE_CRC
	unsigned int crc;
	crc = crc32(&txbuffer->raw[8], txlen-8);
	txbuffer->raw[txlen  ] = (crc & 0xff);
	txbuffer->raw[txlen+1] = (crc & 0xff00) >> 8;
	txbuffer->raw[txlen+2] = (crc & 0xff0000) >> 16;
	txbuffer->raw[txlen+3] = (crc & 0xff000000) >> 24;
	txlen += 4;
#endif

#ifdef DEBUG_MICROUDP_TX
	int j;
	printf(">>>> txlen : %d\n", txlen);
	for(j=0;j<txlen;j++) {
	  if( (j % 8) == 0 )
	    printf(" ");
	  printf("%02x",txbuffer->raw[j]);
	}
	printf("\n");
#endif

	/* fill slot, length and send */
	ethmac_sram_reader_slot_write(txslot);
	ethmac_sram_reader_length_write(txlen);
	ethmac_sram_reader_start_write(1);

	/* update txslot / txbuffer */
	txslot = (txslot+1)%ETHMAC_TX_SLOTS;
	txbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * (ETHMAC_RX_SLOTS + txslot));
}

// this thunk allows us to access the static variables in microudp.c to prepare data for send_packet()
void send_packet_etherbone(unsigned char *raw, int rawlen) {
  memcpy(txbuffer, raw, rawlen);
  txlen = rawlen;
  send_packet();
}

#ifdef LIBUIP
static void libuip_send(void) {
  txlen = uip_len;
  memset(txbuffer, 0, 60);
  txlen = MIN(txlen, 1514);
  memcpy(txbuffer, uip_buf, txlen);
  txlen = MAX(txlen, 60);

  /* fill slot, length and send */
  ethmac_sram_reader_slot_write(txslot);
  ethmac_sram_reader_length_write(txlen);
  ethmac_sram_reader_start_write(1);
  
  /* update txslot / txbuffer */
  txslot = (txslot+1)%ETHMAC_TX_SLOTS;
  txbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * (ETHMAC_RX_SLOTS + txslot));
}
#endif

static unsigned char my_mac[6];
static unsigned int my_ip;

/* ARP cache - one entry only */
static unsigned char cached_mac[6];
static unsigned int cached_ip;

static void process_arp(void)
{
	const struct arp_frame *rx_arp = &rxbuffer->frame.contents.arp;
	struct arp_frame *tx_arp = &txbuffer->frame.contents.arp;

	if(rxlen < ARP_PACKET_LENGTH) return;
	if(ntohs(rx_arp->hwtype) != ARP_HWTYPE_ETHERNET) return;
	if(ntohs(rx_arp->proto) != ARP_PROTO_IP) return;
	if(rx_arp->hwsize != 6) return;
	if(rx_arp->protosize != 4) return;

	if(ntohs(rx_arp->opcode) == ARP_OPCODE_REPLY) {
		if(ntohl(rx_arp->sender_ip) == cached_ip) {
			int i;
			for(i=0;i<6;i++)
				cached_mac[i] = rx_arp->sender_mac[i];
		}
		return;
	}
	if(ntohs(rx_arp->opcode) == ARP_OPCODE_REQUEST) {
		if(ntohl(rx_arp->target_ip) == my_ip) {
			int i;

			fill_eth_header(&txbuffer->frame.eth_header,
				rx_arp->sender_mac,
				my_mac,
				ETHERTYPE_ARP);
			txlen = ARP_PACKET_LENGTH;
			tx_arp->hwtype = htons(ARP_HWTYPE_ETHERNET);
			tx_arp->proto = htons(ARP_PROTO_IP);
			tx_arp->hwsize = 6;
			tx_arp->protosize = 4;
			tx_arp->opcode = htons(ARP_OPCODE_REPLY);
			tx_arp->sender_ip = htonl(my_ip);
			for(i=0;i<6;i++)
				tx_arp->sender_mac[i] = my_mac[i];
			tx_arp->target_ip = htonl(ntohl(rx_arp->sender_ip));
			for(i=0;i<6;i++)
				tx_arp->target_mac[i] = rx_arp->sender_mac[i];
			send_packet();
		}
		return;
	}
}

static const unsigned char broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

int microudp_arp_resolve(unsigned int ip)
{
	struct arp_frame *arp;
	int i;
	int tries;
	int timeout;

	if(cached_ip == ip) {
		for(i=0;i<6;i++)
			if(cached_mac[i]) return 1;
	}
	cached_ip = ip;
	for(i=0;i<6;i++)
		cached_mac[i] = 0;

	for(tries=0;tries<100;tries++) {
		/* Send an ARP request */
		fill_eth_header(&txbuffer->frame.eth_header,
				broadcast,
				my_mac,
				ETHERTYPE_ARP);
		txlen = ARP_PACKET_LENGTH;
		arp = &txbuffer->frame.contents.arp;
		arp->hwtype = htons(ARP_HWTYPE_ETHERNET);
		arp->proto = htons(ARP_PROTO_IP);
		arp->hwsize = 6;
		arp->protosize = 4;
		arp->opcode = htons(ARP_OPCODE_REQUEST);
		arp->sender_ip = htonl(my_ip);
		for(i=0;i<6;i++)
			arp->sender_mac[i] = my_mac[i];
		arp->target_ip = htonl(ip);
		for(i=0;i<6;i++)
			arp->target_mac[i] = 0;

		send_packet();

		/* Do we get a reply ? */
		for(timeout=0;timeout<100000;timeout++) {
			microudp_service();
			for(i=0;i<6;i++)
				if(cached_mac[i]) return 1;
		}
	}

	return 0;
}

static unsigned short ip_checksum(unsigned int r, void *buffer, unsigned int length, int complete)
{
	unsigned char *ptr;
	unsigned int i;

	ptr = (unsigned char *)buffer;
	length >>= 1;

	for(i=0;i<length;i++)
		r += ((unsigned int)(ptr[2*i]) << 8)|(unsigned int)(ptr[2*i+1]) ;

	/* Add overflows */
	while(r >> 16)
		r = (r & 0xffff) + (r >> 16);

	if(complete) {
		r = ~r;
		r &= 0xffff;
		if(r == 0) r = 0xffff;
	}
	return r;
}

void *microudp_get_tx_buffer(void)
{
	return txbuffer->frame.contents.udp.payload;
}

struct pseudo_header {
	unsigned int src_ip;
	unsigned int dst_ip;
	unsigned char zero;
	unsigned char proto;
	unsigned short length;
} __attribute__((packed));

int microudp_send(unsigned short src_port, unsigned short dst_port, unsigned int length)
{
	struct pseudo_header h;
	unsigned int r;

	if((cached_mac[0] == 0) && (cached_mac[1] == 0) && (cached_mac[2] == 0)
		&& (cached_mac[3] == 0) && (cached_mac[4] == 0) && (cached_mac[5] == 0))
		return 0;

	txlen = length + sizeof(struct ethernet_header) + sizeof(struct udp_frame);
	if(txlen < ARP_PACKET_LENGTH) txlen = ARP_PACKET_LENGTH;

	fill_eth_header(&txbuffer->frame.eth_header,
		cached_mac,
		my_mac,
		ETHERTYPE_IP);

	txbuffer->frame.contents.udp.ip.version = IP_IPV4;
	txbuffer->frame.contents.udp.ip.diff_services = 0;
	txbuffer->frame.contents.udp.ip.total_length = htons(length + sizeof(struct udp_frame));
	txbuffer->frame.contents.udp.ip.identification = htons(0);
	txbuffer->frame.contents.udp.ip.fragment_offset = htons(IP_DONT_FRAGMENT);
	txbuffer->frame.contents.udp.ip.ttl = IP_TTL;
	h.proto = txbuffer->frame.contents.udp.ip.proto = IP_PROTO_UDP;
	txbuffer->frame.contents.udp.ip.checksum = 0;
	h.src_ip = txbuffer->frame.contents.udp.ip.src_ip = htonl(my_ip);
	h.dst_ip = txbuffer->frame.contents.udp.ip.dst_ip = htonl(cached_ip);
	txbuffer->frame.contents.udp.ip.checksum = htons(ip_checksum(0, &txbuffer->frame.contents.udp.ip,
		sizeof(struct ip_header), 1));

	txbuffer->frame.contents.udp.udp.src_port = htons(src_port);
	txbuffer->frame.contents.udp.udp.dst_port = htons(dst_port);
	h.length = txbuffer->frame.contents.udp.udp.length = htons(length + sizeof(struct udp_header));
	txbuffer->frame.contents.udp.udp.checksum = 0;

	h.zero = 0;
	r = ip_checksum(0, &h, sizeof(struct pseudo_header), 0);
	if(length & 1) {
		txbuffer->frame.contents.udp.payload[length] = 0;
		length++;
	}
	r = ip_checksum(r, &txbuffer->frame.contents.udp.udp,
		sizeof(struct udp_header)+length, 1);
	txbuffer->frame.contents.udp.udp.checksum = htons(r);

	send_packet();

	return 1;
}

int microicmp_reply(unsigned short id, unsigned short seq, char *stuff, unsigned short length) {
  struct pseudo_header h; // compiler emits warning about this, but we need this variable!
  unsigned int r;
  int i;

  if((cached_mac[0] == 0) && (cached_mac[1] == 0) && (cached_mac[2] == 0)
     && (cached_mac[3] == 0) && (cached_mac[4] == 0) && (cached_mac[5] == 0))
    return 0;
  
  txlen = sizeof(struct ethernet_header) + sizeof(struct icmp_frame) + length;

  fill_eth_header(&txbuffer->frame.eth_header,
		  cached_mac,
		  my_mac,
		  ETHERTYPE_IP);

  txbuffer->frame.contents.icmp.ip.version = IP_IPV4;
  txbuffer->frame.contents.icmp.ip.diff_services = 0;
  txbuffer->frame.contents.icmp.ip.total_length = htons(length + sizeof(struct icmp_frame));
  txbuffer->frame.contents.icmp.ip.identification = htons(0);
  txbuffer->frame.contents.icmp.ip.fragment_offset = htons(IP_DONT_FRAGMENT);
  txbuffer->frame.contents.icmp.ip.ttl = IP_TTL;
  h.proto = txbuffer->frame.contents.icmp.ip.proto = IP_PROTO_ICMP;
  txbuffer->frame.contents.icmp.ip.checksum = 0;
  h.src_ip = txbuffer->frame.contents.icmp.ip.src_ip = htonl(my_ip);
  h.dst_ip = txbuffer->frame.contents.icmp.ip.dst_ip = htonl(cached_ip);
  txbuffer->frame.contents.icmp.ip.checksum = htons(ip_checksum(0, &txbuffer->frame.contents.icmp.ip,
							       sizeof(struct ip_header), 1));

  // fill in frame contents
  txbuffer->frame.contents.icmp.icmp.type = ICMP_ECHOREPLY;
  txbuffer->frame.contents.icmp.icmp.code = 0;
  txbuffer->frame.contents.icmp.icmp.checksum = 0;
  txbuffer->frame.contents.icmp.icmp.un.echo.id = htons(id);
  txbuffer->frame.contents.icmp.icmp.un.echo.sequence = htons(seq);

  for( i = 0; i < length; i++ ) {
    txbuffer->frame.contents.icmp.payload[i] = stuff[i];
  }
  
  r = ip_checksum(0, &txbuffer->frame.contents.icmp.icmp,
		  sizeof(struct icmp_header)+length, 1);
  txbuffer->frame.contents.icmp.icmp.checksum = htons(r);

  send_packet();
  
  return 1;
}

static udp_callback rx_callback;

// returns 0 if we can process
static int process_ip(void)
{
	if(rxlen < (sizeof(struct ethernet_header)+sizeof(struct udp_frame))) return 1;
	struct udp_frame *udp_ip = &rxbuffer->frame.contents.udp;
	/* We don't verify UDP and IP checksums and rely on the Ethernet checksum solely */
	if(udp_ip->ip.version != IP_IPV4) return 1;
	// check disabled for QEMU compatibility
	//if(rxbuffer->frame.contents.udp.ip.diff_services != 0) return;

	if(udp_ip->ip.proto == IP_PROTO_ICMP) {
	  struct icmp_frame *icmp_ip = &rxbuffer->frame.contents.icmp;
	  if( (icmp_ip->icmp.type == ICMP_ECHO) || (icmp_ip->icmp.type == ICMP_TIMESTAMP) ) {
	    microicmp_reply(ntohs(icmp_ip->icmp.un.echo.id), ntohs(icmp_ip->icmp.un.echo.sequence),
			    icmp_ip->payload, ntohs(icmp_ip->ip.total_length) - ICMP_OVERHEAD);
	  }
	  return 0;
	} else if( udp_ip->ip.proto == IP_PROTO_TCP ) {
	  return 1; // can't process TCP frames in this function
	} else {
	  if(ntohs(udp_ip->ip.total_length) < sizeof(struct udp_frame)) return 1;
	  // check disabled for QEMU compatibility
	  //if(ntohs(rxbuffer->frame.contents.udp.ip.fragment_offset) != IP_DONT_FRAGMENT) return;
	  if(udp_ip->ip.proto != IP_PROTO_UDP) return 1;
	  if(ntohl(udp_ip->ip.dst_ip) != my_ip) return 1;
	  if(ntohs(udp_ip->udp.dst_port) != TFTP_PORT_IN) return 1; // check that it's for TFTP, this is all we handle
	  if(ntohs(udp_ip->udp.length) < sizeof(struct udp_header)) return 1;

	  if(rx_callback) {
	    rx_callback(ntohl(udp_ip->ip.src_ip), ntohs(udp_ip->udp.src_port),
			ntohs(udp_ip->udp.dst_port), udp_ip->payload,
			ntohs(udp_ip->udp.length)-sizeof(struct udp_header));
	  } else return 1;
	  return 0;
	}
	// if we got here, we couldn't process the packet
	return 1;
}

void microudp_set_callback(udp_callback callback)
{
	rx_callback = callback;
}

// returns 0 if frame can be processed by this function
static int process_frame(void)
{
  //	flush_cpu_dcache();
#ifdef DEBUG_MICROUDP_RX
	int j;
	printf("<<< rxlen : %d\n", rxlen);
	for(j=0;j<rxlen;j++) {
	  if( (j % 8) == 0 )
	    printf(" ");
	  printf("%02x", rxbuffer->raw[j]);
	}
	printf("\n");
#endif

#ifndef HW_PREAMBLE_CRC
	int i;
	for(i=0;i<7;i++)
		if(rxbuffer->frame.eth_header.preamble[i] != 0x55) return;
	if(rxbuffer->frame.eth_header.preamble[7] != 0xd5) return;
#endif

#ifndef HW_PREAMBLE_CRC
	unsigned int received_crc;
	unsigned int computed_crc;
	received_crc = ((unsigned int)rxbuffer->raw[rxlen-1] << 24)
		|((unsigned int)rxbuffer->raw[rxlen-2] << 16)
		|((unsigned int)rxbuffer->raw[rxlen-3] <<  8)
		|((unsigned int)rxbuffer->raw[rxlen-4]);
	computed_crc = crc32(&rxbuffer->raw[8], rxlen-12);
	if(received_crc != computed_crc) return;

	rxlen -= 4; /* strip CRC here to be consistent with TX */
#endif

#ifdef LIBUIP	
	if((ntohs(rxbuffer->frame.eth_header.ethertype) == ETHERTYPE_ARP) &&
	   (arp_mode == ARP_MICROUDP)) {
#else
	  if((ntohs(rxbuffer->frame.eth_header.ethertype) == ETHERTYPE_ARP)) {
#endif
	  process_arp();
	  return 0;
	}
	else if(ntohs(rxbuffer->frame.eth_header.ethertype) == ETHERTYPE_IP) {
	  return(process_ip());
	}
	return 1;
}

void microudp_start(const unsigned char *macaddr, unsigned char ip0, unsigned char ip1,
		    unsigned char ip2, unsigned char ip3)
{
	int i;
	unsigned int ip = IPTOINT(ip0, ip1, ip2, ip3);
	
	ethmac_sram_reader_ev_pending_write(ETHMAC_EV_SRAM_READER);
	ethmac_sram_writer_ev_pending_write(ETHMAC_EV_SRAM_WRITER);

	for(i=0;i<6;i++)
		my_mac[i] = macaddr[i];
	my_ip = ip;

	cached_ip = 0;
	for(i=0;i<6;i++)
		cached_mac[i] = 0;

	txslot = 0;
	ethmac_sram_reader_slot_write(txslot);
	txbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * (ETHMAC_RX_SLOTS + txslot));

	rxslot = 0;
	rxbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * rxslot);
	rx_callback = (udp_callback)0;

#ifdef LIBUIP	
	uip_ipaddr_t ipaddr;
	
	rxbuffer0 = (ethernet_buffer *)(ETHMAC_BASE + 0*ETHMAC_SLOT_SIZE);
	rxbuffer1 = (ethernet_buffer *)(ETHMAC_BASE + 1*ETHMAC_SLOT_SIZE);
	txbuffer0 = (ethernet_buffer *)(ETHMAC_BASE + 2*ETHMAC_SLOT_SIZE);
	txbuffer1 = (ethernet_buffer *)(ETHMAC_BASE + 3*ETHMAC_SLOT_SIZE);
	
	/* uip periods */
	uip_periodic_period = CONFIG_CLOCK_FREQUENCY/100; /*  10 ms */
	uip_arp_period = CONFIG_CLOCK_FREQUENCY/10;       /* 100 ms */

	/* init uip */
	process_init();
	process_start(&etimer_process, NULL);
	uip_init();

	/* configure mac / ip */
	for (i=0; i<6; i++) uip_lladdr.addr[i] = macaddr[i];
	uip_ipaddr(&ipaddr, ip0, ip1, ip2, ip3);
	uip_sethostaddr(&ipaddr);
#endif
	printf("Microudp init done: my IP is %d.%d.%d.%d\n", ip0, ip1, ip2, ip3);
	
}

void microudp_service(void)
{
#ifdef LIBUIP
	int i;
	struct uip_eth_hdr *buf = (struct uip_eth_hdr *)&uip_buf[0];

	etimer_request_poll();
	process_run();
#endif
	/* this is the heart of liteethmac_poll() */
	if(ethmac_sram_writer_ev_pending_read() & ETHMAC_EV_SRAM_WRITER) {
		rxslot = ethmac_sram_writer_slot_read();
		rxbuffer = (ethernet_buffer *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * rxslot);
		rxlen = ethmac_sram_writer_length_read();
#ifdef LIBUIP
		memcpy(uip_buf, rxbuffer, rxlen);
		uip_len = rxlen;
		if( process_frame() == 0 ) {
		  uip_len = 0; // bypass the remaining handler if microudp stack could handle it
		}
#else
		process_frame();
#endif
		
		ethmac_sram_writer_ev_pending_write(ETHMAC_EV_SRAM_WRITER);
	} else {
#ifdef LIBUIP
	  uip_len = 0;
#endif
	}
#ifdef LIBUIP
	if(uip_len > 0) {
#ifdef DEBUG_LIBUIP_RX
	  printf( " <<< uip_rx %d bytes: ", uip_len );
	  for( i = 0; i < uip_len; i++ ) {
	    if( (i % 8) == 0 ) printf(" ");
	    printf("%02x", ((unsigned char *)buf)[i]);
	  }
	  printf("\n");
	  printf("buf->type: %d, %d\n", buf->type, uip_htons(UIP_ETHTYPE_IP) );
#endif
		if(buf->type == uip_htons(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			if(uip_len > 0) {
				uip_arp_out();
				libuip_send();
			}
		} else if(buf->type == uip_htons(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if(uip_len > 0)
				libuip_send();
		}
	} else if (elapsed(&uip_periodic_event, uip_periodic_period)) {
		for(i = 0; i < UIP_CONNS; i++) {
			uip_periodic(i);

			if(uip_len > 0) {
				uip_arp_out();
				libuip_send();
			}
		}
	}
	if (elapsed(&uip_arp_event, uip_arp_period)) {
		uip_arp_timer();
	}
#endif
}

static void busy_wait(unsigned int ds)
{
	timer0_en_write(0);
	timer0_reload_write(0);
	timer0_load_write(CONFIG_CLOCK_FREQUENCY/10*ds);
	timer0_en_write(1);
	timer0_update_value_write(1);
	while(timer0_value_read()) timer0_update_value_write(1);
}

void eth_init(void)
{
#ifdef CSR_ETHPHY_CRG_RESET_ADDR
	ethphy_crg_reset_write(1);
	busy_wait(2);
	ethphy_crg_reset_write(0);
	busy_wait(2);
#endif
}

#ifdef CSR_ETHPHY_MODE_DETECTION_MODE_ADDR
void eth_mode(void)
{
	printf("Ethernet phy mode: ");
	if (ethphy_mode_detection_mode_read())
		printf("MII");
	else
		printf("GMII");
	printf("\n");
}
#endif

#endif
