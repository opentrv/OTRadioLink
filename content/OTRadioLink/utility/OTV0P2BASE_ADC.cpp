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
*/

/*
 ADC (Analogue-to-Digital Converter) support.

 V0p2/AVR only for now.
 */


#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#include <util/crc16.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_ADC.h"

#include <OTV0p2Base.h>

#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR

// Allow wake from (lower-power) sleep while ADC is running.
static volatile bool ADC_complete;
ISR(ADC_vect) { ADC_complete = true; }

// Nominally accumulate mainly the bottom bits from normal ADC conversions for entropy,
// especially from earlier unsettled conversions when taking multiple samples.
static volatile uint8_t _adcNoise;

// Read ADC/analogue input with reduced noise if possible, in range [0,1023].
//   * admux  is the value to set ADMUX to
//   * samples  maximum number of samples to take (if one, nap() before); strictly positive
// Sets sleep mode to SLEEP_MODE_ADC, and disables sleep on exit.
uint16_t _analogueNoiseReducedReadM(const uint8_t admux, int8_t samples)
  {
  const bool neededEnable = powerUpADCIfDisabled();
  ACSR |= _BV(ACD); // Disable the analogue comparator.
  ADMUX = admux;
  if(samples < 2) { ::OTV0P2BASE::nap(WDTO_15MS); } // Allow plenty of time for things to settle if not taking multiple samples.
  set_sleep_mode(SLEEP_MODE_ADC);
  ADCSRB = 0; // Enable free-running mode.
  bitWrite(ADCSRA, ADATE, (samples>1)); // Enable ADC auto-trigger iff wanting multiple samples.
  bitSet(ADCSRA, ADIE); // Turn on ADC interrupt.
  bitSet(ADCSRA, ADSC); // Start conversion(s).
  uint8_t oldADCL = 0xff;
  uint8_t oldADCH = 0xff; // Ensure that a second sample will get taken if multiple samples have been requested.
  // Usually take several readings to improve accuracy.  Discard all but the last...
  while(--samples >= 0)
      {
      ADC_complete = false;
      while(!ADC_complete) { sleep_mode(); }
      const uint8_t l = ADCL; // Capture the low byte and latch the high byte.
      const uint8_t h = ADCH; // Capture the high byte.
      if((h == oldADCH) && (l == oldADCL)) { break; } // Stop now if result seems to have settled.
      oldADCL = l;
      oldADCH = h;
      _adcNoise = (_adcNoise >> 1) + (l ^ h) + (__TIME__[7] & 0xf); // Capture a little entropy.
      }
  bitClear(ADCSRA, ADIE); // Turn off ADC interrupt.
  bitClear(ADCSRA, ADATE); // Turn off ADC auto-trigger.
  //sleep_disable();
  const uint8_t l = ADCL; // Capture the low byte and latch the high byte.
  const uint8_t h = ADCH; // Capture the high byte.
  if(neededEnable) { powerDownADC(); }
  return((h << 8) | l);
  }

// Read ADC/analogue input with reduced noise if possible, in range [0,1023].
//   * aiNumber is the analogue input number [0,7] for ATMega328P
//   * mode  is the analogue reference, eg DEFAULT (Vcc). TODO write up possible modes
// May set sleep mode to SLEEP_MODE_ADC, and disable sleep on exit.
// Nominally equivalent to analogReference(mode); return(analogRead(pinNumber));
uint16_t analogueNoiseReducedRead(const uint8_t aiNumber, const uint8_t mode)
  { return(_analogueNoiseReducedReadM((mode << 6) | (aiNumber & 7))); }

// Read from the specified analogue input vs the band-gap reference; true means AI > Vref.
// Uses the comparator.
//   * aiNumber is the analogue input number [0,7] for ATMega328P
//   * napToSettle  if true then take a minimal sleep/nap to allow voltage to settle
//       if input source relatively high impedance (>>10k)
// Assumes that the band-gap reference is already running,
// eg from being used for BOD; if not, it must be given time to start up.
// For input settle time explanation please see for example:
//   * http://electronics.stackexchange.com/questions/67171/input-impedance-of-arduino-uno-analog-pins
//
// FIXME: create power Up/Down routines for comparator that nest ADC power Up/Down request.
//
// For comparator use examples see:
//   * http://forum.arduino.cc/index.php?topic=165744.0
//   * http://forum.arduino.cc/index.php?topic=17450.0
//   * http://www.avr-tutorials.com/comparator/utilizing-avr-analog-comparator-aco
//   * http://winavr.scienceprog.com/avr-gcc-tutorial/avr-comparator-c-programming-example.html
bool analogueVsBandgapRead(const uint8_t aiNumber, const bool napToSettle)
  {
  // Set following registers (see ATMega spec p246 onwards):
  //   * PRADC (PRR) to 0 (to avoid power reduction of comparator circuitry)
  //   * ACME (ADCSRB) to 1 (Analog Comparator Multiplexer Enable)
  //   * ADEN (ADCSRA) to 0 (to avoid enabling the A to D which must be off)
  //   * MUX 2..0 to aiNumber (to choose correct input via mux)
  //   * ACD (ACSR) to 0 (don't disable comparator)
  //   * ACBG (ACSR) to 1 (select bandgap as +ve input)
  //   * ACIC (ACSR) to 0 (disable input capture)
  //   * ACI, ACIE (ACSR) to 1 and 0 (to disable comparator interrupts)
  PRR &= ~_BV(PRADC); // Enable ADC power.
  ADCSRB |= _BV(ACME); // Allow comparator to use the mux.
  ADCSRA &= ~_BV(ADEN); // Disable the ADC itself so that the comparator can use mux input.
  ACSR =
    (0<<ACD) |    // Analog Comparator: enabled
    (1<<ACBG) |   // Analog Comparator Bandgap Select: bandgap is applied to the positive input
    (0<<ACO) |    // Analog Comparator Output: don't care for write
    (1<<ACI) |    // Analog Comparator Interrupt Flag: clear pending
    (0<<ACIE) |   // Analog Comparator Interrupt: disabled
    (0<<ACIC) |   // Analog Comparator Input Capture: disabled
    (0<<ACIS1) | (0<ACIS0); // Analog Comparator Interrupt Mode: comparator interrupt on toggle
  ADMUX = aiNumber & 7;

  // Wait for voltage to stabilise.
  if(napToSettle) { ::OTV0P2BASE::nap(WDTO_15MS); }

  // Read comparator output from ACO (ACSR).
  bool result = (0 != (ACSR & _BV(ACO)));
#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("ACSR: ");
  V0P2BASE_DEBUG_SERIAL_PRINT(ACSR);
  V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif

  ACSR |= _BV(ACD); // Disable the analogue comparator.
  ACSR &= ~_BV(ACBG); // Disconnect the bandgap reference from the comparator (to allow it to power down).
  //ADCSRA &= ~_BV(ADEN); // Do before power_[adc|all]_disable() to avoid freezing the ADC in an active state!
  PRR |= _BV(PRADC); // Disable ADC power.
  return(result);
  }


//// Default low-battery threshold suitable for 2xAA NiMH, with AVR BOD at 1.8V.
//#define BATTERY_LOW_MV 2000
//
//// Using some sensors forces a higher voltage threshold for 'low battery'.
//#if defined(SENSOR_SHT21_ENABLE)
//#define SENSOR_SHT21_MINMV 2199 // Only specified down to 2.1V.
//#if BATTERY_LOW_MV < SENSOR_SHT21_MINMV
//#undef BATTERY_LOW_MV
//#define BATTERY_LOW_MV SENSOR_SHT21_MINMV
//#endif
//#endif




// Attempt to capture maybe one bit of noise/entropy with an ADC read, possibly more likely in the lsbits if at all.
// In this case take a raw reading of the bandgap vs Vcc,
// and then all 8 ADC inputs relative to Vcc,
// and avoid the normal precautions to reduce noise.
// Resample a few times to try to actually see a changed value,
// and combine with a decentish hash.
// If requested (and needed) powers up extra I/O during the reads.
//   powerUpIO if true then power up I/O (and power down after if so)
//
// If defined, update _adcNoise value to make noisyADCRead() output at least a poor PRNG if called in a loop, though might disguise underlying problems.
//#define CATCH_OTHER_NOISE_DURING_NAR // May hide underlying weakness if defined.
#define IGNORE_POWERUPIO // FIXME
uint8_t noisyADCRead(const bool /*powerUpIO*/)
  {
  const bool neededEnable = powerUpADCIfDisabled();
#ifndef IGNORE_POWERUPIO
  const bool poweredUpIO = powerUpIO;
  if(powerUpIO) { power_intermittent_peripherals_enable(false); }
#endif
  // Sample supply voltage.
  ADMUX = _BV(REFS0) | 14; // Bandgap vs Vcc.
  ADCSRB = 0; // Enable free-running mode.
  bitWrite(ADCSRA, ADATE, 0); // Multiple samples NOT required.
  ADC_complete = false;
  bitSet(ADCSRA, ADIE); // Turn on ADC interrupt.
  bitSet(ADCSRA, ADSC); // Start conversion.
  uint8_t count = 0;
  while(!ADC_complete) { ++count; } // Busy wait while 'timing' the ADC conversion.
  const uint8_t l1 = ADCL; // Capture the low byte and latch the high byte.
  const uint8_t h1 = ADCH; // Capture the high byte.
#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("NAR V: ");
  V0P2BASE_DEBUG_SERIAL_PRINTFMT(h1, HEX);
  V0P2BASE_DEBUG_SERIAL_PRINT(' ');
  V0P2BASE_DEBUG_SERIAL_PRINTFMT(l1, HEX);
  V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
  // Sample internal temperature.
  ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3); // Temp vs bandgap.
  ADC_complete = false;
  bitSet(ADCSRA, ADSC); // Start conversion.
  while(!ADC_complete) { ++count; } // Busy wait while 'timing' the ADC conversion.
  const uint8_t l2 = ADCL; // Capture the low byte and latch the high byte.
  const uint8_t h2 = ADCH; // Capture the high byte.
#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("NAR T: ");
  V0P2BASE_DEBUG_SERIAL_PRINTFMT(h2, HEX);
  V0P2BASE_DEBUG_SERIAL_PRINT(' ');
  V0P2BASE_DEBUG_SERIAL_PRINTFMT(l2, HEX);
  V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
  uint8_t result = (h1 << 5) ^ (l2) ^ (h2 << 3) ^ count;
#if defined(CATCH_OTHER_NOISE_DURING_NAR)
  result = _crc_ibutton_update(_adcNoise++, result);
#endif
  // Sample all possible ADC inputs relative to Vcc, whatever the inputs may be connected to.
  // Assumed never to do any harm, eg physical damage, nor to disturb I/O setup.
  for(uint8_t i = 0; i < 8; ++i)
    {
    ADMUX = (i & 7) | (DEFAULT << 6); // Switching MUX after sample has started may add further noise.
    ADC_complete = false;
    bitSet(ADCSRA, ADSC); // Start conversion.
    while(!ADC_complete) { ++count; }
    const uint8_t l = ADCL; // Capture the low byte and latch the high byte.
    const uint8_t h = ADCH; // Capture the high byte.
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("NAR M: ");
    V0P2BASE_DEBUG_SERIAL_PRINTFMT(h, HEX);
    V0P2BASE_DEBUG_SERIAL_PRINT(' ');
    V0P2BASE_DEBUG_SERIAL_PRINTFMT(l, HEX);
    V0P2BASE_DEBUG_SERIAL_PRINT(' ');
    V0P2BASE_DEBUG_SERIAL_PRINT(count);
    V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
    result = _crc_ibutton_update(result ^ h, l ^ count); // A thorough hash.
#if 0 && defined(V0P2BASE_DEBUG)
    V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("NAR R: ");
    V0P2BASE_DEBUG_SERIAL_PRINTFMT(result, HEX);
    V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
    }
  bitClear(ADCSRA, ADIE); // Turn off ADC interrupt.
  bitClear(ADCSRA, ADATE); // Turn off ADC auto-trigger.
#ifndef IGNORE_POWERUPIO
  if(poweredUpIO) { power_intermittent_peripherals_disable(); }
#endif
  if(neededEnable) { powerDownADC(); }
  result ^= l1; // Ensure that the Vcc raw lsbs get directly folded in to the final result.
#if 0 && defined(V0P2BASE_DEBUG)
  V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING("NAR: ");
  V0P2BASE_DEBUG_SERIAL_PRINTFMT(result, HEX);
  V0P2BASE_DEBUG_SERIAL_PRINTLN();
#endif
  return(result); // Use all the bits collected.
  }


// Get approximate internal temperature in nominal C/16.
// Only accurate to +/- 10C uncalibrated.
// May set sleep mode to SLEEP_MODE_ADC, and disables sleep on exit.
int readInternalTemperatureC16()
  {
  // Measure internal temperature sensor against internal voltage source.
  // Response is ~1mv/C with 0C at ~289mV according to the data sheet.
  const uint16_t raw = OTV0P2BASE::_analogueNoiseReducedReadM(_BV(REFS1) | _BV(REFS0) | _BV(MUX3), 1);
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("Int temp raw: ");
  DEBUG_SERIAL_PRINT(raw);
  DEBUG_SERIAL_PRINTLN_FLASHSTRING("");
#endif
  //const int degC = (raw - 328) ; // Crude fast adjustment for one sensor at ~20C (DHD20130429).
  const int degC = ((((int)raw) - 324) * 210) >> 4; // Slightly less crude adjustment, see http://playground.arduino.cc//Main/InternalTemperatureSensor
  return(degC);
  }

#endif // ARDUINO_ARCH_AVR


}
