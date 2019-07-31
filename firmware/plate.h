#define PROX_PRESENT_THRESH  5000
#define PROX_FULL_STROKE  28
#define PROX_CHECK_INTERVAL 80

#define MOTOR_JAM_CURRENT (0.25)

void do_plate(char *token);

// returns 0 if absent
uint32_t plate_present(void);

uint32_t plate_lock(void);
uint32_t plate_unlock(void);
uint32_t plate_home(void);
