#ifndef __I2C_H
#define __I2C_H

#define I2C_CMD_MASK_STA  (1 << 7)
#define I2C_CMD_MASK_STO  (1 << 6)
#define I2C_CMD_MASK_RD   (1 << 5)
#define I2C_CMD_MASK_WR   (1 << 4)
#define I2C_CMD_MASK_ACK  (1 << 3)
#define I2C_CMD_MASK_IACK (1 << 0)

#define I2C_STAT_MASK_RXACK (1 << 7)
#define I2C_STAT_MASK_BUSY  (1 << 6)
#define I2C_STAT_MASK_AL    (1 << 5)
#define I2C_STAT_MASK_TIP   (1 << 1)
#define I2C_STAT_MASK_IF    (1 << 0)

#define I2C_CTL_MASK_EN  (1 << 7)
#define I2C_CTL_MASK_IEN (1 << 6)

int i2c_init(void);
int i2c_master(unsigned char addr, uint8_t *txbuf, int txbytes, uint8_t *rxbuf, int rxbytes, unsigned int timeout);

#endif /* __I2C_H */
