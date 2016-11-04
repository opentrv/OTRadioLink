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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
                           Deniz Erbilgin 2015
*/

/*
  Utilities to assist with minimal power usage,
  including interrupts and sleep.

  Mainly V0p2/AVR specific for now.
  */

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#include <avr/wdt.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#endif

#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_Sleep.h"

namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR
    // If ADC was disabled, power it up and return true.
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

    //// Power ADC down.
    //void powerDownADC()
    //  {
    //  ADCSRA &= ~_BV(ADEN); // Do before power_[adc|all]_disable() to avoid freezing the ADC in an active state!
    //  PRR |= _BV(PRADC); // Disable the ADC.
    //  }
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
// Flush any pending UART TX bytes in the hardware if UART is enabled, eg useful after Serial.flush() and before sleep.
static void flushSerialHW()
  {
  if(PRR & _BV(PRUSART0)) { return; } // UART not running, so nothing to do.

  // Snippet c/o http://www.gammon.com.au/forum/?id=11428
  // TODO: try to sleep in a low-ish power mode while waiting for data to clear.
  while(!(UCSR0A & _BV(UDRE0)))  // Wait for empty transmit buffer.
     { UCSR0A |= _BV(TXC0); }  // Mark transmission not complete.
  while(!(UCSR0A & _BV(TXC0))) { } // Wait for the transmission to complete.
  return;
  }
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
//#ifndef flushSerialProductive
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
  while(serialTXInProgress()) { captureEntropy1(); }
  Serial.flush(); // Wait for all output to have been sent.
  // Could wait two character times at 10 bits per character based on BAUD.
  // Or mess with the UART...
  flushSerialHW();
  }
//#endif
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
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
//#ifdef ENABLE_USE_OF_AVR_IDLE_MODE
//  while(serialTXInProgress() && (getSubCycleTime() < GSCT_MAX - 2 - (20/SUBCYCLE_TICK_MS_RD)))
//    { idle15AndPoll(); } // Save much power by idling CPU, though everything else runs.
//#endif
  flushSerialProductive();
  }
#endif
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
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
#endif // ARDUINO_ARCH_AVR


#ifdef ARDUINO_ARCH_AVR
    // If TWI (I2C) was disabled, power it up, do Wire.begin(), and return true.
    // If already powered up then do nothing other than return false.
    // If this returns true then a matching powerDownRWI() may be advisable.
    bool powerUpTWIIfDisabled()
      {
      if(!(PRR & _BV(PRTWI))) { return(false); }

      PRR &= ~_BV(PRTWI); // Enable TWI power.
      TWCR |= _BV(TWEN); // Enable TWI.
      Wire.begin(); // Set it going.
      // TODO: reset TWBR and prescaler for our low CPU frequency     (TWBR = ((F_CPU / TWI_FREQ) - 16) / 2 gives -3!)
    #if F_CPU <= 1000000
      TWBR = 0; // Implies SCL freq of F_CPU / (16 + 2 * TBWR * PRESC) = 62.5kHz @ F_CPU==1MHz and PRESC==1 (from Wire/TWI code).
    #endif
      return(true);
      }

    // Power down TWI (I2C).
    void powerDownTWI()
      {
      TWCR &= ~_BV(TWEN); // Disable TWI.
      PRR |= _BV(PRTWI); // Disable TWI power.

      // De-activate internal pullups for TWI especially if powering down all TWI devices.
      //digitalWrite(SDA, 0);
      //digitalWrite(SCL, 0);

      // Convert to hi-Z inputs.
      //pinMode(SDA, INPUT);
      //pinMode(SCL, INPUT);
      }
#endif // ARDUINO_ARCH_AVR


// Enable power to intermittent peripherals.
//   * waitUntilStable  wait long enough (and maybe test) for I/O power to become stable.
// Waiting for stable may only be necessary for those items hung from IO_POWER cap;
// items powered direct from IO_POWER_UP may need no such wait.
//
// Switches the digital line to high then output (to avoid ever *discharging* the output cap).
// Note that with 100nF cap, and 330R (or lower) resistor from the output pin,
// then 1ms delay should be plenty for the voltage on the cap to settle.
void power_intermittent_peripherals_enable(bool waitUntilStable)
  {
#ifdef ARDUINO_ARCH_AVR
  // V0p2/AVR implementation.
  fastDigitalWrite(V0p2_PIN_DEFAULT_IO_POWER_UP, HIGH);
  pinMode(V0p2_PIN_DEFAULT_IO_POWER_UP, OUTPUT);
  // If requested, wait long enough that I/O peripheral power should be stable.
  // Wait in a relatively low-power way...
  if(waitUntilStable) { sleepLowPowerMs(1); }
#endif // ARDUINO_ARCH_AVR
  }

// Disable/remove power to intermittent peripherals.
// Switches the digital line to input with no pull-up (ie high-Z).
// There should be some sort of load to stop this floating.
void power_intermittent_peripherals_disable()
  {
#ifdef ARDUINO_ARCH_AVR
  // V0p2/AVR implementation.
  pinMode(V0p2_PIN_DEFAULT_IO_POWER_UP, INPUT);
#endif // ARDUINO_ARCH_AVR
  }


#ifdef ARDUINO_ARCH_AVR

// DHD20161104: observed battery stats from a DORM1/TRV1 (5s) continually resetting with presumed low battery:
//    http://www.earth.org.uk/img/20161104-16WWSensorPower.png
//
//2016-11-04T07:20:50Z 240
//2016-11-04T07:31:55Z 228
//2016-11-04T07:32:18Z 240
//2016-11-04T07:32:39Z 228
//2016-11-04T07:33:01Z 240
//2016-11-04T07:37:07Z 240
//2016-11-04T07:47:11Z 242
//2016-11-04T07:48:08Z 230
//2016-11-04T07:48:53Z 230
//2016-11-04T07:52:53Z 240
//2016-11-04T08:02:58Z 228
//2016-11-04T08:03:28Z 240
//2016-11-04T08:03:44Z 230
//2016-11-04T08:04:54Z 240
//2016-11-04T08:08:48Z 242
//2016-11-04T08:19:51Z 228
//2016-11-04T08:25:19Z 242
//2016-11-04T08:32:21Z 228
//2016-11-04T08:32:52Z 240
//2016-11-04T08:33:07Z 232
//...
//2016-11-04T13:44:43Z 242
//2016-11-04T13:51:38Z 226
//2016-11-04T13:52:23Z 226
//2016-11-04T13:53:01Z 240
//2016-11-04T13:56:59Z 240
//2016-11-04T14:08:05Z 226
//2016-11-04T14:08:50Z 226
//2016-11-04T14:12:44Z 240
//2016-11-04T14:23:43Z 226
//2016-11-04T14:24:19Z 238
//2016-11-04T14:24:27Z 226
//2016-11-04T14:25:12Z 237
//2016-11-04T14:29:01Z 240
//2016-11-04T14:37:12Z 240
//2016-11-04T14:40:06Z 226
//2016-11-04T14:40:38Z 238
//2016-11-04T14:40:51Z 226
//2016-11-04T14:43:45Z 240
//2016-11-04T14:54:44Z 226
//2016-11-04T14:55:28Z 226
//
// Note typical 'good' values 2.5V--2.6V.

// Default V0p2 very low-battery threshold suitable for 2xAA NiMH, with AVR BOD at 1.8V.
// Set to be high enough for common sensors such as SHT21, ie >= 2.1V.
static const uint16_t BATTERY_VERY_LOW_cV = 210;

// Default V0p2 low-battery threshold suitable for 2xAA NiMH, with AVR BOD at 1.8V.
// Set to be high enough for safe motor operation without brownouts, etc.
static const uint16_t BATTERY_LOW_cV = 245;

// Force a read/poll of the supply voltage and return the value sensed.
// Expensive/slow.
// NOT thread-safe nor usable within ISRs (Interrupt Service Routines).
uint16_t SupplyVoltageCentiVolts::read()
  {
  // Measure internal bandgap (1.1V nominal, 1.0--1.2V) as fraction of Vcc [0,1023].
  const uint16_t raw = OTV0P2BASE::_analogueNoiseReducedReadM(_BV(REFS0) | 14);
  // If Vcc was 1.1V then raw ADC would be 1023, so (1023<<6)/raw = 1<<6, target output 110.
  // If Vcc was 2.2V then raw ADC would be 511, so (1023<<6)/raw = 2<<6, target output 220.
  // (Raw ADC output of 0, which would cause a divide-by-zero, is effectively impossible.)
//  const uint16_t result = ((1023U<<6) / raw) * (1100U>>6); // For mV, without overflow.
  const uint16_t result = (((1023U<<6) / raw) * 55U) >> 5; // For cV, without overflow.
  rawInv = raw;
  value = result;
  isVeryLow = (result <= BATTERY_VERY_LOW_cV);
  isLow = isVeryLow || (result <= BATTERY_LOW_cV);
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("Battery cV: ");
  DEBUG_SERIAL_PRINT(result);
  DEBUG_SERIAL_PRINT_FLASHSTRING(" raw ");
  DEBUG_SERIAL_PRINT(raw);
  if(isLow) { DEBUG_SERIAL_PRINT_FLASHSTRING(" LOW"); }
  DEBUG_SERIAL_PRINTLN();
#endif
  return(result);
  }
#endif // ARDUINO_ARCH_AVR


// Selectively turn off all modules that need not run continuously on V0p2 board
// so as to minimise power without (ie over and above) explicitly entering a sleep mode.
// Suitable for start-up and for belt-and-braces use before main sleep on each cycle,
// to ensure that nothing power-hungry is accidentally left on.
// Any module that may need to run all the time should not be turned off here.
// May be called from panic(), so do not be too clever.
// Does NOT attempt to power down the radio, eg in case that needs to be left in RX mode.
// Does NOT attempt to power down the hardware serial/UART.
void minimisePowerWithoutSleep()
  {
#ifdef ARDUINO_ARCH_AVR
  // V0p2/AVR implementation.
  // Disable the watchdog timer.
  wdt_disable();
#endif

  // Ensure that external peripherals are powered down.
  OTV0P2BASE::power_intermittent_peripherals_disable();

#ifdef ARDUINO_ARCH_AVR
  // V0p2/AVR implementation.
  // Turn off analogue stuff that eats power.
  ADCSRA = 0; // Do before power_[adc|all]_disable() to avoid freezing the ADC in an active state!
  ACSR = (1<<ACD); // Disable the analog comparator.
  DIDR0 = 0x3F; // Disable digital input buffers on all ADC0-ADC5 pins.
    // More subtle approach possible...
    //  // Turn off the digital input buffers on analogue inputs in use as such
    //  // so as to reduce power consumption with mid-supply input voltages.
    //  DIDR0 = 0
    //#if defined(TEMP_POT_AIN)
    //    | (1 << TEMP_POT_AIN)
    //#endif
    //#if defined(LDR_SENSOR_AIN)
    //    | (1 << LDR_SENSOR_AIN)
    //#endif
    //    ;
  DIDR1 = (1<<AIN1D)|(1<<AIN0D); // Disable digital input buffer on AIN1/0.
  power_adc_disable();

  // Ensure that SPI is powered down.
  OTV0P2BASE::powerDownSPI();

  // Ensure that TWI is powered down.
  OTV0P2BASE::powerDownTWI();

  // TIMERS
  // See: http://letsmakerobots.com/node/28278
  //   * For Arduino timer0 is used for the timer functions such as delay(), millis() and micros().
  //   * Servo Library uses timer1 (on UNO).
  //   * tone() function uses at least timer2.
  // Note that timer 0 in normal use sometimes seems to eat a lot of power.

//#if defined(DONT_USE_TIMER0)
//  power_timer0_disable();
//#endif

  power_timer1_disable();

//#ifndef WAKEUP_32768HZ_XTAL
//  power_timer2_disable();
//#endif
#endif // ARDUINO_ARCH_AVR
  }

#ifdef ARDUINO_ARCH_AVR
//#ifdef WAKEUP_32768HZ_XTAL
static void timer2XtalIntSetup()
 {
  // Set up TIMER2 to wake CPU out of sleep regularly using external 32768Hz crystal.
  // See http://www.atmel.com/Images/doc2505.pdf
  TCCR2A = 0x00;

//#if defined(HALF_SECOND_RTC_SUPPORT)
//  TCCR2B = (1<<CS22); // Set CLK/64 for overflow interrupt every 0.5s.
//#elif defined(V0P2BASE_TWO_S_TICK_RTC_SUPPORT)
  TCCR2B = (1<<CS22)|(1<<CS21); // Set CLK/128 for overflow interrupt every 2s.
//#else
//  TCCR2B = (1<<CS22)|(1<<CS20); // Set CLK/128 for overflow interrupt every 1s.
//#endif
  //TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20); // Set CLK/1024 for overflow interrupt every 8s (slowest possible).

  ASSR = (1<<AS2); // Enable asynchronous operation.
  TIMSK2 = (1<<TOIE2); // Enable the timer 2 interrupt.
  }
//#endif

// Call from setup() for V0p2 board to turn off unused modules, set up timers and interrupts, etc.
// I/O pin setting is not done here.
void powerSetup()
  {
//#ifdef DEBUG
//  assert(OTV0P2BASE::DEFAULT_CPU_PRESCALE == clock_prescale_get()); // Verify that CPU prescaling is as expected.
//#endif

  // Do normal gentle switch off, including analogue module/control in correct order.
  minimisePowerWithoutSleep();

  // Brutally force off all modules, then re-enable explicitly below any still needed.
  power_all_disable();

//#if !defined(DONT_USE_TIMER0)
  power_timer0_enable(); // Turning timer 0 off messes up some standard Arduino support such as delay() and millis().
//#endif

//#if defined(WAKEUP_32768HZ_XTAL)
  power_timer2_enable();
  timer2XtalIntSetup();
//#endif
  }
#endif // ARDUINO_ARCH_AVR







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
