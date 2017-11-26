// Definitions for radio module and teensy hardware
 


#ifndef _BOARD_H_
#define _BOARD_H_


//Tennsy 3.2 board software reset

#define CPU_RESTART_ADDR (uint32_t *)0xE000ED0C
#define CPU_RESTART_VAL 0x5FA0004
#define CPU_RESTART (*CPU_RESTART_ADDR = CPU_RESTART_VAL);


enum BoardFrequency {
    FREQ_433	= 0x43,
    FREQ_470	= 0x47,
    FREQ_868	= 0x86,
    FREQ_915	= 0x91,
    FREQ_NONE	= 0xf0,
};


#define BOARD_FREQUENCY_REG	    0x43	// board frequency
#define BOARD_BL_VERSION_REG	0x01		// bootloader version

#define BOARD_MINTXPOWER 0		// Minimum transmit power level
#define BOARD_MAXTXPOWER 20		// Maximum transmit power level

#define BOARD_NAME "RacSik"
#define BOARD_ID 197209


//Led pin definitions
#define LED_RADIO       21
#define LED_BOOTLOADER  22
#define LED_DEBUG       23

#define LED_OFF LOW
#define LED_ON HIGH


#define EZRADIOPRO_OSC_CAP_VALUE 0xb4	// Per HRF demo code
#define ENABLE_RFM50_SWITCH	1	// Per HRF demo code, verified presence of RF switch on the RFM50 module


//SPI configuration for Teensy board
//RFM23xx module connected to the SPI1 port using standard pin configuration
// SDO - 11, SDI - 12, SCK - 13
//SDN is not connected, IRQ and SEL are connected as follows

#define rfSEL                   14      // rfm module select active LOW
#define rfIRQ                   15      // rfm module irq active LOW

//Macros for enabling and disabling the SEL line of RFM23xx module
#define rfSEL_SELECT             digitalWrite(rfSEL, LOW)
#define rfSEL_DESELECT           digitalWrite(rfSEL, HIGH);


//CTS and RTS pins for serial 1
#define CTS_PIN         20                      //It could be only 18 or 20 - will become input
#define RTS_PIN         2                       //It can be any digital pin - will become output


#endif