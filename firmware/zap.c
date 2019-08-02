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

#define VOLT_TOLERANCE 0.01
#define WAIT_TIMEOUT   100   // timeout in ms
// returns 0 if success, 1 if timeout
uint32_t wait_until_voltage(uint32_t voltage) {
  // core acquisition/trigger loop
  int acq_timer, start_time, delta;
  float pct_diff;
  float cur_v = 0.0;
  uint16_t  *data = (uint16_t *)MONITOR_BASE;
  
  monitor_depth_write(10);
  monitor_presample_write(200); // won't trigger during monitor acquisition, because presample > depth

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
    printf( "debug: cur_v = %dmV, delta %dms, pct_diff %d\n", (int) (cur_v * 1000), ((delta)*1000/SYSTEM_CLOCK_FREQUENCY), (int) (pct_diff * 100));
  } while( (pct_diff >= VOLT_TOLERANCE) && (((delta)*1000/SYSTEM_CLOCK_FREQUENCY) < WAIT_TIMEOUT) );
  
  if( pct_diff < -0.01 ) {
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

int32_t do_zap(uint8_t row, uint8_t col, uint32_t voltage, uint32_t depth) {
  // fundamental hardware modes
  monitor_period_write(SYSTEM_CLOCK_FREQUENCY / 1000000); // shoot for 1 microsecond period
  zappio_triggermode_write(0); // use hardware trigger
  zappio_override_safety_write(1); // set to 1 to bypass lockouts for testing
  
  if( voltage > 1000 ) {
    printf( "Voltage out of range (0-1000): %d\n", voltage );
    return -1;
  }
  // pull in row/col
  if( row >= 4 ) {
    printf( "Row out of range (0-3): %d\n", row );
    return -1;
  }
  if( col >= 12 ) {
    printf( "Col out of range (0-11): %d\n", col );
    return -1;
  }
  zappio_col_write(1 << col);
  zappio_row_write(1 << row);
  
  // basic safety status
  if( zappio_scram_status_read() ) {
    printf( "ERROR: zappio is indicating a SCRAM condition. Aborting.\n" );
    return -1;
  }
  if( zappio_override_safety_read() ) {
    printf( "WARNING: zappio safeties are overidden. Hope you know what you're doing!\n" );
  }
  if( zappio_mb_unplugged_read() ) {
    printf( "WARNING: motherboard seems to be unplugged.\n" );
  }
  if( zappio_mk_unplugged_read() ) {
    printf( "WARNING: HV supply seems to be unplugged.\n" );
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
  
  zappio_hv_setting_write(10); ////// set to the desired value, right now this is just a stand-in low voltage
  while( !zappio_hv_ready_read() )
    ;
  zappio_hv_update_write(1); // commit the voltage
  
  
  // now here we would wait until we got to the desired voltage
  // (code to wait until the cap voltage is correct)
  // use the monitor_acquire_write(1) API with a depth of 1 to update the instantaneous ADC readback values
  if( wait_until_voltage(voltage) ) {
    printf( "WARNING: timeout waiting for voltage" );
  }
  
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
  
  // WAIT UNTIL CAPACITOR IS DISCHARGED (code missing)
  printf( "WARNING: missing code to wait until cap voltage is discharged\n" );
  delay_ms(10); // just hard wait instead
  
  zappio_discharge_write(0);
  zappio_cap_write(0); // disengage the capacitor
  
  // WAIT UNTIL MKHV is reading 0
  printf( "WARNING: missing code to wait until MKHV supply is discharged\n" );
  // this code would use the vmon() API to check the monitor voltage of the MKHV supply
  
  // status print after safe shutdown
  printf("Acquisition finished in %d ticks or %d ms. Overrun status: %d\n", delta, (delta)*1000/SYSTEM_CLOCK_FREQUENCY,
	 monitor_overrun_read());
  printf("Run 'upload' to get a copy of the data\n");
  
  return 0;
}
