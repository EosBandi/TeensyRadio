#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stdint.h>
#include "radio.h"



/// check if a serial speed is valid
///
/// @param	speed		The serial speed to configure
///
bool serial_device_valid_speed(register uint8_t speed);
void serial_init(register uint8_t speed);
bool serial_read_buf( uint8_t * buf, uint8_t count);

#endif