///
/// @file	flash.h
///		Prototypes for the flash interface.
///



extern void	flash_erase_scratch(void);
extern uint8_t	flash_read_scratch(uint16_t address);
extern void	flash_write_scratch(uint16_t address, uint8_t c);
