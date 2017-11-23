// In serial1.c (avr core) TX and RX buffer size changed to 1024 !
// In serial1.c and HardwareSerial.h peekx function is defined take care of them


//debug for development
#define DEBUG 1



#include <stdarg.h>
#include <EEPROM.h>
#include "radio.h"
//#include "tdm.h"
//#include "timer.h"
#include "freq_hopping.h"



const char g_banner_string[] = "RFD SiK " stringify(APP_VERSION_HIGH) "." stringify(APP_VERSION_LOW) " on " BOARD_NAME;
const char g_version_string[] = stringify(APP_VERSION_HIGH) "." stringify(APP_VERSION_LOW);

enum 			BoardFrequency	g_board_frequency;	///< board info from the bootloader
uint8_t			g_board_bl_version;	///< from the bootloader

/// Configure the Si1000 for operation.
///
static void hardware_init(void);

/// Initialise the radio and bring it online.
///
static void radio_init(void);

/// statistics for radio and serial errors
struct error_counts errors;
struct statistics statistics, remote_statistics;

/// optional features
bool feature_golay;
uint8_t feature_mavlink_framing;
bool feature_rtscts;

IntervalTimer ivtAtTimer;
IntervalTimer ivtTdmtimer;

void put_char(char c) {

Serial1.write(c);


}




void setup()
{
 
 //Start up USB serial connection for debug	
 delay(3000);
 Serial.begin(115200);
  debug("Starting..");
 



}



void loop()
{

	// Stash board info from the bootloader before we let anything touch
	// the SFRs.
	//
	g_board_frequency = FREQ_433;
	g_board_bl_version = BOARD_BL_VERSION_REG;


	debug("%s\n", g_banner_string);

	// Load parameters from flash or defaults
	// this is done before hardware_init() to get the serial speed
	/*
	if (!param_load()) {
		param_default();
		debug("Default Parameters loaded...\n");
		param_save();
		debug("And Saved...\n");
	}
	*/
	param_default();
	param_save();

	// setup boolean features
	feature_mavlink_framing = param_get(PARAM_MAVLINK);
	feature_golay = param_get(PARAM_ECC)?true:false;
	feature_rtscts = param_get(PARAM_RTSCTS)?true:false;

	// Do hardware initialisation.
	hardware_init();

	// do radio initialisation
	radio_init();

	//connect interrupt
	attachInterrupt(rfIRQ, rfInterrupt, FALLING);


	debug("Radio init success\r");
	//delay_set(2000);
	//debug("Start 2sec delay....");
	//while (!delay_expired());
	//debug("done\r");


	//if (radio_initialise()) {
	//		/debug("Success\n");
	//}


	// turn on the receiver
	if (!radio_receiver_on()) {
		panic("failed to enable receiver");
	}

	// Init user pins
	
	
	tdm_serial_loop();

  while(1);
}


static void
hardware_init(void)
{
	uint16_t	i;

    debug("start hardware init\n");

	//Led initialisation
	pinMode(LED_RADIO, OUTPUT);   //Set led for blinking
	pinMode(LED_BOOTLOADER, OUTPUT);
	pinMode(LED_DEBUG, OUTPUT);
   
	// SPI1
	pinMode(rfSEL, OUTPUT);
	pinMode(rfIRQ, INPUT);				

	spi4teensy3::init(2);				// This supposed to be 6Mhz bus speed, rfm22 is good up to 10Mhz
	rfSEL_DESELECT;
	
	
	// initialise timers
	ivtAtTimer.begin(hundredhztimer,10000);
	ivtTdmtimer.begin(timer2irq,15.65);

	// UART - set the configured speed
	serial_init(param_get(PARAM_SERIAL_SPEED));

	debug("Serial speed:%u\n", param_get(PARAM_SERIAL_SPEED));
	

	// Turn off the 'radio running' LED and turn off the bootloader LED
	setLed(LED_RADIO,LED_ON);
	setLed(LED_BOOTLOADER,LED_ON);
	setLed(LED_DEBUG, LED_ON);
	debug("hardware init successfull\n");
	delay(200);
	setLed(LED_RADIO,LED_OFF);
	setLed(LED_BOOTLOADER,LED_OFF);
	setLed(LED_DEBUG, LED_OFF);
	//at_testmode = AT_TEST_RSSI + AT_TEST_TDM;


}


void setLed(char ledPin, char ledValue)
{
  digitalWrite(ledPin, ledValue);

}

static void
radio_init(void)
{
	uint32_t freq_min, freq_max;
	uint32_t channel_spacing;
	uint8_t txpower;

	// Do generic PHY initialisation
	if (!radio_initialise()) {
		panic("radio_initialise failed");
	}

	switch (g_board_frequency) {
	case FREQ_433:
		freq_min = 433050000UL;
		freq_max = 434790000UL;
		txpower = 10;
		num_fh_channels = 10;
		break;
	case FREQ_470:
		freq_min = 470000000UL;
		freq_max = 471000000UL;
		txpower = 10;
		num_fh_channels = 10;
		break;
	case FREQ_868:
		freq_min = 868000000UL;
		freq_max = 870000000UL;
		txpower = 10;
		num_fh_channels = 10;
		break;
	case FREQ_915:
		freq_min = 915000000UL;
		freq_max = 928000000UL;
		txpower = 20;
		num_fh_channels = MAX_FREQ_CHANNELS;
		break;
	default:
		freq_min = 0;
		freq_max = 0;
		txpower = 0;
		panic("bad board frequency %d", g_board_frequency);
		break;
	}

	if (param_get(PARAM_NUM_CHANNELS) != 0) {
		num_fh_channels = param_get(PARAM_NUM_CHANNELS);
	}
	if (param_get(PARAM_MIN_FREQ) != 0) {
		freq_min        = param_get(PARAM_MIN_FREQ) * 1000UL;
	}
	if (param_get(PARAM_MAX_FREQ) != 0) {
		freq_max        = param_get(PARAM_MAX_FREQ) * 1000UL;
	}
	if (param_get(PARAM_TXPOWER) != 0) {
		txpower = param_get(PARAM_TXPOWER);
	}



	// constrain power and channels
	txpower = constrain(txpower, BOARD_MINTXPOWER, BOARD_MAXTXPOWER);
	num_fh_channels = constrain(num_fh_channels, 1, MAX_FREQ_CHANNELS);

	// double check ranges the board can do
	switch (g_board_frequency) {
	case FREQ_433:
		freq_min = constrain(freq_min, 414000000UL, 460000000UL);
		freq_max = constrain(freq_max, 414000000UL, 460000000UL);
		break;
	case FREQ_470:
		freq_min = constrain(freq_min, 450000000UL, 490000000UL);
		freq_max = constrain(freq_max, 450000000UL, 490000000UL);
		break;
	case FREQ_868:
		freq_min = constrain(freq_min, 849000000UL, 889000000UL);
		freq_max = constrain(freq_max, 849000000UL, 889000000UL);
		break;
	case FREQ_915:
		freq_min = constrain(freq_min, 868000000UL, 935000000UL);
		freq_max = constrain(freq_max, 868000000UL, 935000000UL);
		break;
	default:
		panic("bad board frequency %d", g_board_frequency);
		break;
	}

	if (freq_max == freq_min) {
		freq_max = freq_min + 1000000UL;
	}
	// get the duty cycle we will use
	duty_cycle = param_get(PARAM_DUTY_CYCLE);
	duty_cycle = constrain(duty_cycle, 0, 100);
	param_set(PARAM_DUTY_CYCLE, duty_cycle);

	// get the LBT threshold we will use
	lbt_rssi = param_get(PARAM_LBT_RSSI);
	if (lbt_rssi != 0) {
		// limit to the RSSI valid range
		lbt_rssi = constrain(lbt_rssi, 25, 220);
	}
	param_set(PARAM_LBT_RSSI, lbt_rssi);

	// sanity checks
	param_set(PARAM_MIN_FREQ, freq_min/1000);
	param_set(PARAM_MAX_FREQ, freq_max/1000);
	param_set(PARAM_NUM_CHANNELS, num_fh_channels);

	channel_spacing = (freq_max - freq_min) / (num_fh_channels+2);

	// add half of the channel spacing, to ensure that we are well
	// away from the edges of the allowed range
	freq_min += channel_spacing/2;

	// add another offset based on network ID. This means that
	// with different network IDs we will have much lower
	// interference
	sdcc_srand(param_get(PARAM_NETID));
	if (num_fh_channels > 5) {
		freq_min += ((unsigned long)(sdcc_rand()*625)) % channel_spacing;
	}
	debug("freq low=%lu high=%lu spacing=%lu\n", 
	       freq_min, freq_min+(num_fh_channels*channel_spacing), 
	       channel_spacing);

	// set the frequency and channel spacing
	// change base freq based on netid
	radio_set_frequency(freq_min);

	// set channel spacing
	radio_set_channel_spacing(channel_spacing);

	// start on a channel chosen by network ID
	radio_set_channel(param_get(PARAM_NETID) % num_fh_channels);

	// And intilise the radio with them.
	if (!radio_configure(param_get(PARAM_AIR_SPEED)) &&
	    !radio_configure(param_get(PARAM_AIR_SPEED)) &&
	    !radio_configure(param_get(PARAM_AIR_SPEED))) {
		panic("radio_configure failed");
	}

	// report the real air data rate in parameters
	param_set(PARAM_AIR_SPEED, radio_air_rate());

	// setup network ID
	radio_set_network_id(param_get(PARAM_NETID));

	// setup transmit power
	radio_set_transmit_power(txpower);
	
	// report the real transmit power in settings
	param_set(PARAM_TXPOWER, radio_get_transmit_power());

	// initialise frequency hopping system
	fhop_init();

	// initialise TDM system
	tdm_init();
debug("Radio init successfull\n");


}