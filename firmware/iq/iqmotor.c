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
#include "motor.h"

static struct iqMotor motor_storage;
static struct iqMotor *motor;

size_t write(int fd, const void *buf, size_t count);
size_t read(int fd, const void *buf, size_t count);

EXTERNC void iqCreateMotor(void) {
  motor = &motor_storage;
  
  motor->iq_com = new GenericInterface();
  motor->mta_client = new MultiTurnAngleControlClient(0);

  motor->fd = 0;
}


EXTERNC int iqSetCoast( void ) {
  // This buffer is for passing around messages.
  uint8_t communication_buffer_in[IQ_BUFLEN];
  // Stores length of message to send or receive
  uint8_t communication_length_in;

  motor->mta_client->ctrl_coast_.set(*(motor->iq_com)); // put the input controller in "coast" mode
  // Grab outbound messages in the com queue, store into buffer
  // If it transferred something to communication_buffer...
  if(motor->iq_com->GetTxBytes(communication_buffer_in, communication_length_in)) {
    write(motor->fd, communication_buffer_in, communication_length_in);
    return 0;
  } else {
    return 1;
  }
}


EXTERNC double iqReadAngle( void ) {
  // This buffer is for passing around messages.
  uint8_t communication_buffer_in[IQ_BUFLEN];
  uint8_t communication_buffer_out[IQ_BUFLEN];
  // Stores length of message to send or receive
  uint8_t communication_length_in;
  uint8_t communication_length_out;

  float angle;

  ///////////// READ THE INPUT CONTROLLER
  // Generate the set messages
  motor->mta_client->obs_angular_displacement_.get(*(motor->iq_com)); // get the angular displacement

  // Grab outbound messages in the com queue, store into buffer
  // If it transferred something to communication_buffer...
  if(motor->iq_com->GetTxBytes(communication_buffer_in, communication_length_in)) {
    write(motor->fd, communication_buffer_in, communication_length_in);
  } else {
    return -1; // should be NAN...
  }
  
  delay(1); // delay 1ms for serial data to transmit data...
  
  // Reads however many bytes are currently available
  communication_length_in = read(motor->fd, communication_buffer_in, IQ_BUFLEN);
  
  // Puts the recently read bytes into com's receive queue
  motor->iq_com->SetRxBytes(communication_buffer_in, communication_length_in);
  
  uint8_t *rx_data; // temporary pointer to received type+data bytes
  uint8_t rx_length; // number of received type+data bytes
  // while we have message packets to parse
  while(motor->iq_com->PeekPacket(&rx_data, &rx_length)) {
    // Share that packet with all client objects
    //    motor->mta_client->ReadMsg(*(motor->iq_com), rx_data, rx_length);
    motor->mta_client->ReadMsg(rx_data, rx_length);
    
    // Once we're done with the message packet, drop it
    motor->iq_com->DropPacket();
  }

  //  if( motor->mta_client->obs_angular_displacement_.IsFresh() ) {
    angle = motor->mta_client->obs_angular_displacement_.get_reply();
  //  }
    
  return angle;
}

EXTERNC void iqSetAngle( double target_angle, unsigned long travel_time_ms ) {
  // This buffer is for passing around messages.
  uint8_t communication_buffer_in[IQ_BUFLEN];
  uint8_t communication_buffer_out[IQ_BUFLEN];
  // Stores length of message to send or receive
  uint8_t communication_length_in;
  uint8_t communication_length_out;

  /////////////// WRITE OUTPUT CONTROLLER
  motor->mta_client->ctrl_mode_.set(*(motor->iq_com), 6); // put the input controller in "coast" mode
  
  // Generate the set messages
  motor->mta_client->trajectory_angular_displacement_.set(*(motor->iq_com), (float) target_angle);
  motor->mta_client->trajectory_duration_.set(*(motor->iq_com), (float) travel_time_ms / 1000.0 ); 

  motor->mta_client->obs_angular_displacement_.get(*(motor->iq_com));
  
  // Grab outbound messages in the com queue, store into buffer
  // If it transferred something to communication_buffer...
  if(motor->iq_com->GetTxBytes(communication_buffer_out, communication_length_out)) {
    write(motor->fd, communication_buffer_out, communication_length_out);
  }
}

EXTERNC void iqSetAngleDelta( double target_angle_delta, unsigned long travel_time_ms ) {
  double cur_angle;

  cur_angle = iqReadAngle();
  iqSetAngle(cur_angle + target_angle_delta, travel_time_ms );
}

