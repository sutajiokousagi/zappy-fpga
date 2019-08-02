#ifndef __ZAPPY_UI__
#define __ZAPPY_UI__

#define ADC_SLOW 0
#define ADC_FAST 1

extern char ui_notifications[32];
extern uint8_t last_row;
extern uint8_t last_col;

void oled_logo(void);
void oled_ui(void);
float convert_code(uint16_t code, uint8_t adc_path);
uint16_t convert_voltage_adc_code(float hv, uint8_t adc_path);
float mk_code_to_voltage(uint16_t code);
uint16_t volts_to_hvdac_code(float voltage);

#endif
