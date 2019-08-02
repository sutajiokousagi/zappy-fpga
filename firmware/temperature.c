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

#include "i2c.h"
#include "temperature.h"

typedef struct tempzone {
  int32_t  temperature;
  uint8_t   address;
  char name[8];
} tempzone;

#define NUM_TEMPZONES 5
tempzone tempzones[NUM_TEMPZONES] = {
  {0, 0x4c, "logic"},
  {0, 0x48, "dischrg"},
  {0, 0x49, "hvbleed"},
  {0, 0x4a, "plate"},
  {0, 0x4b, "adc"},
};

void max_temperature(int32_t *max, char *zone) {
  int i;

  *max = -1000;
  for( i = 0; i < NUM_TEMPZONES; i++ ) {
    if( tempzones[i].temperature > *max ) {
      *max = tempzones[i].temperature;
      strncpy(zone, tempzones[i].name, 8);
    }
  }
}

void print_temperature(void) {
  int i;
  uint32_t remainder;
  
  for( i = 0; i < NUM_TEMPZONES; i ++ ) {
    if( tempzones[i].temperature > 0 )
      remainder = tempzones[i].temperature % 10000;
    else
      remainder = -tempzones[i].temperature % 10000;
    printf( "Zone '%s': %d.%dC\n", tempzones[i].name, tempzones[i].temperature / 10000, remainder / 1000 );
  }
}

void update_temperature(void) {
  uint8_t tx[3];
  uint8_t rx[2];
  int ret = 0;
  int i;

  for( i = 0; i < NUM_TEMPZONES; i++ ) {
    tx[0] = 0x1; // pointer
    tx[1] = 0x60; // set 12 bits
    tx[2] = 0x0;
    ret = i2c_master(tempzones[i].address, tx, 3, NULL, 0, 50000000);
    
    tx[0] = 0x0; // pointer
    ret += i2c_master(tempzones[i].address, tx, 1, NULL, 0, 50000000);

    rx[0] = 0; rx[1] = 0;
    ret += i2c_master(tempzones[i].address, tx, 0, rx, 2, 50000000);
    if( ret > 0 )
      printf( "I2C calls returned %d errors\n", ret );

    int16_t inttemp = ((int16_t) rx[0] << 8) | (int16_t)rx[1];
    inttemp = inttemp >> 4;
    tempzones[i].temperature = ((int32_t) inttemp) * 625;
  }
  
}

