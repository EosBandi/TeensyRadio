// -*- Mode: C; c-basic-offset: 8; -*-
//
// Copyright (c) 2011 Michael Smith, All Rights Reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  o Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  o Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//

///
/// @file	parameters.c
///
/// Storage for program parameters.
///
/// Parameters are held in a contiguous array of 16-bit values.
/// It is up to the caller to decide how large a parameter is and to
/// access it accordingly.
///
/// When saved, parameters are copied bitwise to the flash scratch page,
/// with an 8-bit XOR checksum appended to the end of the flash range.
///

#include "radio.h"
//#include "tdm.h"
#include "crc.h"

#define DEBUG 1

/// In-ROM parameter info table.
///
const struct parameter_info {
	const char	*name;
	param_t		default_value;
} parameter_info[PARAM_MAX] = {
	{"FORMAT",         PARAM_FORMAT_CURRENT},
	{"SERIAL_SPEED",   57}, // match APM default of 57600
	{"AIR_SPEED",      64}, // relies on MAVLink flow control
	{"NETID",          25},
	{"TXPOWER",        11},
	{"ECC",             1},
	{"MAVLINK",         0},
	{"OPPRESEND",       1},
	{"MIN_FREQ",        0},
	{"MAX_FREQ",        0},
	{"NUM_CHANNELS",    13},
	{"DUTY_CYCLE",    100},
	{"LBT_RSSI",        0},
	{"MANCHESTER",      0},
	{"RTSCTS",          0},
	{"MAX_WINDOW",    131},
};

/// In-RAM parameter store.
///
/// It seems painful to have to do this, but we need somewhere to///
/// hold all the parameters when we're rewriting the scratchpad
/// page anyway.
///
param_t	parameter_values[PARAM_MAX];

// Three extra bytes, 1 for the number of params and 2 for the checksum
#define PARAM_FLASH_START   0
#define PARAM_FLASH_END     (PARAM_FLASH_START + sizeof(parameter_values) + 3)


// Check to make sure we dont overflow off the page
typedef char endCheck[(PARAM_FLASH_END < 1023) ? 0 : -1];

static bool
param_check(enum ParamID id, uint32_t val)
{
	// parameter value out of range - fail
	if (id >= PARAM_MAX)
		return false;

	switch (id) {
	case PARAM_FORMAT:
		return false;

	case PARAM_SERIAL_SPEED:
		return serial_device_valid_speed(val);

	case PARAM_AIR_SPEED:
		if (val > 256)
			return false;
		break;

	case PARAM_NETID:
		// all values are OK
		return true;

	case PARAM_TXPOWER:
		if (val > BOARD_MAXTXPOWER)
			return false;
		break;

	case PARAM_ECC:
	case PARAM_OPPRESEND:
		// boolean 0/1 only
		if (val > 1)
			return false;
		break;

	case PARAM_MAVLINK:
		if (val > 2)
			return false;
		break;

	case PARAM_MAX_WINDOW:
		// 131 milliseconds == 0x1FFF 16 usec ticks,
		// which is the maximum we can handle with a 13
		// bit trailer for window remaining
		if (val > 131)
			return false;
		break;

	default:
		// no sanity check for this value
		break;
	}
	return true;
}

bool
param_set(enum ParamID param, param_t value)
{
	// Sanity-check the parameter value first.
	if (!param_check(param, value))
		return false;

	// some parameters we update immediately
	switch (param) {
	case PARAM_TXPOWER:
		// useful to update power level immediately when range
		// testing in RSSI mode		
		radio_set_transmit_power(value);
		value = radio_get_transmit_power();
		break;

	case PARAM_DUTY_CYCLE:
		// update duty cycle immediately
		value = constrain_(value, 0, 100);
		duty_cycle = value;
		break;

	case PARAM_LBT_RSSI:
		// update LBT RSSI immediately
		if (value != 0) {
			value = constrain_(value, 25, 220);
		}
		lbt_rssi = value;
		break;

	case PARAM_MAVLINK:
		feature_mavlink_framing = (uint8_t) value;
		value = feature_mavlink_framing;
		break;

	case PARAM_OPPRESEND:
		break;

	case PARAM_RTSCTS:
		feature_rtscts = value?true:false;
		value = feature_rtscts?1:0;
		break;

	default:
		break;
	}

	parameter_values[param] = value;

	return true;
}

param_t
param_get(enum ParamID param)
{
	if (param >= PARAM_MAX)
		return 0;
	return parameter_values[param];
}

bool read_params(uint8_t * input, uint16_t start, uint8_t size)
{
	uint16_t		i;
	
	debug("Start :%u Size:%u\n",start,size);

	for (i = start; i < start+size; i ++){
		input[i-start] = flash_read_scratch(i);
		//debug("%u - %u \n",i,input[i-start]);
	}
	// verify checksum
	if (crc16(size, input) != ((uint16_t) flash_read_scratch(i+1)<<8 | flash_read_scratch(i)))
		return false;
	debug("Paramater load from eeprom successfull\n");
	return true;
}

void write_params(uint8_t * input, uint16_t start, uint8_t size)
{
	uint16_t	i, checksum;

	// save parameters to the scratch page
	for (i = start; i < start+size; i ++)
		flash_write_scratch(i, input[i-start]);
	
	// write checksum
	checksum = crc16(size, input);
	flash_write_scratch(i, checksum&0xFF);
	flash_write_scratch(i+1, checksum>>8);
}

bool
param_load(void)
{
	uint8_t	i, expected;

	// Start with default values
	param_default();
	debug("Start Param Load...\n");
	// loop reading the parameters array
	expected = flash_read_scratch(PARAM_FLASH_START);
	if (expected > sizeof(parameter_values) || expected < 12*sizeof(param_t))
		return false;
	
	// read and verify params
	if(!read_params((uint8_t *)parameter_values, PARAM_FLASH_START+1, expected))
		return false;
	
	// decide whether we read a supported version of the structure
	if (param_get(PARAM_FORMAT) != PARAM_FORMAT_CURRENT) {
		debug("parameter format %lu expecting %lu", param_get[PARAM_FORMAT], PARAM_FORMAT_CURRENT);
		return false;
	}

	for (i = 0; i < sizeof(parameter_values); i++) {
		if (!param_check((ParamID)i, parameter_values[i])) {
			parameter_values[i] = parameter_info[i].default_value;

		

		}
	}
	
	return true;
}

void
param_save(void)
{

	// tag parameters with the current format
	parameter_values[PARAM_FORMAT] = PARAM_FORMAT_CURRENT;

	// erase the scratch space
	// Actually we dont need this since we are using EEPROM library
	//flash_erase_scratch();

	// write param array length
	flash_write_scratch(PARAM_FLASH_START, sizeof(parameter_values));

	// write params
	write_params((uint8_t *)parameter_values, PARAM_FLASH_START+1, sizeof(parameter_values));


}

void
param_default(void)
{
	uint8_t	i;

	// set all parameters to their default values
	for (i = 0; i < PARAM_MAX; i++) {
		parameter_values[i] = parameter_info[i].default_value;
	}
	
}

enum ParamID
param_id(char * name)
{
	uint8_t i;

	for (i = 0; i < PARAM_MAX; i++) {
		if (!strcmp(name, parameter_info[i].name))
			break;
	}
	return (ParamID)i;
}

const char *
param_name(enum ParamID param)
{
	if (param < PARAM_MAX) {
		return parameter_info[param].name;
	}
	return 0;
}

// constraint for parameter values
uint32_t constrain_(uint32_t v, uint32_t min, uint32_t max)
{
	if (v < min) v = min;
	if (v > max) v = max;
	return v;
}

