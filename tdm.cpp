///
/// @file	tdm.c
///
/// time division multiplexing code
///


#include <stdarg.h>
#include "radio.h"
#include "tdm.h"
//#include "timer.h"
//#include "packet.h"
#include "golay.h"
#include "freq_hopping.h"
#include "crc.h"
#include "serial.h"

#define USE_TICK_YIELD 1

/// the state of the tdm system
#define TDM_TRANSMIT 	0
#define TDM_SILENCE1	1
#define TDM_RECEIVE		2
#define TDM_SILENCE2	3

//enum tdm_state { TDM_TRANSMIT=0, TDM_SILENCE1=1, TDM_RECEIVE=2, TDM_SILENCE2=3 };
static char tdm_state;

/// a packet buffer for the TDM code
 uint8_t	pbuf[MAX_PACKET_LENGTH];

/// packet buffer sor sbus insertion
 uint8_t sbuf[10];


/// how many 16usec ticks are remaining in the current state
static uint16_t tdm_state_remaining;

/// This is enough to hold at least 3 packets and is based
/// on the configured air data rate.
static uint16_t tx_window_width;

/// the maximum data packet size we can fit
static uint8_t max_data_packet_length;

/// the silence period between transmit windows
/// This is calculated as the number of ticks it would take to transmit
/// two zero length packets
static uint16_t silence_period;

/// whether we can transmit in the other radios transmit window
/// due to the other radio yielding to us
static char bonus_transmit;

/// whether we have yielded our window to the other radio
static char transmit_yield;

// activity indication
// when the 16 bit timer2_tick() value wraps we check if we have received a
// packet since the last wrap (ie. every second)
// If we have the green radio LED is held on.
// Otherwise it blinks every 1 seconds. The received_packet flag
// is set for any received packet, whether it contains user data or
// not.
static char blink_state;
static char received_packet;

/// the latency in 16usec timer2 ticks for sending a zero length packet
static uint16_t packet_latency;

/// the time in 16usec ticks for sending one byte
static uint16_t ticks_per_byte;

/// number of 16usec ticks to wait for a preamble to turn into a packet
/// This is set when we get a preamble interrupt, and causes us to delay
/// sending for a maximum packet latency. This is used to make it more likely
/// that two radios that happen to be exactly in sync in their sends
/// will eventually get a packet through and get their transmit windows
/// sorted out
uint16_t transmit_wait;

/// the long term duty cycle we are aiming for
uint8_t duty_cycle;

/// the average duty cycle we have been transmitting
static float average_duty_cycle;

/// duty cycle offset due to temperature
uint8_t duty_cycle_offset = 0;

/// set to true if we need to wait for our duty cycle average to drop
static bool duty_cycle_wait;

/// how many ticks we have transmitted for in this TDM round
static uint16_t transmitted_ticks;

/// the LDB (listen before talk) RSSI threshold
uint8_t lbt_rssi;

/// how long we have listened for for LBT
static uint16_t lbt_listen_time;

/// how long we have to listen for before LBT is OK
static uint16_t lbt_min_time;

/// random addition to LBT listen time (see European regs)
static uint16_t lbt_rand;

/// test data to display in the main loop. Updated when the tick
/// counter wraps, zeroed when display has happened
 uint8_t test_display;

/// set when we should send a statistics packet on the next round
static char send_statistics;

/// set when we should send a MAVLink report pkt
extern bool seen_mavlink;

struct tdm_trailer {
	uint16_t window:13;
	uint16_t injected:1;
	uint16_t bonus:1;
  uint16_t resend:1;
  uint8_t  stream_id;          //Identify the stream that we are receiving, based on the stream id we can send it to a separated output
};
struct tdm_trailer trailer;

#define PACKET_OVERHEAD (sizeof(trailer)+16)

/// display RSSI output
///
void
tdm_show_rssi(void)
{
  
  static unsigned long t = millis();
  static unsigned long last_packets = 0;


  unsigned long td;
  unsigned long sp;
  
  td = millis() - t;
  t = millis();

  sp = sbus_packets_sent - last_packets;
  last_packets = sbus_packets_sent;

  s1printf("Time:%u SBUS Packets:%u ",td,sp);

	s1printf("L/R RSSI: %u/%u  L/R noise: %u/%u pkts: %u\n",
	       (unsigned)statistics.average_rssi,
	       (unsigned)remote_statistics.average_rssi,
	       (unsigned)statistics.average_noise,
	       (unsigned)remote_statistics.average_noise,
	       (unsigned)statistics.receive_count);
  	s1printf(" txe=%u rxe=%u stx=%u srx=%u ecc=%u/%u temp=%d dco=%u\n",
	       (unsigned)errors.tx_errors,
	       (unsigned)errors.rx_errors,
	       (unsigned)errors.serial_tx_overflow,
	       (unsigned)errors.serial_rx_overflow,
	       (unsigned)errors.corrected_errors,
	       (unsigned)errors.corrected_packets,
	       (int)radio_temperature(),
	       (unsigned)duty_cycle_offset);

	statistics.receive_count = 0;
}

/// display test output
///
static void
display_test_output(void)
{
//  if (test_display & AT_TEST_RSSI) {
    tdm_show_rssi();
//  }
}


/// estimate the flight time for a packet given the payload size
///
/// @param packet_len		payload length in bytes
///
/// @return			flight time in 16usec ticks
static uint16_t flight_time_estimate(uint8_t packet_len)
{
  return packet_latency + (packet_len * ticks_per_byte);
}


/// synchronise tx windows
///
/// we receive a 16 bit value with each packet which indicates how many
/// more 16usec ticks the sender has in their transmit window. The
/// sender has already adjusted the value for the flight time
///
/// The job of this function is to adjust our own transmit window to
/// match the other radio and thus bring the two radios into sync
///
static void
sync_tx_windows(uint8_t packet_length)
{
  char old_state = tdm_state;
  uint16_t old_remaining = tdm_state_remaining;
  
  if (trailer.bonus) {
    // the other radio is using our transmit window
    // via yielded ticks
    if (old_state == TDM_SILENCE1) {
      // This can be caused by a packet
      // taking longer than expected to arrive.
      // don't change back to transmit state or we
      // will cause an extra frequency change which
      // will get us out of sequence
      tdm_state_remaining = silence_period;
    } else if (old_state == TDM_RECEIVE || old_state == TDM_SILENCE2) {
      // this is quite strange. We received a packet
      // so we must have been on the right
      // frequency. Best bet is to set us at the end
      // of their silence period
      tdm_state = TDM_SILENCE2;
      tdm_state_remaining = 1;
    } else {
      tdm_state = TDM_TRANSMIT;
      tdm_state_remaining = trailer.window;
    }
  } else {
    // we are in the other radios transmit window, our
    // receive window
    tdm_state = TDM_RECEIVE;
    tdm_state_remaining = trailer.window;
  }
  
  // if the other end has sent a zero length packet and we are
  // in their transmit window then they are yielding some ticks to us.
  bonus_transmit = (tdm_state == TDM_RECEIVE && packet_length==0);
  
  // if we are not in transmit state then we can't be yielded
  if (tdm_state != TDM_TRANSMIT) {
    transmit_yield = 0;
  }
  
  if (at_testmode & AT_TEST_TDM) {
    int16_t delta;
    delta = old_remaining - tdm_state_remaining;
    if (old_state != tdm_state ||
        delta > (int16_t)packet_latency/2 ||
        delta < -(int16_t)packet_latency/2) {
      s1printf("TDM: %u/%u len=%u ",
          (unsigned)old_state,
          (unsigned)tdm_state,
          (unsigned)packet_length);
      s1printf(" delta: %d\n",(int)delta);
    }
  }
}

/// update the TDM state machine
///
static void
tdm_state_update(uint16_t tdelta)
{
  // update the amount of time we are waiting for a preamble
  // to turn into a real packet
  if (tdelta > transmit_wait) {
    transmit_wait = 0;
  } else {
    transmit_wait -= tdelta;
  }
  
  // have we passed the next transition point?
  while (tdelta >= tdm_state_remaining) {
	// advance the tdm state machine
	
    tdm_state =(tdm_state+1) % 4;
    
    // work out the time remaining in this state
    tdelta -= tdm_state_remaining;
    
    if (tdm_state == TDM_TRANSMIT || tdm_state == TDM_RECEIVE) {
      tdm_state_remaining = tx_window_width;
    } else {
      tdm_state_remaining = silence_period;
    }
    
    // change frequency at the start and end of our transmit window
    // this maximises the chance we will be on the right frequency
    // to match the other radio
    if (tdm_state == TDM_TRANSMIT || tdm_state == TDM_SILENCE1) {
      fhop_window_change();
      radio_receiver_on();
      
      if (num_fh_channels > 1) {
        // reset the LBT listen time
        lbt_listen_time = 0;
        lbt_rand = 0;
      }
    }
    
    if (tdm_state == TDM_TRANSMIT && (duty_cycle - duty_cycle_offset) != 100) {
      // update duty cycle averages
      average_duty_cycle = (0.95*average_duty_cycle) + (0.05*(100.0*transmitted_ticks)/(2*(silence_period+tx_window_width)));
      transmitted_ticks = 0;
      duty_cycle_wait = (average_duty_cycle >= (duty_cycle - duty_cycle_offset));
    }
    
    // we lose the bonus on all state changes
    bonus_transmit = 0;
    
    // reset yield flag on all state changes
    transmit_yield = 0;
    
    // no longer waiting for a packet
    transmit_wait = 0;
  }
  
  tdm_state_remaining -= tdelta;
}

/// change tdm phase
///
void
tdm_change_phase(void)
{
  tdm_state =(tdm_state+2) % 4;
}

/// called to check temperature
///
/// Temperatur based duty cycle offset is not implemented YET! But the code is keeped here for later use 
/*
static void temperature_update(void)
{
  register int16_t diff;
  if (radio_get_transmit_power() <= 20) {
    duty_cycle_offset = 0;
    return;
  }
  
  diff = radio_temperature() - MAX_PA_TEMPERATURE;
  if (diff <= 0 && duty_cycle_offset > 0) {
    // under temperature
    duty_cycle_offset -= 1;
  } else if (diff > 10) {
    // getting hot!
    duty_cycle_offset += 10;
  } else if (diff > 5) {
    // well over temperature
    duty_cycle_offset += 5;
  } else if (diff > 0) {
    // slightly over temperature
    duty_cycle_offset += 1;				
  }
  // limit to minimum of 20% duty cycle to ensure link stays up OK
  if ((duty_cycle-duty_cycle_offset) < 20) {
    duty_cycle_offset = duty_cycle - 20;
  }
}
*/

/// blink the radio LED if we have not received any packets
///
static void
link_update(void)
{

  static uint8_t unlock_count = 10;
  //static uint8_t temperature_count;

  if (received_packet) {
    unlock_count = 0;
    received_packet = false;
  } else {
    unlock_count++;
  }
  
  if (unlock_count < 2) {
    setLed(LED_RADIO,LED_ON);
  } else {
    
    setLed(LED_RADIO,blink_state);
    blink_state = !blink_state;
  }
  
  if (unlock_count > 40) {
    // if we have been unlocked for 20 seconds
    // then start frequency scanning again
    
    unlock_count = 5;
    // randomise the next transmit window using some
    // entropy from the radio if we have waited
    // for a full set of hops with this time base
    if (timer_entropy() & 1) {
      register uint16_t old_remaining = tdm_state_remaining;
      if (tdm_state_remaining > silence_period) {
        tdm_state_remaining -= packet_latency;
      } else {
        tdm_state_remaining = 1;
      }
      if (at_testmode & AT_TEST_TDM) {
        s1printf("TDM: change timing %u/%u\n",
                (unsigned)old_remaining,
                (unsigned)tdm_state_remaining);
      }
    }
    
    if (at_testmode & AT_TEST_TDM) {
      s1printf("TDM: scanning\n");
    }
    fhop_set_locked(false);
  }
  
  if (unlock_count != 0) {
    statistics.average_rssi = (radio_last_rssi() + 3*(uint16_t)statistics.average_rssi)/4;
    
    // reset statistics when unlocked
    statistics.receive_count = 0;
  }
  
  if (unlock_count > 5) {
    memset(&remote_statistics, 0, sizeof(remote_statistics));
  }
  
  test_display = at_testmode;
  send_statistics = 1;
  /*
  temperature_count++;
  if (temperature_count == 4) {
    // check every 2 seconds
    temperature_update();
    temperature_count = 0;
  }
  */
}

/// main loop for time division multiplexing transparent serial
///
void
tdm_serial_loop(void)
{
  uint8_t	len;
  uint16_t tnow, tdelta, sdelta;
  uint8_t max_xmit;
  uint16_t last_t = timer2_tick();
  uint16_t last_link_update = last_t;
  uint16_t last_sbus = last_t;
  

  uint16_t last_sbus_received = 0;
  uint32_t last_sbus_out = 0;
  uint32_t sbus_last_seen  = 0;

  uint32_t mnow = 0;

  bool sbus_failsafe = 0;


  
  for (;;) {


    // display test data if needed
    if (test_display) {
      display_test_output();
      test_display = 0;
    } 
    
    // If we seen a MAVLink heartbeat then send the MAVLink report package and reset seen_mavlink to wait for another heartbeat
    if (seen_mavlink && feature_mavlink_framing ) {
      seen_mavlink = false;
      MAVLink_report();
    }
  
    // set right receive channel
    radio_set_channel(fhop_receive_channel());
    
    // get the time before we check for a packet coming in
    tnow = timer2_tick();
    mnow = millis();

    //sbus passthrough RX handling
	//Check Failsafe, and write sbus output
    if (feature_sbus == SBUS_FUNCTION_RX)
    {

	  //Check failsafe
      if ( (mnow - sbus_last_seen) >= param_get(PARAM_SBUS_FS_TIMEOUT))
      {
        sbus_failsafe = 1;
        //Set failsafe channel values;
        uint8_t i;
        for (i = 0; i < 8; i++)
        {
          sbus_channels[i] = param_get(PARAM_SBUS_FS_CH1 + i) << 3;
        }
      }
      else
      {
        // NO failsafe
        sbus_failsafe = 0;
      }

	  //output sbus channel values in every 50ms, regardless of receiving status (Failsafe set in the code above)
      if ((mnow - last_sbus_out) >= 50) // 50ms
      {
        sbus_write(sbus_failsafe);
        last_sbus_out = mnow;
      }


    }  //End SBUS RX part 1

    // see if we have received a packet, radio_receive_packet gives back golay decoded packets, so the length is adjusted accordingly
    if (radio_receive_packet(&len, pbuf)) {
      // update the activity indication
      received_packet = true;
      fhop_set_locked(true);
      
      // update filtered RSSI value and packet stats
      statistics.average_rssi = (radio_last_rssi() + 7*(uint16_t)statistics.average_rssi)/8;
      statistics.receive_count++;
      
      // we're not waiting for a preamble
      // any more
      transmit_wait = 0;
      
        // not a valid packet. We always send
        // three control bytes at the end of every packet (modified from two since we added the stream byte to the trailer)
      if (len < 3) { continue; }
      
      // extract control bytes from end of packet, we assume that the last bytes are the trailer
      memcpy(&trailer, &pbuf[len-sizeof(trailer)], sizeof(trailer));

      // Deduct trailer length from the payload length
      len -= sizeof(trailer);
      
      if (trailer.window == 0 && len != 0) {
        // its a control packet
        if (len == sizeof(struct statistics)) {
          memcpy(&remote_statistics, pbuf, len);
        }
    
        // don't count control packets in the stats
        statistics.receive_count--;
      } else if (trailer.window != 0) {
        // sync our transmit windows based on
        // received header
        sync_tx_windows(len);
        last_t = tnow;
        
        //we have to use packet_is_duplicate since it is possible and this populate the last_received buffer
        if (trailer.injected == 1 && len != 0 && !packet_is_duplicate(len, pbuf, trailer.resend))     
        {
		// TODO: Check sbus stream id 
		if (feature_sbus == SBUS_FUNCTION_RX)
          {
            setLed(LED_BOOTLOADER,LED_ON);
           
            sbus_channels[0] = pbuf[0]<<3;
            sbus_channels[1] = pbuf[1]<<3;
            sbus_channels[2] = pbuf[2]<<3;
            sbus_channels[3] = pbuf[3]<<3;
            sbus_channels[4] = pbuf[4]<<3;
            sbus_channels[5] = pbuf[5]<<3;
            sbus_channels[6] = pbuf[6]<<3;
            sbus_channels[7] = pbuf[7]<<3;
            sbus_last_seen = mnow;
            setLed(LED_BOOTLOADER,LED_OFF);
          }
          continue;   //add back control to the main for loop.
        }

		// Ok we have a payload  and it belongs to the data stream (not injected)
        if (len != 0 && trailer.injected == 0 && !packet_is_duplicate(len, pbuf, trailer.resend) )
        {
            //TODO: Check trailer.stream and reroute the incoming payload to the appropiate output stream
              setLed(LED_DEBUG, LED_ON);
              Serial1.write(pbuf, len);
              setLed(LED_DEBUG,LED_OFF);
        }
      }
      continue;
    }   //radio_receive_packet
    
    // At this point we did not received a packet, so time to check that are we allowed to transmit ?

    // see how many 16usec ticks have passed and update
    // the tdm state machine. We re-fetch tnow as a bad
    // packet could have cost us a lot of time.
    tnow = timer2_tick();
    tdelta = tnow - last_t;
    tdm_state_update(tdelta);
    last_t = tnow;
    uint16_t testt;
    testt = tnow-last_link_update;
    sdelta = tnow - last_sbus;    

       // update link status every 0.5s
    if (testt > 32768) {
      link_update();
      last_link_update = tnow;
    }


    if (lbt_rssi != 0) {
      // implement listen before talk
      if (radio_current_rssi() < lbt_rssi) {
        lbt_listen_time += tdelta;
      } else {
        lbt_listen_time = 0;
        if (lbt_rand == 0) {
          lbt_rand = ((uint16_t)sdcc_rand()) % lbt_min_time;
        }
      }
      if (lbt_listen_time < lbt_min_time + lbt_rand) {
        // we need to listen some more
        continue;
      }
    }
    
    // we are allowed to transmit in our transmit window
    // or in the other radios transmit window if we have
    // bonus ticks
#if USE_TICK_YIELD
    if (tdm_state != TDM_TRANSMIT &&
          !(bonus_transmit && tdm_state == TDM_RECEIVE)) {
      // we cannot transmit now
      continue;
    }
#else
    if (tdm_state != TDM_TRANSMIT) {
      continue;
    }		
#endif
    
    if (transmit_yield != 0) {
      // we've give up our window
      continue;
    }
    
    if (transmit_wait != 0) {
      // we're waiting for a preamble to turn into a packet
      continue;
    }
    
    if ( ( (!received_packet) && radio_preamble_detected() ) || radio_receive_in_progress() ) {
      // a preamble has been detected. Don't
      // transmit for a while
      transmit_wait = packet_latency;
      continue;
    }
    
    // sample the background noise when it is out turn to
    // transmit, but we are not transmitting,
    // averaged over around 4 samples
    statistics.average_noise = (radio_current_rssi() + 3*(uint16_t)statistics.average_noise)/4;
    
    if (duty_cycle_wait) {
      // we're waiting for our duty cycle to drop
      continue;
    }
    
    // how many bytes could we transmit in the time we
    // have left?
    if (tdm_state_remaining < packet_latency) {
      // none ....
      continue;
    }

    max_xmit = (tdm_state_remaining - packet_latency) / ticks_per_byte;

    if (max_xmit < PACKET_OVERHEAD) {
      // can't fit the trailer in with a byte to spare
      continue;
    }

    //max_xmit -= PACKET_OVERHEAD;
    max_xmit -= sizeof(trailer)+1;
        
    if (max_xmit > max_data_packet_length) {
      max_xmit = max_data_packet_length;
    }
        
  if ( (feature_sbus == SBUS_FUNCTION_TX) && (max_xmit >= 9) && (sdelta > 3500) )  {

        if (sbus_read()) {

         setLed(LED_BOOTLOADER,LED_ON);

          sbuf[0] = sbus_channels[0]>>3; //change resolution to 8 bit
          sbuf[1] = sbus_channels[1]>>3;
          sbuf[2] = sbus_channels[2]>>3;
          sbuf[3] = sbus_channels[3]>>3;
          sbuf[4] = sbus_channels[4]>>3;
          sbuf[5] = sbus_channels[5]>>3;
          sbuf[6] = sbus_channels[6]>>3;
          sbuf[7] = sbus_channels[7]>>3;
          sbuf[8] = sbus_channels[8]>>3;
          packet_inject(sbuf, 9);
          last_sbus = tnow;
          sbus_packets_sent++;

          setLed(LED_BOOTLOADER,LED_OFF);

         }
       }

    // ask the packet system for the next packet to send
    // get a packet from the serial port
    len = packet_get_next(max_xmit, pbuf);
    
    if (len > 0) {
       trailer.injected = packet_is_injected();
    } else {
       trailer.injected = 0;
    }
    

    if (len > max_data_packet_length) {
      panic("oversized tdm packet");
    }
    
    trailer.bonus = (tdm_state == TDM_RECEIVE);
    trailer.resend = packet_is_resend();
    
    if (tdm_state == TDM_TRANSMIT &&
            len == 0 &&
            send_statistics &&
            max_xmit >= sizeof(statistics)) {
      // send a statistics packet
      send_statistics = 0;
      memcpy(pbuf, &statistics, sizeof(statistics));
      len = sizeof(statistics);
      
      // mark a stats packet with a zero window
      trailer.window = 0;
      trailer.resend = 0;
    } else {
      // calculate the control word as the number of
      // 16usec ticks that will be left in this
      // tdm state after this packet is transmitted
      
      trailer.window = (uint16_t)(tdm_state_remaining - flight_time_estimate(len+sizeof(trailer)));
    }
    
    // set right transmit channel
    radio_set_channel(fhop_transmit_channel());
    
    memcpy(&pbuf[len], &trailer, sizeof(trailer));  //Add trailer to the packet buffer
    
    if (len != 0 && trailer.window != 0) {
      // show the user that we're sending real data
     // setLed(LED_BOOTLOADER,LED_ON);
    }
    
    if (len == 0) {
      // sending a zero byte payload packet gives up
      // our window, but doesn't change the
      // start of the next window
      transmit_yield = 1;
    }
    
    // after sending a packet leave a bit of time before
    // sending the next one. The receivers don't cope well
    // with back to back packets
    transmit_wait = packet_latency;
    
    // if we're implementing a duty cycle, add the
    // transmit time to the number of ticks we've been transmitting
    if ((duty_cycle - duty_cycle_offset) != 100) {
      transmitted_ticks += flight_time_estimate(len+sizeof(trailer));
    }
    // start transmitting the packet
    //

    //If radio_transmit returns false (Timeot, buffer over on under run)
    //Az adathossz nem volt 0, a trailer.windows nem 0 es trailer injected=0 (nem injected) akkor kuldjuk ujra.
    //FIXME: Do we really does not resend injected packets ????
    if (!radio_transmit(len + sizeof(trailer), pbuf, tdm_state_remaining + (silence_period/2)) &&
        len != 0 && trailer.window != 0 && trailer.injected == 0) {
        debug("Force resend\n");
      packet_force_resend();
    }
    
    if (lbt_rssi != 0) {
      // reset the LBT listen time
      lbt_listen_time = 0;
      lbt_rand = 0;
    }
    
    if (len != 0 && trailer.window != 0) {
      //setLed(LED_BOOTLOADER ,LED_OFF);
    }

    // set right receive channel
    radio_set_channel(fhop_receive_channel());
    
    // re-enable the receiver
    radio_receiver_on();
    
  }
}



// initialise the TDM subsystem
void
tdm_init(void)
{
  uint16_t i;
  uint8_t air_rate = radio_air_rate();
  uint32_t window_width;
  
#define REGULATORY_MAX_WINDOW (((1000000UL/16)*4)/10)
#define LBT_MIN_TIME_USEC 5000

  //Make sure that there is no duty cycle offset
  duty_cycle_offset = 0;
  
	// calculate how many 16usec ticks it takes to send each byte
	ticks_per_byte = (8+ (8000000UL/ (air_rate*1000UL) ) ) /16;      //from 16
        ticks_per_byte++;

	// calculate the minimum packet latency in 16 usec units
	// we initially assume a preamble length of 40 bits, then
	// adjust later based on actual preamble length. This is done
	// so that if one radio has antenna diversity and the other
	// doesn't, then they will both using the same TDM round timings
	packet_latency = (8+(10/2)) * ticks_per_byte + 13;

	if (feature_golay) {
		max_data_packet_length = (MAX_PACKET_LENGTH/2) - (6+sizeof(trailer));

		// golay encoding doubles the cost per byte
		ticks_per_byte *= 2;

		// and adds 4 bytes
		packet_latency += 4*ticks_per_byte;
	} else {
		max_data_packet_length = MAX_PACKET_LENGTH - sizeof(trailer);
	}

	// set the silence period to two times the packet latency
        silence_period = 2*packet_latency;

        // set the transmit window to allow for 3 full sized packets
	window_width = 3*(packet_latency+(max_data_packet_length*(uint32_t)ticks_per_byte));

        // min listen time is 5ms
        lbt_min_time = LBT_MIN_TIME_USEC/16;
        
	// if LBT is enabled, we need at least 3*5ms of window width
	if (lbt_rssi != 0) {
		window_width = constrain_(window_width, 3*lbt_min_time, window_width);
	}

	// the window width cannot be more than 0.4 seconds to meet US
	// regulations
	if (window_width >= REGULATORY_MAX_WINDOW && num_fh_channels > 1) {
		window_width = REGULATORY_MAX_WINDOW;
	}

	// user specified window is in milliseconds
	if (window_width > param_get(PARAM_MAX_WINDOW)*(1000/16)) {
		window_width = param_get(PARAM_MAX_WINDOW)*(1000/16);
	}

	// make sure it fits in the 13 bits of the trailer window
	if (window_width > 0x1fff) {
		window_width = 0x1fff;
	}

	tx_window_width = window_width;

	// now adjust the packet_latency for the actual preamble
	// length, so we get the right flight time estimates, while
	// not changing the round timings
	packet_latency += ((settings.preamble_length-10)/2) * ticks_per_byte;

	// tell the packet subsystem our max packet size, which it
	// needs to know for MAVLink packet boundary detection
	i = (tx_window_width - packet_latency) / ticks_per_byte;
	if (i > max_data_packet_length) {
		i = max_data_packet_length;
	}
	packet_set_max_xmit(i);

}

/// report tdm timings
///
void 
tdm_report_timing(void)
{
  s1printf("silence_period: %u\n", (unsigned)silence_period); 
  s1printf("tx_window_width: %u\n", (unsigned)tx_window_width);
  s1printf("max_data_packet_length: %u\n", (unsigned)max_data_packet_length);
}


void sbus_show_channels(void)
{
  //Do a double read to flush 
  if (feature_sbus == SBUS_FUNCTION_TX)
  {
    Serial3.clear();
    while (!sbus_read()) ;
    s1printf("SBUS ch1:%u ch2:%u ch3:%u ch4:%u ch5:%u ch6:%u ch7:%u ch8:%u\n",sbus_channels[0],sbus_channels[1],sbus_channels[2],sbus_channels[3],sbus_channels[4],sbus_channels[5],sbus_channels[6],sbus_channels[7]);
  }

}
