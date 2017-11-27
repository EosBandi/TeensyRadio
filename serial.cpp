/**
 * @file serial.cpp
 * 
 * Wrappers and helper functions to access and set serial hardware in the Teensy
 * 
 * TODO: make it abstract and remove hardcoded Serialx references (if possible)
 * 
 */


#include "radio.h"


///
/// Table of supported serial speed settings.
/// the table is looked up based on the 'one byte'
/// serial rate scheme that APM uses. If an unsupported
/// rate is chosen then 57600 is used
///
static const struct {
	uint8_t rate;
	uint32_t speed;

} serial_rates[] = {
	{1,   1200}, // 1200
	{2,   2400}, // 2400
	{4,   4800}, // 4800
	{9,   9600}, // 9600
	{19,  19200}, // 19200
	{38,  38400}, // 38400
	{57,  57600}, // 57600 - default
	{115, 115200}, // 115200
	{230, 230400}, // 230400
};


static volatile bool			tx_idle;


bool serial_read_buf( uint8_t * buf, uint8_t count)
{
	uint8_t i;
	uint16_t avail;

	avail = Serial1.available();
	if (count > avail) return false;

	for (i=0;i<count;i++)  buf[i] = Serial1.read();

	return true;
}



//
// check if a serial speed is valid
//
bool serial_device_valid_speed( uint8_t speed)
{
	uint8_t i;
	uint8_t num_rates = ARRAY_LENGTH(serial_rates);

	for (i = 0; i < num_rates; i++) {
		if (speed == serial_rates[i].rate) {
			return true;
		}
	}
	return false;
}

static void serial_device_set_speed(uint8_t speed)
{
	uint8_t i;
	uint8_t num_rates = ARRAY_LENGTH(serial_rates);

	for (i = 0; i < num_rates; i++) {
		if (speed == serial_rates[i].rate) {
			break;
		}
	}
	if (i == num_rates) {
		i = 6; // 57600 default
	}

	// set the rates in the UART
    
    //Serial1.begin(serial_rates[i].speed);
	Serial1.begin(serial_rates[i].speed,SERIAL_8N1);

	// tell the packet layer how fast the serial link is. This is
    // needed for packet framing timeouts
    

	packet_set_serial_speed(speed*125UL);	
}


void
serial_init(register uint8_t speed)
{
	tx_idle = true;

	serial_device_set_speed(speed);		// device-specific clocking setup
	if (param_get(PARAM_RTSCTS))
	{
		Serial1.attachCts(CTS_PIN);
		Serial2.attachRts(RTS_PIN);
	}


}


