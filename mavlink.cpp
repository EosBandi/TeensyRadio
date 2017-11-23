// -*- Mode: C; c-basic-offset: 8; -*-
//
// Copyright (c) 2012 Andrew Tridgell, All Rights Reserved
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
/// @file	mavlink.c
///
/// mavlink reporting code
///

#include <stdarg.h>
#include "radio.h"
#include "packet.h"
#include "timer.h"

extern uint8_t pbuf[MAX_PACKET_LENGTH];
static uint8_t seqnum;

// new RADIO_STATUS common message
#define MAVLINK_MSG_ID_RADIO_STATUS 109
#define MAVLINK_RADIO_STATUS_CRC_EXTRA 185

// use '3D' for 3DRadio
#define RADIO_SOURCE_SYSTEM '3'
#define RADIO_SOURCE_COMPONENT 'D'

/*
 * Calculates the MAVLink checksum on a packet in pbuf[] 
 * and append it after the data
 */
static void mavlink_crc(register uint8_t crc_extra)
{
	register uint8_t length = pbuf[1];
	uint16_t sum = 0xFFFF;
	uint8_t i, stoplen;

	stoplen = length + 6;

	// MAVLink 1.0 has an extra CRC seed
	pbuf[length+6] = crc_extra;
	stoplen++;

	i = 1;
	while (i<stoplen) {
		register uint8_t tmp;
		tmp = pbuf[i] ^ (uint8_t)(sum&0xff);
		tmp ^= (tmp<<4);
		sum = (sum>>8) ^ (tmp<<8) ^ (tmp<<3) ^ (tmp>>4);
		i++;
	}

	pbuf[length+6] = sum&0xFF;
	pbuf[length+7] = sum>>8;
}


/*
we use a hand-crafted MAVLink packet based on the following
message definition

<message name="RADIO" id="166">
	<description>Status generated by radio</description>
		<field type="uint8_t" name="rssi">local signal strength</field>
		<field type="uint8_t" name="remrssi">remote signal strength</field>
	<field type="uint8_t" name="txbuf">percentage free space in transmit buffer</field>
	<field type="uint8_t" name="noise">background noise level</field>
	<field type="uint8_t" name="remnoise">remote background noise level</field>
	<field type="uint16_t" name="rxerrors">receive errors</field>
	<field type="uint16_t" name="fixed">count of error corrected packets</field>
</message>

Note that the wire field ordering follows the MAVLink v1.0 spec

The RADIO_STATUS message in common.xml is the same as the
ardupilotmega.xml RADIO message, but is message ID 109 with
a different CRC seed
*/
struct mavlink_RADIO_v10 {
	uint16_t rxerrors;
	uint16_t fixed;
	uint8_t rssi;
	uint8_t remrssi;
	uint8_t txbuf;
	uint8_t noise;
	uint8_t remnoise;
};

/// send a MAVLink status report packet
/// we always send as MAVLink1 and let the recipient sort it out.
void MAVLink_report(void)
{
	struct mavlink_RADIO_v10 *m = (struct mavlink_RADIO_v10 *)&pbuf[6];
	pbuf[0] = MAVLINK10_STX;
	pbuf[1] = sizeof(struct mavlink_RADIO_v10);
	pbuf[2] = seqnum++;
	pbuf[3] = RADIO_SOURCE_SYSTEM;
	pbuf[4] = RADIO_SOURCE_COMPONENT;
	pbuf[5] = MAVLINK_MSG_ID_RADIO_STATUS;

        m->rxerrors = errors.rx_errors;
        m->fixed    = errors.corrected_packets;
        m->txbuf    = Serial1.availableForWrite();
        m->rssi     = statistics.average_rssi;
        m->remrssi  = remote_statistics.average_rssi;
        m->noise    = statistics.average_noise;
        m->remnoise = remote_statistics.average_noise;
	mavlink_crc(MAVLINK_RADIO_STATUS_CRC_EXTRA);

	if (Serial1.availableForWrite() < sizeof(struct mavlink_RADIO_v10)+8) {
		// don't cause an overflow
		return;
	}

	Serial1.write(pbuf, sizeof(struct mavlink_RADIO_v10)+8);
}
