#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <time.h>
#include <generated/csr.h>
#include <generated/mem.h>
#include <hw/flags.h>
#include <console.h>
#include <system.h>

#include <errno.h>
#include <string.h>
#include <math.h>

#include <time.h>

//#include <linux/limits.h>

#include "iqmotor.h"
#include "../motor.h"

static struct iqMotor motor_storage;
static struct iqMotor *motor;
static struct CommInterface_storage iq_com;
static struct mta_object iq_mta;

size_t write(const void *buf, size_t count);
size_t read(const void *buf, size_t count);

void iqCreateMotor(void) {
  motor = &motor_storage;
  motor->iq_com = &iq_com;

  CommInterface_init(motor->iq_com);
  
  motor->mta = &iq_mta;
  mta_init(motor->mta, motor->iq_com, 0);
}


 int iqSetCoast( void ) {
  // This buffer is for passing around messages.
  uint8_t communication_buffer_in[IQ_BUFLEN];
  // Stores length of message to send or receive
  uint8_t communication_length_in;

  mta_set(motor->mta, kSubCtrlCoast);  //  motor->mta_client->ctrl_coast_.set(*(motor->iq_com)); // put the input controller in "coast" mode
  // Grab outbound messages in the com queue, store into buffer
  // If it transferred something to communication_buffer...
  if(CommInterface_GetTxBytes(motor->iq_com, communication_buffer_in, &communication_length_in)) {
    write(communication_buffer_in, communication_length_in);
    return 0;
  } else {
    return 1;
  }
}


 double iqReadAngle( void ) {
  // This buffer is for passing around messages.
  uint8_t communication_buffer_in[IQ_BUFLEN];
  uint8_t communication_buffer_out[IQ_BUFLEN];
  // Stores length of message to send or receive
  uint8_t communication_length_in;
  uint8_t communication_length_out;

  float angle;

  ///////////// READ THE INPUT CONTROLLER
  // Generate the set messages
  mta_get(motor->mta, kSubObsAngularDisplacement); //  motor->mta_client->obs_angular_displacement_.get(*(motor->iq_com)); // get the angular displacement

  // Grab outbound messages in the com queue, store into buffer
  // If it transferred something to communication_buffer...
  if(CommInterface_GetTxBytes(motor->iq_com, communication_buffer_in, &communication_length_in)) {
    write(communication_buffer_in, communication_length_in);
  } else {
    return -1; // should be NAN...
  }
  
  delay(1); // delay 1ms for serial data to transmit data...
  
  // Reads however many bytes are currently available
  communication_length_in = read(communication_buffer_in, IQ_BUFLEN);
  
  // Puts the recently read bytes into com's receive queue
  CommInterface_SetRxBytes(motor->iq_com, communication_buffer_in, communication_length_in);
  
  uint8_t *rx_data; // temporary pointer to received type+data bytes
  uint8_t rx_length; // number of received type+data bytes
  // while we have message packets to parse
  while(CommInterface_PeekPacket(motor->iq_com, &rx_data, &rx_length)) {
    // Share that packet with all client objects
    //    motor->mta_client->ReadMsg(*(motor->iq_com), rx_data, rx_length);
    CommInterface_ReadMsg(motor->mta, rx_data, rx_length);
    
    // Once we're done with the message packet, drop it
    CommInterface_DropPacket(motor->iq_com);
  }

  //  if( motor->mta_client->obs_angular_displacement_.IsFresh() ) {
  mta_get_reply(motor->mta);
  angle = motor->mta->data.data.f; // we could dispatch on type, but in this case, we "just know"
  //  }
    
  return angle;
}

 void iqSetAngle( double target_angle, unsigned long travel_time_ms ) {
  // This buffer is for passing around messages.
  uint8_t communication_buffer_in[IQ_BUFLEN];
  uint8_t communication_buffer_out[IQ_BUFLEN];
  // Stores length of message to send or receive
  uint8_t communication_length_in;
  uint8_t communication_length_out;

  /////////////// WRITE OUTPUT CONTROLLER
  motor->mta->data.data.c = 6;
  mta_set(motor->mta, kSubCtrlMode);
  // motor->mta_client->ctrl_mode_.set(*(motor->iq_com), 6); // put the input controller in "coast" mode
  
  // Generate the set messages
  motor->mta->data.data.f = (float) target_angle;
  mta_set(motor->mta, kSubTrajectoryAngularDisplacement); //  motor->mta_client->trajectory_angular_displacement_.set(*(motor->iq_com), (float) target_angle);
  motor->mta->data.data.f = (float) travel_time_ms / 1000.0;
  mta_set(motor->mta, kSubTrajectoryDuration); //  motor->mta_client->trajectory_duration_.set(*(motor->iq_com), (float) travel_time_ms / 1000.0 ); 

  mta_get(motor->mta, kSubObsAngularDisplacement);
  // motor->mta_client->obs_angular_displacement_.get(*(motor->iq_com));
  
  // Grab outbound messages in the com queue, store into buffer
  // If it transferred something to communication_buffer...
  if(CommInterface_GetTxBytes(motor->iq_com, communication_buffer_out, &communication_length_out)) {
    write(communication_buffer_out, communication_length_out);
  }
}

void iqSetAngleDelta( double target_angle_delta, unsigned long travel_time_ms ) {
  double cur_angle;

  cur_angle = iqReadAngle();
  iqSetAngle(cur_angle + target_angle_delta, travel_time_ms );
}

size_t write(const void *buf, size_t count) {
  int i = 0;
  char *wbuf = (char *) buf;
  for( i = 0; i < count; i++ ) {
    motor_write(wbuf[i]);
  }

  motor_sync();
  return count;
}

size_t read(const void *buf, size_t count) {
  int i = 0;
  char *rbuf = (char *)buf;

  // TODO: add timeout mechanism
  for( i = 0; i < count; i++ ) {
    rbuf[i] = motor_read();
  }
  return count;
}

