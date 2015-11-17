/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
                           Deniz Erbilgin 2015
*/

/*
  Utilities to assist with minimal power usage on V0p2 boards,
  including interrupts and sleep.
  */

#include <util/atomic.h>
#include <Arduino.h>

#include "OTV0P2BASE_PowerManagement.h"

namespace OTV0P2BASE
{


// If ADC was disabled, power it up, do Serial.begin(), and return true.
// If already powered up then do nothing other than return false.
// This does not power up the analogue comparator; this needs to be manually enabled if required.
// If this returns true then a matching powerDownADC() may be advisable.
bool powerUpADCIfDisabled()
  {
  if(!(PRR & _BV(PRADC))) { return(false); }
  PRR &= ~_BV(PRADC); // Enable the ADC.
  ADCSRA |= _BV(ADEN);
  return(true);
  }

// Power ADC down.
void powerDownADC()
  {
  ADCSRA &= ~_BV(ADEN); // Do before power_[adc|all]_disable() to avoid freezing the ADC in an active state!
  PRR |= _BV(PRADC); // Disable the ADC.
  }


// Flush any pending UART TX bytes in the hardware if UART is enabled, eg useful after Serial.flush() and before sleep.
void flushSerialHW()
  {
  if(PRR & _BV(PRUSART0)) { return; } // UART not running, so nothing to do.

  // Snippet c/o http://www.gammon.com.au/forum/?id=11428
  // TODO: try to sleep in a low-ish power mode while waiting for data to clear.
  while(!(UCSR0A & _BV(UDRE0)))  // Wait for empty transmit buffer.
     { UCSR0A |= _BV(TXC0); }  // Mark transmission not complete.
  while(!(UCSR0A & _BV(TXC0))) { } // Wait for the transmission to complete.
  return;
  }

#ifndef flushSerialProductive
// Does a Serial.flush() attempting to do some useful work (eg I/O polling) while waiting for output to drain.
// Assumes hundreds of CPU cycles available for each character queued for TX.
// Does not change CPU clock speed or disable or mess with USART0, though may poll it.
void flushSerialProductive()
  {
#if 0 && defined(V0P2BASE_DEBUG)
  if(!_serialIsPoweredUp()) { panic(); } // Trying to operate serial without it powered up.
#endif
  // Can productively spin here churning PRNGs or the like before the flush(), checking for the UART TX buffer to empty...
  // An occasional premature exit to flush() due to Serial interrupt handler interaction is benign, and indeed more grist to the mill.
  while(serialTXInProgress()) { /*8burnHundredsOfCyclesProductivelyAndPoll();*/ }
  Serial.flush(); // Wait for all output to have been sent.
  // Could wait two character times at 10 bits per character based on BAUD.
  // Or mass with the UART...
  flushSerialHW();
  }
#endif

#ifndef flushSerialSCTSensitive
// Does a Serial.flush() idling for 15ms at a time while waiting for output to drain.
// Does not change CPU clock speed or disable or mess with USART0, though may poll it.
// Sleeps in IDLE mode for ~15ms at a time (backtopped by watchdog) waking on any interrupt
// so that the caller must be sure RX overrun (etc) will not be an issue.
// Switches to flushSerialProductive() behaviour
// if in danger of overrunning a minor cycle while idling.
void flushSerialSCTSensitive()
  {
#if 0 && defined(V0P2BASE_DEBUG)
  if(!_serialIsPoweredUp()) { panic(); } // Trying to operate serial without it powered up.
#endif
#ifdef ENABLE_USE_OF_AVR_IDLE_MODE
  while(serialTXInProgress() && (getSubCycleTime() < GSCT_MAX - 2 - (20/SUBCYCLE_TICK_MS_RD)))
    { idle15AndPoll(); } // Save much power by idling CPU, though everything else runs.
#endif
  flushSerialProductive();
  }
#endif


// Flush any pending serial output and power it down if up.
void powerDownSerial()
  {
  if(_serialIsPoweredUp())
    {
    // Flush serial output and shut down if apparently active.
    Serial.flush();
    //flushSerialHW();
    Serial.end();
    }
  pinMode(V0p2_PIN_SERIAL_RX, INPUT_PULLUP);
  pinMode(V0p2_PIN_SERIAL_TX, INPUT_PULLUP);
  PRR |= _BV(PRUSART0); // Disable the UART module.
  }


} // OTV0P2BASE





/*
 Power log.
 Basic CPU 1MHz (8MHz RC clock prescaled) + 32768Hz clock running timer 2 async.
 Current draw measured across 100R in Vcc supply on 200mV scale (0.1mV, ie ulp, = 1uA).
 Initially using a 1Hz wake-up from timer 2; later at 0.5Hz.
 USB disconnected for all power measurements unless otherwise stated.
 2013/04/21 11:50 ~5uA@5V in 'frost' mode (no LED flash). USB disconnected (else ~55uA). Using sleepLowPowerLoopsMinCPUSpeed(), ie min CPU speed in wait.
 2013/04/21 15:37 ~4uA@5V,1uA@2.8V in 'frost' mode (no LED flash) using WDT xxxPause(). USB disconnected (else ~55uA).  Possibly less distinct flash lengths.
 2013/04/21 15:37 ~1.5uA@2.6V with readAmbientLight() being called once per second.
 2013/04/25 09:44 Takes ~24--36ms leaving loop() and re-entering after roll to new minor cycle from timer 2 interrupt including loop()-exit Arduino background activity.
 2013/04/25 10:49 ~1uA@2.6V (no readAmbientLight(), no LED flash) with timer 2 wakeup reduced to 0.5Hz.
 2013/04/25 12:48 ~4uA@2.6V with minimal serial status report every 2 seconds (and USB disconnected).
 2013/04/25 14:10 ~1uA@2.6V with minimal serial status report every 60 seconds or on significant change (and USB disconnected).
 2013/04/25 15:24 ~1uA@2.6V having removed #define DONT_USE_TIMER0 so may be benign to leave available for Arduino uses.
 2013/04/25 17:00 ~6.5uA@2.6V adding TMP102 sensor (on SparkFun breakout board) with only Vcc/Gnd connected (default 4Hz continuous conversion).
 2013/04/25 18:18 ~7uA@2.6V with TMP102 SCL/SDA also wired and reading pulled once per 60s (default 4Hz continuous conversion).
 2013/04/25 21:03 ~3uA@2.6V with TMP102 in one-shot mode: TMP102 draws ~2x the current that the ATmega328P does!
 2013/04/26 20:29 ~2.7uA@2.6V 1k resistor in supply line suggests that idle current is 2.7uA; ~1.3uA with TMP102 removed.
 2013/04/27 19:38 ~2.7uA@2.6V still, after all EEPROM / RTC persistence work; surges to very roughly 60uA, once per minute.
 2013/04/30 12:25 ~2.6uA@2.6V multiple small efficiency tweaks and spread out per-minute processing and do less of it in frost mode.
 2013/05/04 17:08 ~1.4mA@2.5V (>1milliAmp!) with RFM22 connected and idle; back to 100R in supply line else won't start up with RFM22 connected.
 2013/05/04 18:47 ~16uA@2.6V with RFM22 powered down with RFM22ModeStandbyAndClearState() including clearing interrupts.
 2013/05/05 10:47 ~3uA@2.6V with all SPI bus pins prevented from floating when idle.  (Measured <3.3uA idle with 1k supply resistor.)
 2013/05/05 12:47 ~3.2uA@2.6V (1k supply resistor) with TWI clock speed pushed up to 62.5kHz, so less time with CPU running.
 2013/05/16 13:53 ~180uA@2.6V (1k supply resistor) with CLI waiting for input ~900ms every 2s (3.3uA when not, and USB disconnected).
 2013/05/21 11:53 ~6.4uA@2.6V (1k supply resistor) with main loop doing nothing but sleepUntilSubCycleTime() for 50% of the minor cycle.
 2013/05/22 12:51 ~1mA@2.6V (100R supply resistor) with IGNORE_FHT_SYNC and in frost mode, ie one FHT8V TX via RFM22 per second.
 2013/05/22 19:16 ~200uA@2.6V (100R supply resistor) in BOOST controlling FHT8V, post sync (& double TXes), LED flashing, USB connected.
 2013/05/22 19:17 ~3uA@2.6V min calculated ~23uA mean in FROST w/ FHT8V, post sync, single TXes, LED off, USB disconn, calced ~50uA mean in WARM mode w/ valve open.
 2013/06/09 16:54 ~40uA@2.6V (100R supply resistor) polling for UART input (CLI active), FHT8V not transmitting.
 2013/06/09 18:21 ~35uA@2.6V (100R supply resistor) polling for UART input (CLI active), FHT8V not transmitting, spending more time in IDLE.
 2014/12/10 18:01 ~4uA@2.5V (100R supply resistor) running current OpenTRV main loop; rises to ~150uA flashing LED in 'FROST' display.
 */
