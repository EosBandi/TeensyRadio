/**
 * @file serial.h 
 * 
 * Prototypes for serial functions
 * 
 */
#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stdint.h>
#include "radio.h"



/**
 * @brief Check is a serial speed is valid
 * 
 * @param speed Speed used in the parameter settings (real speed/1000, rounded )
 * @return true speed is valid 
 * @return false speed is invalid
 */
bool serial_device_valid_speed(register uint8_t speed);


/**
 * @brief init serial port (serial1) with the given speed
 * 
 * @param speed Speed used in the parameter settings (real speed/1000, rounded )
 */
void serial_init(register uint8_t speed);
/**
 * @brief Read from the serial port buffer (using buffer implemented in Teensy serial library)
 * 
 * @param buf char* buffer to read into
 * @param count number of bytes (FIXME: altough the buffer is larger that 256 byte, we never read more that 256 byte at once)
 * @return true Read is successfull
 * @return false  Read is unsuccessfull (less byte in the buffer than count)
 */
bool serial_read_buf( uint8_t * buf, uint8_t count);

#endif