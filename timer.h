/**
 * @file timer.h 
 * 
 * Prototypes for timer and delay functions
 * 
 */


/// return a 16 bit value that rolls over in approximately
/// one second intervals
///
/// @return		16 bit value in units of 16 microseconds
///
extern uint16_t timer2_tick(void);


/// initialise timers
///
extern void timer_init(void);

/// Set the delay timer
///
/// @note Maximum delay is ~2.5sec in the current implementation.
///
/// @param	msec		Minimum time before the timer expiers.  The actual time
///				may be greater.
///
extern void	delay_set( uint16_t msec);

/// Set the delay timer in 100Hz ticks
///
/// @param ticks		Number of ticks before the timer expires.
///
extern void delay_set_ticks( uint8_t ticks);

/// Check the delay timer.
///
/// @return			True if the timer has expired.
///
extern bool	delay_expired(void);

/// Wait for a period of time to expire.
///
/// @note Maximum wait is ~2.5sec in the current implementation.
///
/// @param	msec		Minimum time to wait.  The actual time
///				may be greater.
///
extern void	delay_msec( uint16_t msec);

/// return some entropy from timers
///
extern uint8_t timer_entropy(void);

/**
 * @brief 100hz timer ISR function
 */
extern void hundredhztimer(void);
/**
 * @brief 16ms timer ISR function
 */
extern void timer2irq(void);