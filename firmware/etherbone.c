// This file is Copyright (c) 2015 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#include <generated/mem.h>

#ifdef ETHMAC_BASE

#include "etherbone.h"
#include "ethernet.h"

#define DEBUG_PRINTF(...) /* printf(__VA_ARGS__)  */

static unsigned char callback_buf[1512];
static unsigned int callback_buf_length;

void etherbone_init(void)
{
	callback_buf_length = 0;

	udp_socket_register(&etherbone_udp, NULL,
		(udp_socket_input_callback_t) etherbone_callback_udp);
	udp_socket_bind(&etherbone_udp, ETHERBONE_PORT);

	//// NOTE NOTE NOTE -- this "spoils" the uip stack for any other UDP responder
	tcpip_set_outputfunc(etherbone_output_func);
	printf("Etherbone listening on UDP port %d\n", ETHERBONE_PORT);
}

void etherbone_write(unsigned int addr, unsigned int value)
{
	unsigned int *addr_p = (unsigned int *)addr;
	*addr_p = value;
}

unsigned int etherbone_read(unsigned int addr)
{
	unsigned int value;
	unsigned int *addr_p = (unsigned int *)addr;
	value = *addr_p;
	return value;
}

int etherbone_callback(struct tcp_socket *s, void *ptr, const char *rxbuf, int rxlen)
{
	struct etherbone_packet *packet;
	unsigned char * callback_buf_ptr;

	memcpy(callback_buf + callback_buf_length, rxbuf, rxlen);
	callback_buf_length += rxlen;
	callback_buf_ptr = callback_buf;

	while(callback_buf_length > 0) {
		packet = (struct etherbone_packet *)(callback_buf_ptr);
		/* seek header */
		if(packet->magic != 0x4e6f) {
			callback_buf_ptr++;
			callback_buf_length--;
		/* found header */
		} else {
			/* enough bytes for header? */
			if(callback_buf_length > ETHERBONE_HEADER_LENGTH) {
				unsigned int packet_length;
				packet_length = ETHERBONE_HEADER_LENGTH;
				if(packet->record_hdr.wcount)
					packet_length += (1 + packet->record_hdr.wcount)*4;
				if(packet->record_hdr.rcount)
					packet_length += (1 + packet->record_hdr.rcount)*4;
				/* enough bytes for packet? */
				if(callback_buf_length >= packet_length) {
					etherbone_process(s, callback_buf_ptr);
					callback_buf_ptr += packet_length;
					callback_buf_length -= packet_length;
				} else {
					memmove(callback_buf, callback_buf_ptr, callback_buf_length);
					return 0;
				}
			} else {
				memmove(callback_buf, callback_buf_ptr, callback_buf_length);
				return 0;
			}
		}
	}
	return 0;
}

int etherbone_callback_udp(struct udp_socket *s, void *ptr, const uip_ipaddr_t *source_addr,
			   uint16_t source_port, const uip_ipaddr_t *dest_addr, uint16_t dest_port,
			   const uint8_t *data, uint16_t datalen)
{
	struct etherbone_packet *packet;
	unsigned char * callback_buf_ptr;

	memcpy(callback_buf + callback_buf_length, data, datalen);
	callback_buf_length += datalen;
	callback_buf_ptr = callback_buf;

	while(callback_buf_length > 0) {
		packet = (struct etherbone_packet *)(callback_buf_ptr);
		int i;
		DEBUG_PRINTF( "etherbone packet:\n ");
		for( i = 0; i < sizeof(struct etherbone_packet); i++ ) {
		  DEBUG_PRINTF( "%02x ", callback_buf_ptr[i] );
		}
		/* seek header */
		if(uip_ntohs(packet->magic) != 0x4e6f) {
			callback_buf_ptr++;
			callback_buf_length--;
		/* found header */
		} else {
			/* enough bytes for header? */
			if(callback_buf_length > ETHERBONE_HEADER_LENGTH) {
				unsigned int packet_length;
				packet_length = ETHERBONE_HEADER_LENGTH;
				if(packet->record_hdr.wcount)
					packet_length += (1 + packet->record_hdr.wcount)*4;
				if(packet->record_hdr.rcount)
					packet_length += (1 + packet->record_hdr.rcount)*4;
				/* enough bytes for packet? */
				if(callback_buf_length >= packet_length) {
				  //printf( "source_addr: %x\n", *source_addr );
				  etherbone_process_udp(s, callback_buf_ptr, *source_addr, ETHERBONE_PORT);
					callback_buf_ptr += packet_length;
					callback_buf_length -= packet_length;
				} else {
					memmove(callback_buf, callback_buf_ptr, callback_buf_length);
					return 0;
				}
			} else {
				memmove(callback_buf, callback_buf_ptr, callback_buf_length);
				return 0;
			}
		}
	}
	return 0;
}

void etherbone_process(struct tcp_socket *s, unsigned char *rxbuf)
{
	struct etherbone_packet *rx_packet = (struct etherbone_packet *)rxbuf;
	struct etherbone_packet *tx_packet = (struct etherbone_packet *)etherbone_tx_buf;
	unsigned int i;
	unsigned int addr;
	unsigned int data;
	unsigned int rcount, wcount;

	if(rx_packet->magic != 0x4e6f) return;   /* magic */
	if(rx_packet->addr_size != 4) return;    /* 32 bits address */
	if(rx_packet->port_size != 4) return;    /* 32 bits data */

	rcount = rx_packet->record_hdr.rcount;
	wcount = rx_packet->record_hdr.wcount;

	if(wcount > 0) {
		addr = rx_packet->record_hdr.base_write_addr;
		for(i=0;i<wcount;i++) {
			data = rx_packet->record[i].write_value;
			etherbone_write(addr, data);
			addr += 4;
		}
	}
	if(rcount > 0) {
		for(i=0;i<rcount;i++) {
			addr = rx_packet->record[i].read_addr;
			data = etherbone_read(addr);
			tx_packet->record[i].write_value = data;
		}
		tx_packet->magic = 0x4e6f;
		tx_packet->version = 1;
		tx_packet->nr = 1;
		tx_packet->pr = 0;
		tx_packet->pf = 0;
		tx_packet->addr_size = 4; // 32 bits
		tx_packet->port_size = 4; // 32 bits
		tx_packet->record_hdr.wcount = rcount;
		tx_packet->record_hdr.rcount = 0;
		tx_packet->record_hdr.base_write_addr = rx_packet->record_hdr.base_ret_addr;
		tcp_socket_send(&etherbone_socket,
						etherbone_tx_buf,
						sizeof(*tx_packet) + rcount*sizeof(struct etherbone_record));
	}

	return;
}

static void dump_packet(struct etherbone_packet *p) {
  DEBUG_PRINTF("magic %02x\n", p->magic);
  DEBUG_PRINTF("version %02x\n", p->version);
  DEBUG_PRINTF("reserved %02x\n", p->reserved);
  DEBUG_PRINTF("nr %02x\n", p->nr);
  DEBUG_PRINTF("pr %02x\n", p->pr);
  DEBUG_PRINTF("pf %02x\n", p->pf);
  DEBUG_PRINTF("addr_size %02x\n", p->addr_size);
  DEBUG_PRINTF("port_size %02x\n", p->port_size);
  DEBUG_PRINTF("padding %02x\n", p->padding);
  DEBUG_PRINTF("pr %02x\n", p->pr);
  DEBUG_PRINTF("rh bca %02x\n", p->record_hdr.bca);
  DEBUG_PRINTF("rh rca %02x\n", p->record_hdr.rca);
  DEBUG_PRINTF("rh rff %02x\n", p->record_hdr.rff);
  DEBUG_PRINTF("rh rsvd %02x\n", p->record_hdr.reserved);
  DEBUG_PRINTF("rh cyc %02x\n", p->record_hdr.cyc);
  DEBUG_PRINTF("rh wca %02x\n", p->record_hdr.wca);
  DEBUG_PRINTF("rh wff %02x\n", p->record_hdr.wff);
  DEBUG_PRINTF("rh rsvd2 %02x\n", p->record_hdr.reserved2);
  DEBUG_PRINTF("rh byte enable %02x\n", p->record_hdr.byte_enable);
  DEBUG_PRINTF("rh wcount %02x\n", p->record_hdr.wcount);
  DEBUG_PRINTF("rh rcount %02x\n", p->record_hdr.rcount);
  DEBUG_PRINTF("rh base_addr %08x\n", p->record_hdr.base_write_addr);
  DEBUG_PRINTF("rh record %08x\n", p->record[0].read_addr);
}

void send_packet_etherbone(unsigned char *raw, int rawlen);
int etherbone_len;
uint8_t etherbone_output_func(void) {
  int i;
  DEBUG_PRINTF( "etherbone response packet:\n ");
  for( i = 0; i < etherbone_len + 14; i++ ) {
    DEBUG_PRINTF( "%02x ", uip_buf[i] );
  }
  DEBUG_PRINTF("\n");
  send_packet_etherbone(uip_buf, etherbone_len + 14);

  return 0;
}

void etherbone_process_udp(struct udp_socket *s, unsigned char *rxbuf, const uip_ipaddr_t source_addr, uint16_t source_port)
{
	struct etherbone_packet *rx_packet = (struct etherbone_packet *)rxbuf;
	struct etherbone_packet *tx_packet = (struct etherbone_packet *)etherbone_tx_buf;
	unsigned int i;
	unsigned int addr;
	unsigned int data;
	unsigned int rcount, wcount;
	
	struct udp_socket tx_socket;
	struct uip_udp_conn tx_udp_conn;
	tx_socket.udp_conn = &tx_udp_conn;

	DEBUG_PRINTF("rx_packet:\n");
	dump_packet( rx_packet );
	if(uip_ntohs(rx_packet->magic) != 0x4e6f) return;   /* magic */
	if(rx_packet->addr_size != 4) return;    /* 32 bits address */
	if(rx_packet->port_size != 4) return;    /* 32 bits data */

	rcount = rx_packet->record_hdr.rcount;
	wcount = rx_packet->record_hdr.wcount;

	DEBUG_PRINTF("rcount %d, wcount %d\n", rcount, wcount);
	if(wcount > 0) {
	  addr = uip_ntohl(rx_packet->record_hdr.base_write_addr);
	  for(i=0;i<wcount;i++) {
	    data = uip_ntohl(rx_packet->record[i].write_value);
	    DEBUG_PRINTF("ETHERBONE: write addr %08x <- data %08x\n", addr, data);
	    etherbone_write(addr, data);
	    addr += 4;
	  }
	}
	if(rcount > 0) {
	  for(i=0;i<rcount;i++) {
	    addr = uip_ntohl(rx_packet->record[i].read_addr);
	    data = etherbone_read(addr);
	    DEBUG_PRINTF("ETHERBONE: read addr %08x -> data %08x\n", addr, data);
	    tx_packet->record[i].read_addr = uip_htonl(data);
	  }
	  tx_packet->magic = uip_htons(0x4e6f);
	  tx_packet->version = 1;
	  tx_packet->nr = 1;
	  tx_packet->pr = 0;
	  tx_packet->pf = 0;
	  tx_packet->addr_size = 4; // 32 bits
	  tx_packet->port_size = 4; // 32 bits
	  tx_packet->record_hdr.byte_enable = rx_packet->record_hdr.byte_enable;
	  tx_packet->record_hdr.wcount = rcount;
	  tx_packet->record_hdr.rcount = 0;
	  tx_packet->record_hdr.base_write_addr = uip_htonl(rx_packet->record_hdr.base_ret_addr);
	  ((unsigned char *)tx_packet)[2] = 0x14;
	  DEBUG_PRINTF("tx_packet:\n");
	  dump_packet( tx_packet );
	  //printf( "source_addr: %x\n", source_addr );
	  tx_socket.udp_conn->ripaddr = source_addr;
	  tx_socket.udp_conn->rport = uip_htons(source_port);
	  tx_socket.udp_conn->ttl = s->udp_conn->ttl;
	  //printf( "ripaddr: %x\n", tx_socket.udp_conn->ripaddr );
	  DEBUG_PRINTF( "ripaddr: %x\n", tx_socket.udp_conn->ripaddr );
	  DEBUG_PRINTF( "lport: %d\n", tx_socket.udp_conn->lport );
	  DEBUG_PRINTF( "rport: %d\n", tx_socket.udp_conn->rport );
	  DEBUG_PRINTF( "ttl: %d\n", tx_socket.udp_conn->ttl );
	  udp_socket_send(&tx_socket,
			  tx_packet,
			  sizeof(*tx_packet) + rcount*sizeof(struct etherbone_record));
	}

	return;
}

#endif
