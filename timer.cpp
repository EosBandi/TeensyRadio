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

//This timer does 100hz, so 100 is one second.
//Altough we could replace it with millisEllapes, we still nedd a relative slow interrupt to handle command serial port...

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


// Reimplement it with micros()
// However micros returns unsigned long, so we have to get it devided by 16 (>>4) and chop the upper 16 bits.
// That gives a 16uS tick counter
// This supposed to free up an intervaltimer.... but since it cannot be fine tuned we will loose 3DR compatibility 
// ** The actual timer2 tick in 3DR radio is 15.6 uSec.
uint16_t timer2_tick(void) {

unsigned long ticks;

	ticks = micros();
	ticks = ticks >> 4;
	return ticks;
}

// return some entropy
uint8_t
timer_entropy(void)
{
	return (char)micros();
}
