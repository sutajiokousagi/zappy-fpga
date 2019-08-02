#include <stdbool.h>
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

#include "zap.h"
#include "delay.h"
#include "ui.h"

uint32_t wait_until_voltage(uint32_t voltage);
uint32_t wait_until_safe(void);

uint8_t last_row = 0;
uint8_t last_col = 0;

#define VOLT_TOLERANCE 0.01
#define WAIT_TIMEOUT   100   // timeout in ms
#define SAFE_THRESH    10.0  // safety threshold in volts, if under this, we can move to next operation

// returns 0 if success, 1 if timeout
uint32_t wait_until_voltage(uint32_t voltage) {
  // core acquisition/trigger loop
  int acq_timer, start_time, delta;
  float pct_diff;
  float cur_v = 0.0;
  uint16_t  *data = (uint16_t *)MONITOR_BASE;
  
  zappio_triggerclear_write(1);
  monitor_depth_write(10);
  monitor_presample_write(10); // presample == depth will prevent trigger from ever happening

  elapsed(&acq_timer, -1);
  start_time = acq_timer;
  do {
    pct_diff = ((float) voltage) - cur_v;
    pct_diff = pct_diff / (float) voltage;
    
    monitor_acquire_write(1); // start acquisition & trigger cycle
    while( monitor_done_read() ) // wait for done to go 0
      ;
    while( monitor_done_read() == 0 ) // wait for done to go back to a 1
      ; // in this loop here, we could monitor the current and stop the zap if it goes too high

    
    // update delta timer
    elapsed(&acq_timer, -1);
    delta = acq_timer - start_time;
    if( delta < 0 )
      delta += timer0_reload_read();

    // grab the voltage
    cur_v = convert_code(data[0], ADC_SLOW); // index 0 is slow, index 1 is fast
    //    printf( "debug: cur_v = %dmV, delta %dms, pct_diff %d\n", (int) (cur_v * 1000), ((delta)*1000/SYSTEM_CLOCK_FREQUENCY), (int) (pct_diff * 100));
  } while( (pct_diff >= VOLT_TOLERANCE) && (((delta)*1000/SYSTEM_CLOCK_FREQUENCY) < WAIT_TIMEOUT) );
  
  if( pct_diff < -0.01 ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: HV overshoot");
    printf( "warning: target voltage overshoot!\n" );
    // return immediately in this case, to avoid any further charging of the capacitor
    return 0;
  }
  
  delay_ms(1);  // wait 1 millisecond longer, this should help improve any convergence/noise gap

  if( pct_diff >= VOLT_TOLERANCE )
    return 1; // timed out
  else
    return 0;
}

// returns 0 if success
uint32_t wait_until_safe(void) {
  int acq_timer, start_time, delta;
  float cur_v = 0.0;
  float mk_v = 0.0;
  uint16_t  *data = (uint16_t *)MONITOR_BASE;
  
  zappio_triggerclear_write(1);
  monitor_depth_write(10);
  monitor_presample_write(10); // presample == depth will prevent trigger from ever happening

  elapsed(&acq_timer, -1);
  start_time = acq_timer;
  do {
    monitor_acquire_write(1); // start acquisition & trigger cycle
    while( monitor_done_read() ) // wait for done to go 0
      ;
    while( monitor_done_read() == 0 ) // wait for done to go back to a 1
      ; // in this loop here, we could monitor the current and stop the zap if it goes too high

    vmon_acquire_write(1);
    while( !vmon_valid_read() )
      ;
    mk_v = mk_code_to_voltage(vmon_data_read());
      
    // update delta timer
    elapsed(&acq_timer, -1);
    delta = acq_timer - start_time;
    if( delta < 0 )
      delta += timer0_reload_read();

    // grab the voltage
    cur_v = convert_code(data[0], ADC_SLOW); // index 0 is slow, index 1 is fast
  } while( ((cur_v > SAFE_THRESH) || (mk_v > SAFE_THRESH)) && (((delta)*1000/SYSTEM_CLOCK_FREQUENCY) < WAIT_TIMEOUT) );
  
  if( cur_v > SAFE_THRESH ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: main cap unsafe %dV", (int) cur_v);
    return 1;
  }
  if( mk_v > SAFE_THRESH ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: MK cap unsafe %dV", (int) mk_v);
    return 2;
  }
  
  return 0;
}

int32_t do_zap(uint8_t row, uint8_t col, uint32_t voltage, uint32_t depth) {
  // fundamental hardware modes
  monitor_period_write(SYSTEM_CLOCK_FREQUENCY / 1000000); // shoot for 1 microsecond period
  zappio_triggermode_write(0); // use hardware trigger
  zappio_override_safety_write(0); // set to 1 to bypass lockouts for testing
  
  snprintf(ui_notifications, sizeof(ui_notifications), "Zap: completed"); // set a defalut "all good" message
  
  if( voltage > 1000 ) {
    printf( "Voltage out of range (0-1000): %d\n", voltage );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: request V err %d", voltage);
    return -1;
  }
  // pull in row/col
  if( row >= 4 ) {
    printf( "Row out of range (0-3): %d\n", row );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: request row err %d", row);
    return -1;
  }
  if( col >= 12 ) {
    printf( "Col out of range (0-11): %d\n", col );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: request col err %d", col);
    return -1;
  }
  zappio_col_write(1 << col);
  zappio_row_write(1 << row);
  last_row = row;
  last_col = col;
  
  // basic safety status
  if( zappio_scram_status_read() ) {
    printf( "ERROR: zappio is indicating a SCRAM condition. Aborting.\n" );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: SCRAM abort");
    return -1;
  }
  if( zappio_override_safety_read() ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: Safety override");
    printf( "WARNING: zappio safeties are overidden. Hope you know what you're doing!\n" );
  }
  if( zappio_mb_unplugged_read() ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: MB unplugged");
    printf( "WARNING: motherboard seems to be unplugged.\n" );
    return -1;
  }
  if( zappio_mk_unplugged_read() ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: HV unplugged");
    printf( "WARNING: HV supply seems to be unplugged.\n" );
    return -1;
  }
  
  // disconnect fast-discharge resistor, connect capacitor
  zappio_discharge_write(0); // make sure the discharge resistor is disengaged before engaging the capacitor
  zappio_cap_write(1);  // this should be a routine that charges the capacitor, and waits until the cap is charged
  
  // update & engage the MKHV supply
  zappio_hv_setting_write(0);  // supply /should/ already be zero here
  while( !zappio_hv_ready_read() )
    ;
  zappio_hv_update_write(1);
  
  zappio_hv_engage_write(1);  // engage the supply before writing, under the theory that the supply is at 0
  
  zappio_hv_setting_write(volts_to_hvdac_code((float)voltage));
  while( !zappio_hv_ready_read() )
    ;
  zappio_hv_update_write(1); // commit the voltage
  
  
  // now here we would wait until we got to the desired voltage
  // (code to wait until the cap voltage is correct)
  // use the monitor_acquire_write(1) API with a depth of 1 to update the instantaneous ADC readback values
  if( wait_until_voltage(voltage) ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: charge timeout");
    printf( "WARNING: timeout waiting for voltage" );
  }
  zappio_triggerclear_write(1);
  
  // core acquisition/trigger loop
  int acq_timer, start_time;
  monitor_depth_write(depth);
  monitor_presample_write(1000); // TODO ** change to 200 samples before engaging the zap (1000 for now for exaggerated effect)
  
  elapsed(&acq_timer, -1);
  start_time = acq_timer;
  monitor_acquire_write(1); // start acquisition & trigger cycle
  while( monitor_done_read() ) // wait for done to go 0
    ;
  while( monitor_done_read() == 0 ) // wait for done to go back to a 1
    ; // in this loop here, we could monitor the current and stop the zap if it goes too high
  elapsed(&acq_timer, -1);
  int delta = acq_timer - start_time;
  if( delta < 0 )
    delta += timer0_reload_read();
  
  // safe shutdown
  zappio_col_write(0); // no row/col selected
  zappio_row_write(0);
  zappio_hv_setting_write(0);  // set supply to zero
  while( !zappio_hv_ready_read() )
    ;
  zappio_hv_update_write(1); 
  
  zappio_hv_engage_write(0); // disengage the supply
  zappio_discharge_write(1); // turn on the capacitor discharge resistor
  
  if( wait_until_safe() ) {
    printf( "WARNING: storage cap not discharged\n" ); // ui error message is in the wait_until_safe() api call
  }
  
  zappio_discharge_write(0);
  zappio_cap_write(0); // disengage the capacitor
  
  // status print after safe shutdown
  printf("Acquisition finished in %d ticks or %d ms. Overrun status: %d\n", delta, (delta)*1000/SYSTEM_CLOCK_FREQUENCY,
	 monitor_overrun_read());
  printf("Run 'upload' to get a copy of the data\n");
  
  return 0;
}
