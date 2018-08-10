///
/// @file	packet.c
///
/// packet handling code
///

#include <stdarg.h>
#include "radio.h"
#include "packet.h"
#include "timer.h"



static bool last_sent_is_resend;
static bool last_sent_is_injected;
static bool last_recv_is_resend;
static bool force_resend;

static uint8_t last_received[MAX_PACKET_LENGTH];
static uint8_t last_sent[MAX_PACKET_LENGTH];
static uint8_t last_sent_len;
static uint8_t last_recv_len;

// serial speed in 16usecs/byte
static uint16_t serial_rate;

// the length of a pending MAVLink packet, or zero if no MAVLink
// packet is expected
static uint8_t mav_pkt_len;

// the timer2_tick time when the MAVLink header was seen
static uint16_t mav_pkt_start_time;

// the number of timer2 ticks this packet should take on the serial link
static uint16_t mav_pkt_max_time;

static uint8_t mav_max_xmit;

// true if we have a injected packet to send
static bool injected_packet;

// have we seen a mavlink packet?
bool seen_mavlink;

#define PACKET_RESEND_THRESHOLD 32

// check if a buffer looks like a MAVLink heartbeat packet - this
// is used to determine if we will inject RADIO status MAVLink
// messages into the serial stream for ground station and aircraft
// monitoring of link quality
static void check_heartbeat(uint8_t * buf)
{
        if ((buf[1] == 9 && buf[0] == MAVLINK10_STX && buf[5] == 0) ||
            (buf[1] <= 9 && buf[0] == MAVLINK20_STX && buf[7] == 0 && buf[8] == 0 && buf[9] == 0)) {
		// looks like a MAVLink heartbeat



		seen_mavlink = true;
	}
}

#define MSG_TYP_RC_OVERRIDE 70
#define MSG_LEN_RC_OVERRIDE (9 * 2)


#define MAVLINK_FRAMING_DISABLED 0
#define MAVLINK_FRAMING_SIMPLE 1
#define MAVLINK_FRAMING_HIGHPRI 2

// return a complete MAVLink frame, possibly expanding
// to include other complete frames that fit in the max_xmit limit
static 
uint8_t mavlink_frame(uint8_t max_xmit, uint8_t * buf)
{
	uint16_t slen;

	last_sent_len = 0;
	mav_pkt_len = 0;

	slen = Serial1.available();

	// see if we have more complete MAVLink frames in the serial
	// buffer that we can fit in this packet
	while (slen >= 8) {
		register uint8_t c = Serial1.peekx(0);
                register uint8_t extra_len = 8;
		if (c != MAVLINK10_STX && c != MAVLINK20_STX) {
			// its not a MAVLink packet
			return last_sent_len;			
		}
                if (c == MAVLINK20_STX) {
                        extra_len += 4;
                        if (Serial1.peekx(2) & 1) {
                                // signed packet
                                extra_len += 13;
                        }
                }
                // fetch the length byte
		c = Serial1.peekx(1);
		if (c >= 255 - extra_len || 
		    c+extra_len > max_xmit - last_sent_len) {
			// it won't fit
			break;
		}
		if (c+extra_len > slen) {
			// we don't have the full MAVLink packet in
			// the serial buffer
			break;
		}

                c += extra_len;

                // we can add another MAVLink frame to the packet
                serial_read_buf(&last_sent[last_sent_len], c);
                memcpy(&buf[last_sent_len], &last_sent[last_sent_len], c);
                
                check_heartbeat(buf+last_sent_len);
                        
		last_sent_len += c;
		slen -= c;
	}

	return last_sent_len;
}


uint8_t encryptReturn(uint8_t *buf_out, uint8_t *buf_in, uint8_t buf_in_len)
{ 

  //This is the placeholder for encryption, until it implementd


  // if no encryption or not supported fall back to copy
  memcpy(buf_out, buf_in, buf_in_len);
  return buf_in_len;
}

// return the next packet to be sent


uint8_t
packet_get_next(register uint8_t max_xmit, uint8_t *buf)
{
	register uint16_t slen;

//***************************************************************************
	if (injected_packet) {
		// send a previously injected packet
		slen = last_sent_len;

		//Maximum send size for an injected packet is 32 bye, if the packet is longer than this, then we have to send it fragments
		if (max_xmit > 32) {
   		    max_xmit = 32;
		}

		if (max_xmit < slen) {
			// send as much as we can
 	        last_sent_len = slen - max_xmit;
   	        slen = encryptReturn(buf, last_sent, max_xmit);
			memcpy(last_sent, &last_sent[max_xmit], last_sent_len);
			last_sent_is_injected = true;
			return slen;
		}
		// send the rest
		injected_packet = false;
		last_sent_is_injected = true;
		return encryptReturn(buf, last_sent, last_sent_len);
	}
//****************************************************************************
	// Ok, there is no injected packet
	last_sent_is_injected = false;

	//Now check that is there any serial data available
	//TODO: Auxiliary data stream handling comes here.... 
	/*
		It sould look like the following 
		* Check avail bytes
		* Do a round robin weighted by FIFO bytes
		* avoid saturation by any channel
	*/

	slen = Serial1.available();

	//if force resend is set, then we have to resend the last packet which is still in the last_snet and length is last_sent_len
	//We resend only once, this is ensured by the last_sent_is_resend bit.
	if (force_resend) {
		if (max_xmit < last_sent_len) {
			return 0;
		}
		last_sent_is_resend = true;
		force_resend = false;
		return encryptReturn(buf, last_sent, last_sent_len);
	}

	last_sent_is_resend = false;

	// if we have received something via serial see how
	// much of it we could fit in the transmit FIFO
	if (slen > max_xmit) {
		slen = max_xmit;
	}

	last_sent_len = 0;

	if (slen == 0) {
		// nothing available to send
		return 0;
	}

	//TODO: Only main channel has a mavlink framing, auxiliary channels are raw, so they goes with simple framing.


	if (!feature_mavlink_framing) {
		// simple framing
		if (slen > 0 && serial_read_buf(buf, slen)) {		//Itt belemasolom a buf-ba (ez megy vissza mint csomag payload)
			last_sent_len = slen;
            return encryptReturn(last_sent, buf, slen);			//Es itt bemasolom a last_sent-be is, hogy meglegyen ha esetleg meg kell...
		}
    return 0;
	}
	
	// try to align packet boundaries with MAVLink packets

	if (mav_pkt_len == 1) {
		// we're waiting for the MAVLink length byte
		if (slen == 1) {
			if ((uint16_t)(timer2_tick() - mav_pkt_start_time) > mav_pkt_max_time) {
				// we didn't get the length byte in time
				last_sent[last_sent_len++] = Serial1.read(); // Send the STX
				mav_pkt_len = 0;
				return encryptReturn(buf, last_sent, last_sent_len);
			}
			// still waiting ....
			return 0;
		}
		// we have more than one byte, use normal packet frame
		// detection below
		mav_pkt_len = 0;
	}


	if (mav_pkt_len != 0) {
		if (slen < mav_pkt_len) {
			if ((uint16_t)(timer2_tick() - mav_pkt_start_time) > mav_pkt_max_time) {
				// timeout waiting for the rest of
				// it. Send what we have now.
				serial_read_buf(last_sent, slen);
				last_sent_len = slen;
				mav_pkt_len = 0;
				return encryptReturn(buf, last_sent, last_sent_len);
			}
			// leave it in the serial buffer till we have the
			// whole MAVLink packet			
			return 0;
		}
		
		// the whole of the MAVLink packet is available
		return mavlink_frame(max_xmit, buf);
	}

	// We are now looking for a new packet (mav_pkt_len == 0)
	while (slen > 0) {
		 uint8_t c = Serial1.peekx(0);
		if (c == MAVLINK10_STX || c == MAVLINK20_STX) {
			if (slen == 1) {
				// we got a bare MAVLink header byte
				if (last_sent_len == 0) {
					// wait for the next byte to
					// give us the length
					mav_pkt_len = 1;
					mav_pkt_start_time = timer2_tick();
					mav_pkt_max_time = serial_rate;
					return 0;
				}
				break;
			}
			//More than one byte is in the buffer, so get mavlink length byte (the one after STX)
			mav_pkt_len = Serial1.peekx(1);
			if (mav_pkt_len >= 255-(8+4+13) ||
			    mav_pkt_len+(8+4+13) > mav_max_xmit) {
				// its too big for us to cope with
				mav_pkt_len = 0;
				last_sent[last_sent_len++] = Serial1.read(); // Send the STX and try again (we will lose framing)
				slen--;				
				continue;
			}

			// the length byte doesn't include
			// the header or CRC
			mav_pkt_len += 8;
                        if (c == MAVLINK20_STX) {
                                mav_pkt_len += 4;
                                if (slen > 2 && (Serial1.peekx(2) & 1)) {
                                        // it is signed
                                        mav_pkt_len += 13;
                                }
                        }
			
			if (last_sent_len != 0) {
				// send what we've got so far,
				// and send the MAVLink payload
				// in the next packet
				mav_pkt_start_time = timer2_tick();
				mav_pkt_max_time = mav_pkt_len * serial_rate;
				return encryptReturn(buf, last_sent, last_sent_len);
			} else if (mav_pkt_len > slen) {
				// the whole MAVLink packet isn't in
				// the serial buffer yet. 
				mav_pkt_start_time = timer2_tick();
				mav_pkt_max_time = mav_pkt_len * serial_rate;
				return 0;					
			} else {
				// the whole packet is there
				// and ready to be read
				return mavlink_frame(max_xmit, buf);
			}
		} else {
			last_sent[last_sent_len++] = Serial1.read();
			slen--;
		}
	}
	return encryptReturn(buf, last_sent, last_sent_len);
}

// return true if the packet currently being sent
// is a resend
bool 
packet_is_resend(void)
{
	return last_sent_is_resend;
}

// return true if the packet currently being sent
// is an injected packet
bool 
packet_is_injected(void)
{
	return last_sent_is_injected;
}

// force the last packet to be resent. Used when transmit fails
void
packet_force_resend(void)
{
	force_resend = true;
}

// set the maximum size of a packet
void
packet_set_max_xmit(uint8_t max)
{
	mav_max_xmit = max;
}

// set the serial speed in bytes/s
void
packet_set_serial_speed(uint16_t speed)
{
	// convert to 16usec/byte to match timer2_tick()
	serial_rate = (65536UL / speed) + 1;
}

// determine if a received packet is a duplicate
bool 
packet_is_duplicate(uint8_t len, uint8_t *buf, bool is_resend)
{
	if (!is_resend) {
		memcpy(last_received, buf, len);
		last_recv_len = len;
		last_recv_is_resend = false;
		return false;
	}

	// We are now looking at a packet with the resend bit set
	if (last_recv_is_resend == false && 
			len == last_recv_len &&
			memcmp(last_received, buf, len) == 0) {						//It is the same as we have in the last_received buffer
		last_recv_is_resend = false;  // FIXME - this has no effect
		return true;
	}
#if 0
	printf("RS(%u,%u)[", (unsigned)len, (unsigned)last_recv_len);
	serial_write_buf(last_received, last_recv_len);
	serial_write_buf(buf, len);
	printf("]\r\n");
#endif
	last_recv_is_resend = true;
	return false;
}

// inject a packet to send when possible
void 
packet_inject(uint8_t *buf, uint8_t len)
{
	
	//The length of the injected packet should not be larger than the size of the last_sent buffer (252 byte)
	//However it is advisable that it is less than 32 byte in size, since packet engine will split injected packet for 32 byte sening units.

	if (len > sizeof(last_sent)) {
		len = sizeof(last_sent);
	}

	memcpy(last_sent, buf, len);
	last_sent_len = len;
	last_sent_is_resend = false;
	injected_packet = true;
}
