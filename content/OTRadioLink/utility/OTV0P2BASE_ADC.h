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

#ifndef OTV0P2BASE_ADC_H
#define OTV0P2BASE_ADC_H

#include <stdint.h>


namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR

// Read ADC/analogue input with reduced noise if possible, in range [0,1023].
//   * admux  is the value to set ADMUX to
//   * samples  maximum number of samples to take (if one, nap() before); strictly positive
// Sets sleep mode to SLEEP_MODE_ADC, and disables sleep on exit.
uint16_t _analogueNoiseReducedReadM(const uint8_t admux, int8_t samples = 3);

// Read ADC/analogue input with reduced noise if possible, in range [0,1023].
//   * aiNumber is the analogue input number [0,7] for ATMega328P
//   * mode  is the analogue reference, eg DEFAULT (Vcc).
// May set sleep mode to SLEEP_MODE_ADC, and disable sleep on exit.
// Nominally equivalent to analogReference(mode); return(analogRead(pinNumber));
// DE201512: takes 500-600 microseconds to execute @ 1MHZ CPU (tested with an oscilloscope by strobing pin).
uint16_t analogueNoiseReducedRead(uint8_t aiNumber, uint8_t mode);

// Read from the specified analogue input vs the band-gap reference; true means AI > Vref.
//   * aiNumber is the analogue input number [0,7] for ATMega328P
//   * napToSettle  if true then take a minimal sleep/nap to allow voltage to settle
//       if input source relatively high impedance (>>10k)
// Assumes that the band-gap reference is already running,
// eg from being used for BOD; if not, it must be given time to start up.
// For input settle time explanation please see for example:
//   * http://electronics.stackexchange.com/questions/67171/input-impedance-of-arduino-uno-analog-pins
// DE201512: takes 50-60 microseconds to execute @ 1MHZ CPU when napToSettle is false (tested with an oscilloscope by strobing pin).
bool analogueVsBandgapRead(uint8_t aiNumber, bool napToSettle = false);

// Attempt to capture maybe one bit of noise/entropy with an ADC read, possibly more likely in the lsbits if at all.
// If requested (and needed) powers up extra I/O during the reads.
//   powerUpIO if true then power up I/O (and power down after if so)
// DE201512: takes ~2300 microseconds to execute @ 1MHZ CPU (tested with an oscilloscope by strobing pin).
uint8_t noisyADCRead(bool powerUpIO = true);


// Get approximate internal temperature in nominal C/16.
// Only accurate to +/- 10C uncalibrated.
// May set sleep mode to SLEEP_MODE_ADC, and disables sleep on exit.
int readInternalTemperatureC16();
// TODO: find a better location for this.

#endif // ARDUINO_ARCH_AVR


}
#endif
