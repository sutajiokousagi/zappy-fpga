#ifndef __MICROUDP_H
#define __MICROUDP_H

#define IPTOINT(a, b, c, d) ((a << 24)|(b << 16)|(c << 8)|d)

#define MICROUDP_BUFSIZE (5*1532)

typedef void (*udp_callback)(unsigned int src_ip, unsigned short src_port, unsigned short dst_port, void *data, unsigned int length);

void microudp_start(const unsigned char *macaddr, unsigned char ip0, unsigned char ip1,
		    unsigned char ip2, unsigned char ip3);
int microudp_arp_resolve(unsigned int ip);
void *microudp_get_tx_buffer(void);
int microudp_send(unsigned short src_port, unsigned short dst_port, unsigned int length);
void microudp_set_callback(udp_callback callback);
void microudp_service(void);
int microicmp_reply(unsigned short id, unsigned short seq, char *stuff, unsigned short length);

void eth_init(void);
void eth_mode(void);


#endif /* __MICROUDP_H */
