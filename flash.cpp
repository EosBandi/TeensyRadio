///
/// @file	flash.c
///
/// Flash-related data structures and functions, including the application
/// signature for the bootloader.
///

#include <stdint.h>
#include "radio.h"


/// Load the write-enable keys into the hardware in order to enable
/// one write or erase operation.
///

void
flash_erase_scratch(void)
{
}

uint8_t
flash_read_scratch(uint16_t address)
{
	uint8_t	d;
	d = EEPROM.read(address);
	return d;
}

void
flash_write_scratch(uint16_t address, uint8_t c)
{
	EEPROM.write(address,c);
}
