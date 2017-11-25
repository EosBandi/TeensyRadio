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
/// @file	at.c
///
/// A simple AT command parser.
///

#include "radio.h"
//#include "tdm.h"
#include "at.h"

//Uncomment all tdm_command and param save-reads


// canary data for ram wrap. It is in at.c as the compiler
// assigns addresses in alphabetial order and we want this at a low
// address

// AT command buffer
char at_cmd[AT_CMD_MAXLEN + 1];
uint8_t	at_cmd_len;

// mode flags
bool		at_mode_active;	///< if true, incoming bytes are for AT command
bool		at_cmd_ready;	///< if true, at_cmd / at_cmd_len contain valid data

// test bits
uint8_t		at_testmode;    ///< test modes enabled (AT_TEST_*)

// command handlers
static void	at_ok(void);
static void	at_error(void);
static void	at_i(void);
static void	at_s(void);
static void	at_ampersand(void);
static void	at_p(void);
static void	at_plus(void);

void
at_input( uint8_t c)
{
	// AT mode is active and waiting for a command
	switch (c) {
		// CR - submits command for processing
	case '\r':
		put_char('\n');
		at_cmd[at_cmd_len] = 0;
		at_cmd_ready = true;
		break;

		// backspace - delete a character
		// delete - delete a character
	case '\b':
	case '\x7f':
		if (at_cmd_len > 0) {
			put_char('\b');
			put_char(' ');
			put_char('\b');
			at_cmd_len--;
		}
		break;

		// character - add to buffer if valid
	default:
		if (at_cmd_len < AT_CMD_MAXLEN) {
			if (isprint(c)) {
				c = toupper(c);
				at_cmd[at_cmd_len++] = c;
				put_char(c);
			}
			break;
		}

		// If the AT command buffer overflows we abandon
		// AT mode and return to passthrough mode; this is
		// to minimise the risk of locking up on reception
		// of an accidental escape sequence.

		at_mode_active = 0;
		at_cmd_len = 0;
		break;
	}
}

// +++ detector state machine
//
// states:
//
// wait_for_idle:	-> wait_for_plus1, count = 0 after 1s
//
// wait_for_plus:	-> wait_for_idle if char != +
//			-> wait_for_plus, count++ if count < 3
// wait_for_enable:	-> enabled after 1s
//			-> wait_for_idle if any char
//

#define	ATP_WAIT_FOR_IDLE	0
#define ATP_WAIT_FOR_PLUS1	1
#define ATP_WAIT_FOR_PLUS2	2
#define ATP_WAIT_FOR_PLUS3	3
#define ATP_WAIT_FOR_ENABLE	4

#define ATP_COUNT_1S		100	// 100 ticks of the 100Hz timer

static uint8_t	at_plus_state;
static uint8_t	at_plus_counter = ATP_COUNT_1S;

void
at_plus_detector( uint8_t c)
{
	// If we get a character that's not '+', unconditionally
	// reset the state machine to wait-for-idle; this will
	// restart the 1S timer.
	//
	if (c != (uint8_t)'+')
		at_plus_state = ATP_WAIT_FOR_IDLE;

	// We got a plus; handle it based on our current state.
	//
	switch (at_plus_state) {

	case ATP_WAIT_FOR_PLUS1:
	case ATP_WAIT_FOR_PLUS2:
		at_plus_state++;
		break;

	case ATP_WAIT_FOR_PLUS3:
		at_plus_state = ATP_WAIT_FOR_ENABLE;
		at_plus_counter = ATP_COUNT_1S;
		break;

	default:
		at_plus_state = ATP_WAIT_FOR_IDLE;
		// FALLTHROUGH
	case ATP_WAIT_FOR_IDLE:
	case ATP_WAIT_FOR_ENABLE:
		at_plus_counter = ATP_COUNT_1S;
		break;
	}
}


void
at_timer(void)
{


	// if the counter is running
	if (at_plus_counter > 0) {

		// if it reaches zero, the timeout has expired
		if (--at_plus_counter == 0) {

			// make the relevant state change
			switch (at_plus_state) {
			case ATP_WAIT_FOR_IDLE:
				at_plus_state = ATP_WAIT_FOR_PLUS1;
				break;

			case ATP_WAIT_FOR_ENABLE:
				at_mode_active = true;
				at_plus_state = ATP_WAIT_FOR_IDLE;
				// stuff an empty 'AT' command to get the OK prompt
				at_cmd[0] = 'A';
				at_cmd[1] = 'T';
				at_cmd[2] = '\0';
				at_cmd_len = 2;
				at_cmd_ready = true;
				break;
			}
		}
	}
}

void
at_command(void)
{
	// require a command with the AT prefix
	if (at_cmd_ready) {
		if ((at_cmd_len >= 2) && (at_cmd[0] == 'R') && (at_cmd[1] == 'T')) {
			// remote AT command - send it to the tdm
			// system to send to the remote radio
			//tdm_remote_at();
			at_cmd_len = 0;
			at_cmd_ready = false;
			return;
		}
		
		if ((at_cmd_len >= 2) && (at_cmd[0] == 'A') && (at_cmd[1] == 'T')) {

			// look at the next byte to determine what to do
			switch (at_cmd[2]) {
			case '\0':		// no command -> OK
				at_ok();
				break;
			case '&':
				at_ampersand();
				break;
			case '+':
				at_plus();
				break;
			case 'I':
				at_i();
				break;
			case 'O':		// O -> go online (exit command mode)
				at_plus_counter = ATP_COUNT_1S;
				at_mode_active = 0;
				break;
			case 'S':
				at_s();
				break;
			case 'Z':
				// generate a software reset
				CPU_RESTART;
				for (;;)
					;

			default:
				at_error();
			}
		}

		// unlock the command buffer
		at_cmd_len = 0;
		at_cmd_ready = false;
	}
}

static void
at_ok(void)
{
	s1printf("%s\n", "OK");
}

static void
at_error(void)
{
	s1printf("%s\n", "ERROR");
}

uint8_t		idx;
uint32_t	at_num;

/*
  parse a number at idx putting the result in at_num
 */
static void
at_parse_number() 
{
	 uint8_t c;

	at_num = 0;
	for (;;) {
		c = at_cmd[idx];
		if (!isdigit(c))
			break;
		at_num = (at_num * 10) + (c - '0');
		idx++;
	}
}

/*
static void print_ID_vals(char param, uint8_t end,
                          const char *(*name_param)(enum ParamID param),
                          param_t (*get_param)enum ParamID param)
                         )
{
   enum ParamID id;
  // convenient way of showing all parameters
  for (id = 0; id < end; id++) {
    s1printf("%c%u:%s=%lu\n",
      param,
      (unsigned)id,
      name_param(id),
      (unsigned long)get_param(id));
  }

}
*/
static void
at_i(void)
{
  switch (at_cmd[3]) {
  case '\0':
  case '0':
    s1printf("%s\n", g_banner_string);
    return;
  case '1':
    s1printf("%s\n", g_version_string);
    return;
  case '2':
    s1printf("%u\n", BOARD_ID);
    break;
  case '3':
    s1printf("%u\n", g_board_frequency);
    break;
  case '4':
    s1printf("%u\n", g_board_bl_version);
    return;
  case '5':
    //print_ID_vals('S', PARAM_MAX, param_name, param_get);
    return;
  case '6':
    //tdm_report_timing();
    return;
  case '7':
    //tdm_show_rssi();
    return;
  default:
    at_error();
    return;
  }
}

static void
at_s(void)
{
	uint8_t		sreg;

	// get the  number first
	idx = 3;
	at_parse_number();
	sreg = at_num;
	// validate the selected sreg
	if (sreg >= PARAM_MAX) {
		at_error();
		return;
	}

	switch (at_cmd[idx]) {
	case '?':
		//at_num = param_get(sreg);
		s1printf("%lu\n", at_num);
		return;

	case '=':
		if (sreg > 0) {
			idx++;
			at_parse_number();
			//if (param_set(sreg, at_num)) {
//				at_ok();
//				return;
//			}
		}
		break;
	}
	at_error();
}

static void
at_ampersand(void)
{
	switch (at_cmd[3]) {
	case 'F':
//		param_default();
		at_ok();
		break;
	case 'W':
//		param_save();
		at_ok();
		break;

	case 'U':
		if (!strcmp(at_cmd + 4, "PDATE")) {
			// Erase Flash signature forcing it into reprogram mode next reset
			//FLKEY = 0xa5;
			//FLKEY = 0xf1;
			//PSCTL = 0x03;				// set PSWE and PSEE
			//*(uint8_t *)FLASH_SIGNATURE_BYTES = 0xff;	// do the page erase
			//PSCTL = 0x00;				// disable PSWE/PSEE
			
			// Reset the device using sofware reset
			//RSTSRC |= 0x10;
			
			//for (;;)
			//	;
		}
		at_error();
		break;

	case 'P':
//		tdm_change_phase();
		break;

	case 'T':
		// enable test modes
		if (!strcmp(at_cmd + 4, "")) {
			// disable all tests
			at_testmode = 0;
		} else if (!strcmp(at_cmd + 4, "=RSSI")) {
			// display RSSI stats
			at_testmode ^= AT_TEST_RSSI;
		} else if (!strcmp(at_cmd + 4, "=TDM")) {
			// display TDM debug
			at_testmode ^= AT_TEST_TDM;
		} else {
			at_error();
		}
		break;
	default:
		at_error();
		break;
	}
}

static void
at_plus(void)
{
  at_error();
}
