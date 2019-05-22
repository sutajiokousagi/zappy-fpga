#include <time.h>
#include <generated/csr.h>
#include <generated/mem.h>
#include <hw/flags.h>
#include <console.h>
#include <system.h>
#include <stdio.h>

#include "i2c.h"

static int i2c_tip_wait(int timeout);

int i2c_init(void) {
  // set prescaler
  i2c_prescale_write(199);  // 100MHz / (5* 100kHz) - 1 = 199

  i2c_control_write(I2C_CTL_MASK_EN); // enable the I2C unit

  return 0;
}

static int i2c_tip_wait(int timeout) {
  int init_time;
  int timer;

  elapsed(&init_time, -1); // initialize timer
  timer = init_time;
  
  // wait for TIP to go high
  while( !(i2c_status_read() & I2C_STAT_MASK_TIP) ) {
    if( elapsed(&timer, timeout) ) {
      printf("TIP did not go high\n");
      i2c_command_write(0);
      return 1;
    }
    timer = init_time;
  }

  elapsed(&init_time, -1); // initialize timer
  timer = init_time;
  // wait for TIP to go low
  while( i2c_status_read() & I2C_STAT_MASK_TIP ) {
    if( elapsed(&timer, timeout) ) {
      printf("TIP did not go low\n");
      i2c_command_write(0);
      return 1;
    }
    timer = init_time;
  }

  i2c_command_write(0);
  return 0;
}

// returns 0 if good
// timeout is in SYSCLK cycles
int i2c_master(unsigned char addr, uint8_t *txbuf, int txbytes, uint8_t *rxbuf, int rxbytes, unsigned int timeout) {
  int i;
  int ret = 0;

  /// write half
  if( (txbytes > 0) && (txbuf != NULL) ) {
    i2c_txr_write( addr << 1 | 0 ); // LSB 0 = writing
    i2c_command_write( I2C_CMD_MASK_STA | I2C_CMD_MASK_WR );

    ret += i2c_tip_wait(timeout);

    i = 0;
    while( i < txbytes ) {
      if( i2c_status_read() & I2C_STAT_MASK_RXACK ) {
	printf( "Tx fail on byte %d\n", i );
	ret = 1;
      }
      i2c_txr_write( txbuf[i] );
      if( (i == (txbytes - 1)) && (rxbytes == 0 || rxbuf == NULL) )
	i2c_command_write( I2C_CMD_MASK_WR | I2C_CMD_MASK_STO );
      else
	i2c_command_write( I2C_CMD_MASK_WR );
      ret += i2c_tip_wait(timeout);
      i++;
    }
    
    if( i2c_status_read() & I2C_STAT_MASK_RXACK ) {
      printf( "Tx fail on byte %d\n", i );
      ret = 1;
    }
  }

  /// read half
  if( rxbytes == 0 || rxbuf == NULL )
    return ret;
  
  i2c_txr_write( addr << 1 | 1 ); // LSB 1 = reading
  i2c_command_write( I2C_CMD_MASK_STA | I2C_CMD_MASK_WR );
  ret += i2c_tip_wait(timeout);

  i = 0;
  while( i < rxbytes ) {
    if( i == (rxbytes - 1) )
      i2c_command_write( I2C_CMD_MASK_RD | I2C_CMD_MASK_ACK | I2C_CMD_MASK_STO );
    else
      i2c_command_write( I2C_CMD_MASK_RD );
    
    ret += i2c_tip_wait(timeout);
    
    rxbuf[i] = i2c_rxr_read();
    i++;
  }

  return ret;
}
