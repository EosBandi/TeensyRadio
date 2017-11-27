///
/// @file	tdm.h
///
/// Interface to the time division multiplexing code
///

#ifndef _TDM_H_
#define _TDM_H_

/// initialise tdm subsystem
///
extern void tdm_init(void);

// tdm main loop
///
extern void tdm_serial_loop(void);

/// report tdm timings
///
extern void tdm_report_timing(void);

/// dispatch a remote AT command
extern void tdm_remote_at(void);

/// change tdm phase (for testing recovery)
extern void tdm_change_phase(void);

/// show RSSI information
extern void tdm_show_rssi(void);

/// the long term duty cycle we are aiming for
extern uint8_t duty_cycle;

/// the LBT threshold
extern uint8_t lbt_rssi;

#endif // _TDM_H_
