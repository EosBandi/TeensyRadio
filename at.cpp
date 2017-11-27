///
/// @file	at.c
///
/// A simple AT command parser.
///

#include "radio.h"
#include "tdm.h"
#include "at.h"

//Uncomment all tdm_command and param save-reads


// canary data for ram wrap. It is in at.c as the compiler
// assigns addresses in alphabetial order and we want this at a low
// address

// AT command buffer
char at_cmd[AT_CMD_MAXLEN + 1];
uint8_t	at_cmd_len;

// mode flags
bool		at_cmd_ready;	///< if true, at_cmd / at_cmd_len contain valid data

// test bits
uint8_t		at_testmode;    ///< test modes enabled (AT_TEST_*)

// command handlers
static void	at_ok(void);
static void	at_error(void);
static void	at_i(void);
static void	at_s(void);
static void	at_ampersand(void);
static void	at_plus(void);
static void at_help(void);

/**
 * @brief Get input caracter from serial link and process it as a command, sets at_cmd_ready when \r received
 * 
 * @param c uint8_t character to process
 */
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

		at_cmd_len = 0;
		break;

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
			case 'S':
				at_s();
				break;
			case 'H':
				at_help();
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
	s1printf("OK\n");
}

static void
at_error(void)
{
	s1printf("ERROR\n");
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


static void print_params( void )
{
   uint8_t id;
  // convenient way of showing all parameters
  for (id = 0; id < PARAM_MAX; id++) {
    s1printf("S%u:%s=%lu\n", (unsigned)id, param_name(id), (unsigned long)param_get(id));
  }

}


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
    print_params();
    return;
  case '6':
    tdm_report_timing();
    return;
  case '7':
    tdm_show_rssi();
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
		at_num = param_get(sreg);
		s1printf("%lu\n", at_num);
		return;

	case '=':
		if (sreg > 0) {
			idx++;
			at_parse_number();
			if (param_set(sreg, at_num)) {
				at_ok();
				return;
			}
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
		param_default();
		at_ok();
		break;
	case 'W':
		param_save();
		at_ok();
		break;
	case 'P':
		tdm_change_phase();
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


static void at_help(void)
{
	s1printf("Available commands:\n");
	s1printf("ATI# Where # is:\n");
	s1printf("      0 - sw banner  | 1 - sw version | 2 - board id\n");
	s1printf("      3 - frequency  | 4 - board ver. | 5 - all parameters\n");
	s1printf("      6 - tdm timing | 7 - show rssi  | \n");
	s1printf("ATS#?     - Get parameter # value\n");
	s1printf("ATS#=val  - Set parameter # value to val\n");
	s1printf("AT&W      - Save parameters to EEPROM\n");
	s1printf("AT&F      - Load defalt parameters (no save)\n");
	s1printf("AT&P      - Change TDM phase\n");
	s1printf("AT&T=RSSI - toggle RSSI debug reporting\n");
	s1printf("AT&T=TDM  - toggle TDM debug reporting\n");
	s1printf("ATZ       - Reset radio\n");
	s1printf("ATH       - This help\n");




}