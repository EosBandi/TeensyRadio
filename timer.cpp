/**
 * @file timer.cpp
 * 
 * Timer functions and delays
 * 
 */

#include "radio.h"
#include "timer.h"

/// Counter used by delay_msec
///
static volatile uint8_t delay_counter;

/// high 16 bits of timer2 SYSCLK/12 interrupt
static volatile uint16_t timer2_high = 0;

//Intervalum timer



//This timer does 100hz, so 100 is one second. All I have to do is to set up a timer interrupt and call it the at_timer inside it.
void hundredhztimer(void)
{

	// call the AT parser tick
	if (Serial.available())
	{
		char c = Serial.read();
		at_input(c);
		at_command();
	}

	// update the delay counter
	if (delay_counter > 0)
		delay_counter--;
}

void
delay_set( uint16_t msec)
{
	if (msec >= 2550) {
		delay_counter = 255;
	} else {
		delay_counter = (msec + 9) / 10;
	}
}

void 
delay_set_ticks( uint8_t ticks)
{
	delay_counter = ticks;
}

bool
delay_expired(void)
{
	return delay_counter == 0;
}

void
delay_msec( uint16_t msec)
{
	delay_set(msec);
	while (!delay_expired())
		;
}

/*
// timer2 interrupt called every 32768 microseconds
INTERRUPT(T2_ISR, INTERRUPT_TIMER2)
{
	// re-arm the interrupt by clearing TF2H
	TMR2CN = 0x04;

	// increment the high 16 bits
	timer2_high++;

	if (feature_rtscts) {
		serial_check_rts();
	}
}

// return the 16 bit timer2 counter
// this call costs about 2 microseconds
uint16_t 
timer2_16(void)
{
	 uint8_t low, high;
	do {
		// we need to make sure that the high byte hasn't changed
		// between reading the high and low parts of the 16 bit timer
		high = TMR2H;
		low = TMR2L;
	} while (high != TMR2H);
	return low | (((uint16_t)high)<<8);
}

#if 0
// return microseconds since boot
// this call costs about 5usec
uint32_t 
micros(void)
{
	uint16_t low, high;
	do {
		high = timer2_high;
		low = timer2_16();
	} while (high != timer2_high);
	return ((((uint32_t)high)<<16) | low) >> 1;
}
#endif
*/
// return a 16 bit value that rolls over in approximately
// one second intervals
void  timer2irq(void)
{
	timer2_high++;
}


uint16_t 
timer2_tick(void)
{
	//uint32_t t;

	//t = micros();
	//t = t / 15;				//decrease precision to 16uS 

	return timer2_high;		//Convert to uint16 to match with the program....


}

// initialise timers
void 
timer_init(void)
{

	//Start calling at_timer function in every .01 second (100 calls per second)
	//ivtAtTimer.begin(hundredhztimer,10000);

}




// return some entropy
uint8_t
timer_entropy(void)
{
	return (char)timer2_high;
}
