/// return the next packet to be sent
///
/// @param max_xmit		maximum bytes that can be sent
/// @param buf			buffer to put bytes in
///
/// @return			number of bytes to send
extern uint8_t packet_get_next(uint8_t max_xmit, uint8_t *buf);

/// return true if the last packet was a resend
///
/// @return			true is a resend
extern bool packet_is_resend(void);

/// return true if the last packet was a injected packet
///
/// @return			true is injected
extern bool packet_is_injected(void);

/// determine if a received packet is a duplicate
///
/// @return			true if this is a duplicate
extern bool packet_is_duplicate(uint8_t len, uint8_t *buf, bool is_resend);

/// force the last packet to be re-sent. Used when packet transmit has
/// failed
extern void packet_force_resend(void);

/// set the maximum size of a packet
///
extern void packet_set_max_xmit(uint8_t max);

/// set the serial rate in bytes/s
///
/// @param  speed		serial speed bytes/s
///
extern void packet_set_serial_speed(uint16_t speed);

/// inject a packet to be sent when possible
/// @param buf			buffer to send
/// @param len			number of bytes
///			
extern void packet_inject(uint8_t *buf, uint8_t len);

// mavlink 1.0 and 2.0 markers
#define MAVLINK10_STX 254
#define MAVLINK20_STX 253

