// calibration records, unique to each zappy device
#ifndef __ZAPPY_CAL__
#define __ZAPPY_CAL__

typedef struct _cal_record {
  char hostname[32];
  float fast_m; // y = mx + b curve for interpolating voltages
  float fast_b;
  float slow_m;
  float slow_b;
  float p5v_adc;
  float p5v_adc_logic;
  float fs_dac;
  float hvdac_m;
  float hvdac_b;
  float capres;
} cal_record;

extern const cal_record zappy_cal;

#ifndef ZAPPY_SERIAL  // allows ZAPPY_SERIAL to be specified via make D= argument
#define ZAPPY_SERIAL  1
#endif


#if ZAPPY_SERIAL == 1
/*
Calibration curve measurements (serial #1): 

At temperature: 29.5C

0.0027V
slow: 0.715mV
fast: 0.957mV

4.9967 V
slow: 21.77mV
fast: 23.40mV

9.9903 V
slow: 43.49mV
fast: 46.52mV

14.986 V
slow: 6.22 mV  (+/-0.01mV)
fast: 69.67 mV  (+/-0.01mV)

19.980 V
slow: 86.94mV
fast: 92.83mV

24.977 V
slow: 0.10866V
fast: 0.11597V

34.966 V
slow: 0.15212V
fast: 0.16229V
 */
#define HOSTNAME  "zappy-01"
#define FAST_M  215.7720466
#define FAST_B  -0.0488699
#define SLOW_M  229.9235716
#define SLOW_B  -0.008779325
#define P5V_ADC 5.009
#define P5V_ADC_LOGIC 5.000  // not yet measured
#define FS_DAC  9.9643
#define HVDAC_M 6556.4623
#define HVDAC_B 201.1304
#define CAPRES  46.79   // ohms, measured at 27.5C
#endif


#endif
