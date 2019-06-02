#ifndef __TFTP_H
#define __TFTP_H

#include <stdint.h>

/* Local TFTP client port (arbitrary) */
#define PORT_IN		7642
#define TFTP_PORT_IN    PORT_IN

int tftp_get(uint32_t ip, uint16_t server_port, const char *filename,
    void *buffer);
int tftp_put(uint32_t ip, uint16_t server_port, const char *filename,
    const void *buffer, int size);

#endif /* __TFTP_H */

