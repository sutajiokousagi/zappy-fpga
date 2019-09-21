extern uint32_t sampledepth;

// max_current_code < 0 means don't use max_current
int32_t do_zap(uint8_t row, uint8_t col, uint32_t voltage, uint32_t depth, int16_t max_current_code, uint32_t energy_cutoff);
uint32_t wait_until_safe(void);
