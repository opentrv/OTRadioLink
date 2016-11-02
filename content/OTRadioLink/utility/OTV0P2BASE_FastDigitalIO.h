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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

/*
  Fast GPIO with minimal run-time look-up for V0p2 boards.
  Also accurate micro delays for bit-banging time-sensitive protocols.
  */

#ifndef OTV0P2BASE_FASTDIGITALIO_H
#define OTV0P2BASE_FASTDIGITALIO_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace OTV0P2BASE
{

// Fast read of digital pins where pin number is constant.
// Avoids lots of logic (many 10s of CPU cycles) in normal digitalRead()/digitalWrite() calls,
// and this saves time and energy on (critical) paths polling I/O.
// Does not do any error checking: beware.
// Only really intended for ATmega328P.
#ifdef ARDUINO_ARCH_AVR
#ifdef __AVR_ATmega328P__
/* Register: PIND for 0--7, PINB for 8--13, 14--19 PINC (ADC/AI). */
/* Bit: 0--7 as-is, 8--13 subtract 8, else subtract 14. */
// Compute the bit mask for the port pin.
#define _fastDigitalMask(pin) \
    ((__builtin_constant_p((pin)) && ((pin) >= 0) && ((pin) < 8)) ? (1 << (pin)) : \
    ((__builtin_constant_p((pin)) && ((pin) >= 8) && ((pin) < 14)) ? (1 << ((pin)-8)) : \
    ((__builtin_constant_p((pin)) && ((pin) >= 14) && ((pin) < 20)) ? (1 << ((pin)-14)) : \
    0))) // Give up if not valid pin number and a constant.
#define fastDigitalMask(pin) \
    ((__builtin_constant_p((pin)) && ((pin) >= 0) && ((pin) < 8)) ? (1 << (pin)) : \
    ((__builtin_constant_p((pin)) && ((pin) >= 8) && ((pin) < 14)) ? (1 << ((pin)-8)) : \
    ((__builtin_constant_p((pin)) && ((pin) >= 14) && ((pin) < 20)) ? (1 << ((pin)-14)) : \
    digitalPinToBitMask((pin))))) // Fall back to generic routine.
// Compute the base (PINx, input) register (from PINx, DDRx, PORTx) for the port pin.
#define _fastDigitalInputRegister(pin) \
    ((__builtin_constant_p((pin)) && ((pin) >= 0) && ((pin) < 8)) ? &PIND : \
    ((__builtin_constant_p((pin)) && ((pin) >= 8) && ((pin) < 14)) ? &PINB : \
    ((__builtin_constant_p((pin)) && ((pin) >= 14) && ((pin) < 20)) ? &PINC : \
    &PIND))) // Give up if not valid pin number and a constant.
#define fastDigitalInputRegister(pin) \
    ((__builtin_constant_p((pin)) && ((pin) >= 0) && ((pin) < 8)) ? &PIND : \
    ((__builtin_constant_p((pin)) && ((pin) >= 8) && ((pin) < 14)) ? &PINB : \
    ((__builtin_constant_p((pin)) && ((pin) >= 14) && ((pin) < 20)) ? &PINC : \
    portInputRegister(digitalPinToPort((pin)))))) // Fall back to generic routine.

// READ GENERIC
// Handle quickly constant-value pins that we know about; fall back to generic run-time routine for rest.
#define fastDigitalRead(pin) \
    ((__builtin_constant_p((pin)) && ((pin) >= 0) && ((pin) < 8)) ? ((int) ((PIND >> (pin)) & 1)) : \
    ((__builtin_constant_p((pin)) && ((pin) >= 8) && ((pin) < 14)) ? ((int) ((PINB >> max(((pin)-8),0)) & 1)) : \
    ((__builtin_constant_p((pin)) && ((pin) >= 14) && ((pin) < 20)) ? ((int) ((PINC >> max(((pin)-14),0)) & 1)) : \
    digitalRead((pin))))) // Fall back to generic routine.    
// WRITE GENERIC
/* Register: PORTD for 0--7, PORTB for 8--13, (eg 13 is PORTB), 14--19 PINC (ADC/AI). */
/* Bit: 0--7 as-is, 8--13 subtract 8, else subtract 14. */
// Handle quickly constant-value pins that we know about; fall back to generic run-time routine for rest.
// NOTE: Falls back to digitalWrite if 'value' is not a compile time constant.
#define fastDigitalWrite(pin, value) do { \
    if(__builtin_constant_p((pin)) && (__builtin_constant_p(value) && ((pin) >= 0) && ((pin) < 8))) { bitWrite(PORTD, (pin), (value)); } \
    else if(__builtin_constant_p((pin)) && (__builtin_constant_p((value)) && ((pin) >= 8) && ((pin) < 14))) { bitWrite(PORTB, max((pin)-8,0), (value)); } \
    else if(__builtin_constant_p((pin)) && (__builtin_constant_p((value)) && ((pin) >= 14) && ((pin) < 20))) { bitWrite(PORTC, max((pin)-14,0), (value)); } \
    else { digitalWrite((pin), (value)); } } while(false) // Fall back to generic routine.    
#else
#define fastDigitalRead(pin) digitalRead((pin)) // Don't know about other AVRs.
#define fastDigitalWrite(pin, value) digitalWrite((pin), (value)) // Don't know about other AVRs.
#endif // __AVR_ATmega328P__
#endif // ARDUINO_ARCH_AVR

}
#endif
