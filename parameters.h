///
/// @file	parameters.h
///
/// Definitions for program parameter storage.
///

/// Parameter IDs.
///
/// Parameter IDs here match AT S-register numbers, so change them with extreme
/// care.  Parameter zero cannot be written by AT commands.
///
/// Note that this enumeration is used to initialise the parameter_names
/// array, and parameters not listed in that array will not be visible by name.
///
/// When adding or removing a parameter here, you must also update:
///
///   parameters.c:parameter_names[]
///   parameters.c:param_check()
///
	
#define		PARAM_FORMAT 			0		// Must always be parameter 0
#define		PARAM_SERIAL_SPEED		1		// BAUD_RATE_* constant
#define		PARAM_AIR_SPEED			2		// over the air baud rate
#define		PARAM_NETID				3		// network ID
#define		PARAM_TXPOWER			4		// transmit power (dBm)
#define		PARAM_ECC				5		// ECC using golay encoding
#define		PARAM_MAVLINK			6		// MAVLink framing, 0=ignore, 1=use, 2=rc-override
#define		PARAM_OPPRESEND			7		// opportunistic resend // DISABLED
#define		PARAM_MIN_FREQ			8		// min frequency in MHz
#define		PARAM_MAX_FREQ			9		// max frequency in MHz
#define		PARAM_NUM_CHANNELS		10		// number of hopping channels
#define		PARAM_DUTY_CYCLE		11		// duty cycle (percentage)
#define		PARAM_LBT_RSSI			12		// listen before talk threshold
#define		PARAM_MANCHESTER		13		// enable manchester encoding
#define		PARAM_RTSCTS			14		// enable hardware flow control
#define		PARAM_MAX_WINDOW		15		// The maximum window size allowed
#define     PARAM_SBUS_FUNCTION     16      // SBUS function - 0-disabled, 1-SBUS input, 2-SBUS-output
#define     PARAM_SBUS_FS_CH1       17
#define     PARAM_SBUS_FS_CH2       18
#define     PARAM_SBUS_FS_CH3       19
#define     PARAM_SBUS_FS_CH4       20
#define     PARAM_SBUS_FS_CH5       21
#define     PARAM_SBUS_FS_CH6       22
#define     PARAM_SBUS_FS_CH7       23
#define     PARAM_SBUS_FS_CH8       24
#define		PARAM_MAX				25		// must be last

#define PARAM_FORMAT_CURRENT	0x1cUL				///< current parameter format ID

/// Parameter type.
///
/// All parameters have this type.
///
typedef uint32_t	param_t;

/// Set a parameter
///
/// @note Parameters are not saved until param_save is called.
///
/// @param	param		The parameter to set.
/// @param	value		The value to assign to the parameter.
/// @return			True if the parameter's value is valid.
///
extern bool param_set(uint8_t param, param_t value);

/// Get a parameter
///
/// @param	param		The parameter to get.
/// @return			The parameter value, or zero if the param
///				argument is invalid.
///
extern param_t param_get(uint8_t param);

/// Look up a parameter by name
///
/// @param	name		The parameter name
/// @return			The parameter ID, or PARAM_MAX if the
///				parameter is not known.
///
extern uint8_t param_id(char * name);

/// Return the name of a parameter.
///
/// @param	param		The parameter ID to look up.
/// @return			A pointer to the name of the parameter,
///				or NULL if the parameter is not known.
///
extern const char * param_name(uint8_t param);

/// Load parameters from the flash scratchpad.
///
/// @return			True if parameters were successfully loaded.
///
extern bool param_load(void);

/// Save parameters to the flash scratchpad.
///
extern void param_save(void);

/// Reset parameters to default.
///
/// Note that this just resets - it does not save.
///
extern void param_default(void);

/// convenient routine to constrain parameter values
uint32_t constrain_(uint32_t v, uint32_t min, uint32_t max);

