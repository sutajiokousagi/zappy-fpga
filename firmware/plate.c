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

#include "si1153.h"
#include "iqmotor.h"
#include "motor.h"
#include "plate.h"
#include "delay.h"
#include "ui.h"

#define UNLOCKED 0
#define LOCKED 1
#define ERROR 2

static float stopping_angle = 0.0;
static float home_angle = 0.0;
static uint8_t homed = 0;
static float coast_current = 0.0;
static plate_state pstate = platestate_unlocked;

plate_state get_platestate(void) {
  return pstate;
}

uint32_t plate_present(void) {
  if( getSensorData() > PROX_PRESENT_THRESH )
    return 1;
  else
    return 0;
}

uint32_t plate_home(void) {
  float cur_angle = 0.0;
  int i;

  iqSetCoast();
  coast_current = iqReadAmps();
  delay(100);
  coast_current = iqReadAmps();
  delay(100);
  
  cur_angle = iqReadAngle();
  i = 1;
  // open the dog
  while( zappio_l25_open_read() == 0 ) {
    iqSetAngle(6.28 * (float) i + cur_angle, 500);
    delay(600);
    i++;
  }

  delay(100);
  cur_angle = iqReadAngle();
  delay(100);
  cur_angle = iqReadAngle();
  // now slowly close it
  i = 1;
  while( zappio_l25_open_read() ) {
    iqSetAngle( cur_angle - (0.1 * (float) i), 10);
    delay(20);
    i++;
  }

  delay(100);
  home_angle = iqReadAngle();
  delay(100);
  home_angle = iqReadAngle();

  homed = 1;
  pstate = platestate_unlocked;

  return 1;
}

// lock the plate in place; returns 0 if fail, 1 if success
uint32_t plate_lock(void) {
  int i = 0;
  float motor_current = 0.0;
  
  telnet_tx = 1;
  if( !homed ) {
    plate_home();
    homed = 1;
  }
  
  if( plate_present() ) {
    while( (i < PROX_FULL_STROKE) && (zappio_noplate_read() != 0) ) {
      iqSetAngle(3.14 * (float) i + home_angle, PROX_CHECK_INTERVAL);
      delay(PROX_CHECK_INTERVAL);

      motor_current = iqReadAmps();
      // printf( "motor current: %dmA\n", motor_current * 1000.0 );
      if( motor_current > (MOTOR_JAM_CURRENT + coast_current) ) {
	snprintf(ui_notifications, sizeof(ui_notifications), "Lock: motor jam (%d)\n", i);
	printf( "Motor jam detected at rotation %d : zerr\n", i );
	iqSetAngle(home_angle, 1000);
	pstate = platestate_error;
	telnet_tx = 0;
	return 0;
      }
      i++;
    };

    snprintf(ui_notifications, sizeof(ui_notifications), "Lock: success (%d)", i);
    printf( "Plate locked at rotation count: %d : zpass\n", i );
    stopping_angle = iqReadAngle();
    
    if( i >= PROX_FULL_STROKE ) {
      snprintf(ui_notifications, sizeof(ui_notifications), "Lock: over-rotate (%d)", i);
      printf( "Over-rotation: plate may not be engaged fully or missing : zerr\n" );
      pstate = platestate_warning;
      telnet_tx = 0;
      return 0;
    } else {
      pstate = platestate_locked;
      telnet_tx = 0;
      return 1;
    }
  } else {
    printf( "Plate not present : zerr\n" );
    snprintf(ui_notifications, sizeof(ui_notifications), "Lock: plate not present", i);
    pstate = platestate_unlocked;
    telnet_tx = 0;
    return 0;
  }
}

uint32_t plate_unlock(void) {
  float motor_current = 0.0;
  float cur_angle = 0.0;
  
  telnet_tx = 1;
  if( !homed ) {
    plate_home();
    homed = 1;
  }
  
  iqSetAngle(home_angle, 1000);
  delay(1000);

  motor_current = iqReadAmps();
  if( motor_current > (MOTOR_JAM_CURRENT + coast_current) ) {
    buzzpwm_enable_write(1); // sound an alarm
    printf("unlock jam : zerr\n");
    snprintf(ui_notifications, sizeof(ui_notifications), "Unlock: JAM");
    pstate = platestate_error;
    telnet_tx = 0;
    return 0;
  }
  cur_angle = iqReadAngle();
  cur_angle = cur_angle - home_angle;
  if( cur_angle < 0.0 )
    cur_angle = -cur_angle;

  if( cur_angle > 1.57 ) { // more than half a rotation from the home angle
    buzzpwm_enable_write(1); // sound an alarm
    pstate = platestate_error;
    printf("homing fail : zerr\n");
    snprintf(ui_notifications, sizeof(ui_notifications), "Unlock: homing fail");
    telnet_tx = 0;
    return 0;
  }
  
  pstate = platestate_unlocked;
  snprintf(ui_notifications, sizeof(ui_notifications), "Unlock: success");
  printf( "unlock success : zpass\n" );
  telnet_tx = 0;
  return 1;
}


void do_plate(char *token) {
  if( strcmp(token, "test") == 0 ) {
    for(int i = 0; i < 100; i ++ ) {
      printf("plate: %02x\n", zappio_noplate_read());
      delay(100);
    }
  } else if( strcmp(token, "present") == 0 ) {
    if( getSensorData() > PROX_PRESENT_THRESH ) {
      printf("Plate: present\n");
    } else {
      printf("Plate: absent\n");
    }
  } else if( strcmp(token, "lock") == 0 ) {
    if(plate_lock()) {
      printf( "success\n" );
    } else {
      printf( "fail\n" );
    }
  } else if( strcmp(token, "unlock") == 0 ) {
    plate_unlock();
  } else if( strcmp(token, "home") == 0 ) {
    printf("Before homing: %d\n",(int) (home_angle * 1000.0) );
    plate_home();
    printf("After homing: %d\n",(int) (home_angle * 1000.0) );
  } else if( strcmp(token, "l25o") == 0 ) {
    for(int i = 0; i < 100; i++ ) {
      printf("l25 open: %d\n", zappio_l25_open_read()); // returns 1 when dog is not interrupting the sensor, 0 when dog interrupts
      delay(100);
    }
  } else {
    printf("Plate command %s unrecognized\n", token);
  }
}
