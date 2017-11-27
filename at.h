///
/// @file	at.h
///
/// Prototypes for the AT command parser
///

#ifndef _AT_H_
#define _AT_H_

extern bool	at_cmd_ready;	///< if true, at_cmd / at_cmd_len contain valid data



/// AT command character input method.
///
/// Call this at interrupt time for every incoming character when at_mode_active
/// is true, and don't buffer those characters.
///
///  @param	c		Received character.
///
extern void	at_input(register uint8_t c);

/// Check for and execute AT commands
///
/// Call this from non-interrupt context when it's safe for an AT command
/// to be executed.  It's cheap if at_mode_active is false.
///
extern void	at_command(void);

/// AT_TEST_* test modes
extern uint8_t  at_testmode;    ///< AT_TEST_* bits

#define AT_TEST_RSSI 1
#define AT_TEST_TDM  2

// max size of an AT command
#define AT_CMD_MAXLEN	69

// AT command buffer
extern char at_cmd[AT_CMD_MAXLEN + 1];
extern uint8_t at_cmd_len;

#endif	// _AT_H_
