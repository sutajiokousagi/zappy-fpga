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

#include <net/microudp.h>
#include <net/tftp.h>
#include "ethernet.h"

uint32_t wait_until_voltage(uint32_t voltage);

uint8_t last_row = 0;
uint8_t last_col = 0;

#define VOLT_TOLERANCE 0.01
#define WAIT_TIMEOUT   100   // timeout in ms
#define WAIT_CHARGE_TIMEOUT 250 // timout in ms
#define SAFE_THRESH    10.0  // safety threshold in volts, if under this, we can move to next operation
#define CHARGE_RETRY_LIMIT 3

// returns 0 if success, 1 if timeout
uint32_t wait_until_voltage(uint32_t voltage) {
  // core acquisition/trigger loop
  int acq_timer, start_time, delta;
  float pct_diff;
  float cur_v = 0.0;
  uint16_t  *data = (uint16_t *)MONITOR_BASE;
  int charge_retry = 0;
  int converged = 0;
  float volt_tolerance = VOLT_TOLERANCE;

  // at lower voltages, the tolerance is not as tight due to the range becoming smaller relative to the absolute accuracy of the circuitry
  if( voltage < 200 )
    volt_tolerance = 0.025;
  if( voltage < 120 )
    volt_tolerance = 0.05;

  // setup the loop to run
  zappio_triggerclear_write(1);
  monitor_depth_write(10);
  monitor_presample_write(10); // presample == depth will prevent trigger from ever happening

  elapsed(&acq_timer, -1);
  start_time = acq_timer;
  while( charge_retry < CHARGE_RETRY_LIMIT && !converged ) {
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
      //    printf( "debug: cur_v = %dmV, delta %dms, pct_diff %d\n", (int) (cur_v * 1000), ((delta)*1000/CONFIG_CLOCK_FREQUENCY), (int) (pct_diff * 100));
    } while( (pct_diff >= volt_tolerance) && (((delta)*1000/CONFIG_CLOCK_FREQUENCY) < WAIT_CHARGE_TIMEOUT) );
  
    if( pct_diff < -0.01 ) {
      snprintf(ui_notifications, sizeof(ui_notifications), "Zap: HV overshoot");
      printf( "warning: target voltage overshoot! : zwarn\n" );
      // return immediately in this case, to avoid any further charging of the capacitor
      return 0;
    }
    
    if( pct_diff >= volt_tolerance ) {
      // the charging didn't converge, could be due to OC condition on the HV supply.
      // re-set the supply by turning it off, then turning it back on again
      converged = 0;
      
      zappio_triggerclear_write(1); // make sure we're not in a triggered state that would engage row/col
      
      snprintf(ui_notifications, sizeof(ui_notifications), "Zap: HV converge retry");
      printf( "warning: HV supply convergence retry, should be benign : zwarn\n" );
      
      /////////// disengage the HV supply and re-engage it to clear any transient OC condition ////////
      zappio_hv_setting_write(0);  // set supply to zero
      while( !zappio_hv_ready_read() )
	;
      zappio_hv_update_write(1); 
  
      zappio_hv_engage_write(0); // disengage the supply
      zappio_discharge_write(1); // turn on the capacitor discharge resistor
      wait_until_safe(); // full cycle down
      
      delay_ms(250); // give it a fraction of a second to cool down
      
      // disconnect fast-discharge resistor, make sure cap is engaged (should already be engaged)
      zappio_discharge_write(0); 
      zappio_cap_write(1);
  
      //////////// update & re-engage the MKHV supply //////////////
      zappio_hv_engage_write(1);  // engage the supply before writing, under the theory that the supply is at 0
  
      zappio_hv_setting_write(volts_to_hvdac_code((float)voltage));
      while( !zappio_hv_ready_read() )
	;
      zappio_hv_update_write(1); // commit the voltage

      // re-initialize all the loop parameters
      cur_v = 0.0;
      zappio_triggerclear_write(1);
      monitor_depth_write(10);
      monitor_presample_write(10); // presample == depth will prevent trigger from ever happening
      
      elapsed(&acq_timer, -1);
      start_time = acq_timer;
    } else {
      converged = 1;
    }
    
    charge_retry++;
  }
    
  delay_ms(1);  // wait 1 millisecond longer, this should help improve any convergence/noise gap

  if( !converged )
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
  } while( ((cur_v > SAFE_THRESH) || (mk_v > SAFE_THRESH)) && (((delta)*1000/CONFIG_CLOCK_FREQUENCY) < WAIT_TIMEOUT) );
  
  if( cur_v > SAFE_THRESH ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: main cap unsafe %dV", (int) cur_v);
    status_led = LED_STATUS_RED;
    return 1;
  }
  if( mk_v > SAFE_THRESH ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: MK cap unsafe %dV", (int) mk_v);
    status_led = LED_STATUS_RED;
    return 2;
  }
  
  return 0;
}

// depth is equivalent to time in microseconds (each sample is one microsecond)
int32_t do_zap(uint8_t row, uint8_t col, uint32_t voltage, uint32_t depth, int16_t max_current_code, uint32_t energy_cutoff) {
  int r, c, rstart, cstart, rend, cend;
  
  telnet_tx = 1;
  snprintf(ui_notifications, sizeof(ui_notifications), "Zap: completed"); // set a defalut "all good" message
  
  if( voltage > 1000 ) {
    printf( "Voltage out of range (0-1000): %d : zerr\n", voltage );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: request V err %d", voltage);
    status_led = LED_STATUS_RED;
    return -1;
  }
  // pull in row/col
  if( row > 4 ) {
    printf( "Row out of range (0-3): %d : zerr\n", row );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: request row err %d", row);
    status_led = LED_STATUS_RED;
    return -1;
  }
  if( row == 4 ) {
    printf( "Row is 4, doing full row\n" );
    rstart = 0;
    rend = 4;
  } else {
    rstart = row;
    rend = row + 1;
  }
  if( col > 12 ) {
    printf( "Col out of range (0-11): %d : zerr\n", col );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: request col err %d", col);
    return -1;
  }
  if( col == 12 ) {
    printf( "Col is 12, doing full col\n" );
    cstart = 0;
    cend = 12;
  } else {
    cstart = col;
    cend = col + 1;
  }
  if( depth >= 16384 ) { // this constant is in zappy.py memdepth
    printf( "Depth too long: %d : zerr\n", depth );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: depth err %d", depth);
    status_led = LED_STATUS_RED;
    return -1;
  } else {
    sampledepth = depth; // global for the UI routine
  }

  // basic safety status
  if( zappio_scram_status_read() ) {
    printf( "ERROR: zappio is indicating a SCRAM condition. Aborting. : zerr\n" );
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: SCRAM abort");
    status_led = LED_STATUS_RED;
    return -1;
  }
  if( zappio_override_safety_read() ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: Safety override");
    printf( "WARNING: zappio safeties are overidden. Hope you know what you're doing! : zwarn\n" );
  }
  if( zappio_mb_unplugged_read() ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: MB unplugged");
    printf( "WARNING: motherboard seems to be unplugged. : zerr\n" );
    status_led = LED_STATUS_RED;
    return -1;
  }
  if( zappio_mk_unplugged_read() ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: HV unplugged");
    printf( "WARNING: HV supply seems to be unplugged. : zerr\n" );
    status_led = LED_STATUS_RED;
    return -1;
  }

  if( max_current_code > 0xFFF ) {
    snprintf(ui_notifications, sizeof(ui_notifications), "Zap: maxcur prog err");
    printf( "WARNING: max current code out of range, ignoring request : zerr\n" );
  }
  if( max_current_code >= 0 && max_current_code <= 0xFFF ) {
    zappio_maxdelta_ena_write(1);
    zappio_maxdelta_write( (uint16_t) max_current_code );
  } else {
    zappio_maxdelta_ena_write(0);
  }

  // disconnect fast-discharge resistor, connect capacitor
  zappio_discharge_write(0); // make sure the discharge resistor is disengaged before engaging the capacitor
  zappio_cap_write(1);
  
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

  // r, c already setup in arg checking
  for( r = rstart; r < rend; r++ ) {
    
    for( c = cstart; c < cend; c++ ) {

      zappio_col_write(0); // no row/col selected during main cap charging
      zappio_row_write(0);
      // now here we would wait until we got to the desired voltage
      // (code to wait until the cap voltage is correct)
      // use the monitor_acquire_write(1) API with a depth of 1 to update the instantaneous ADC readback values
      if( wait_until_voltage(voltage) ) {
	snprintf(ui_notifications, sizeof(ui_notifications), "Zap: charge timeout");
	status_led = LED_STATUS_RED;
	printf( "WARNING: timeout waiting for voltage : zwarn" );
      }
      
      zappio_triggerclear_write(1);

      if( energy_cutoff == 0 ) { // don't use energy cutoff, but still monitor
	monitor_energy_control_write(1 << CSR_MONITOR_ENERGY_CONTROL_RESET_OFFSET);
      } else {
	// setup energy control
	monitor_energy_threshold_write((unsigned long long int) energy_cutoff);
	monitor_energy_control_write(1 << CSR_MONITOR_ENERGY_CONTROL_ENABLE_OFFSET |
				     1 << CSR_MONITOR_ENERGY_CONTROL_RESET_OFFSET);
	printf( "Energy control debug: thresh %d, ctl %x\n", (uint32_t) monitor_energy_threshold_read(), monitor_energy_control_read());
      }
      
      // set the row/col parameters
      zappio_col_write(1 << c);
      zappio_row_write(1 << r);
      last_row = r;
      last_col = c;
  
      // core acquisition/trigger loop
      int acq_timer, start_time;
      monitor_depth_write(depth);
      monitor_presample_write(1000); // IF THIS CHANGES -- need to update zappy.py to change the preamble compensation time
  
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

      // check if maxdelta current scram happened during zap
      if( zappio_maxdelta_ena_read() ) {
	if( zappio_maxdelta_scram_read() ) {
	  snprintf(ui_notifications, sizeof(ui_notifications), "Zap: arc on r%d c%d", r+1, c+1);
	  status_led = LED_STATUS_RED;
	  printf( "WARNING: max current limit hit on row %d col %d : zwarn", r+1, c+1 );
	}
      }
      // clear the trigger; this also clears the maxdelta scram
      zappio_triggerclear_write(1);
      
      // disengage the row/col so the cap can charge
      zappio_col_write(0); // no row/col selected
      zappio_row_write(0);

      printf("Acquisition finished in %d ticks or %d ms. Overrun status: %d : zinfo\n", delta, (delta)*1000/CONFIG_CLOCK_FREQUENCY,
	     monitor_overrun_read());

      // capacitor charges while the upload happens
      // send up 1 megabyte of data to benchmark upload speed
      unsigned int ip;
      char fname[32];
      ip = IPTOINT(host_ip_addr[0], host_ip_addr[1], host_ip_addr[2], host_ip_addr[3]);
      // send the data dump
      snprintf(fname, sizeof(fname), "zappy-log.r%dc%d", r+1, c+1);
      tftp_put(ip, DEFAULT_TFTP_SERVER_PORT, fname, (void *)MONITOR_BASE, depth*4);
      
      // and store the measured energy of the run
      char energy[32];
      snprintf(fname, sizeof(fname), "zappy-energy.r%dc%d", r+1, c+1);
      snprintf(energy, sizeof(energy), "%02x%08x\n", (unsigned int) (monitor_energy_accumulator_read() >> 32),
	       (unsigned int) monitor_energy_accumulator_read());
      tftp_put(ip, DEFAULT_TFTP_SERVER_PORT, fname, (void *)energy, strlen(energy));
      
      // update the UI
      oled_ui();
    }
  }

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
    printf( "WARNING: storage cap not discharged : zwarn\n" ); // ui error message is in the wait_until_safe() api call
    status_led = LED_STATUS_RED;
  }
  
  zappio_discharge_write(0);
  zappio_cap_write(0); // disengage the capacitor
  
  // status print after safe shutdown
  printf("Run 'upload' to get a copy of the data\n");
  printf("Zap run finished : zpass\n");
  
  telnet_tx = 0;
  status_led = LED_STATUS_GREEN;
  return 0;
}
