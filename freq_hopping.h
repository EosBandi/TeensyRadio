///
/// @file	freq_hopping.h
///
/// Prototypes for the frequency hopping manager
///

#ifndef _FREQ_HOPPING_H_
#define _FREQ_HOPPING_H_

#define MAX_FREQ_CHANNELS 50

/// Randomly shuffle fixed variables for entoropy
///
extern void shuffleRand(void);

/// initialise frequency hopping logic
///
extern void fhop_init(void);

/// tell the TDM code what channel to transmit on
///
/// @return		The channel that we should be transmitting on.
///
extern uint8_t fhop_transmit_channel(void);

/// tell the TDM code what channel to receive on
///
/// @return		The channel that we should be receiving on.
//
extern uint8_t fhop_receive_channel(void);

/// called when the transmit window flips
///
extern void fhop_window_change(void);

/// called when we get or lose radio lock
///
/// @param locked	True if we have gained lock, false if we have lost it.
///
extern void fhop_set_locked(bool locked);

/// how many channels are we hopping over
extern uint8_t num_fh_channels;

#endif // _FREQ_HOPPING_H_
