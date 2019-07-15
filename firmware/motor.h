#ifndef __MOTOR_H
#define __MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif
  
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

