#ifndef __ZAPPY_UI__
#define __ZAPPY_UI__

#define ADC_SLOW 0
#define ADC_FAST 1

extern char ui_notifications[32];
void oled_logo(void);
void oled_ui(void);
float convert_code(uint16_t code, uint8_t adc_path);
uint16_t convert_voltage_adc_code(float hv, uint8_t adc_path);

#endif
