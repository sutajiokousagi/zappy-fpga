#ifndef __MOTOR_H
#define __MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif
  
void iqCreateMotor(void);
void iqSetAngleDelta( double target_angle_delta, unsigned long travel_time_ms );
double iqReadAngle( void );
void iqSetAngle( double target_angle, unsigned long travel_time_ms );
void delay(int ms);


void motor_init(void);
void motor_isr(void);
void motor_sync(void);

void motor_write(char c);
char motor_read(void);
int motor_read_nonblock(void);

#ifdef __cplusplus
}
#endif

#endif

