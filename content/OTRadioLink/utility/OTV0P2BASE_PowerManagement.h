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
  Utilities to assist with minimal power usage on V0p2 boards,
  including interrupts and sleep.
  */

#ifndef OTV0P2BASE_POWERMANAGEMENT_H
#define OTV0P2BASE_POWERMANAGEMENT_H

#include <stdint.h>
#include <util/atomic.h>
#include <Arduino.h>

#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_FastDigitalIO.h"

namespace OTV0P2BASE
{

// TEMPLATED DEFINITIONS OF SPI power up/down.
//
// If SPI was disabled, power it up, enable it as master and with a sensible clock speed, etc, and return true.
// If already powered up then do nothing other than return false.
// If this returns true then a matching powerDownSPI() may be advisable.
template <uint8_t SPI_nSS>
bool t_powerUpSPIIfDisabled()
    {
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        if(!(PRR & _BV(PRSPI))) { return(false); }

        pinMode(SPI_nSS, OUTPUT); // Ensure that nSS is an output to avoid forcing SPI to slave mode by accident.
        fastDigitalWrite(SPI_nSS, HIGH); // Ensure that nSS is HIGH and thus any slave deselected when powering up SPI.

        PRR &= ~_BV(PRSPI); // Enable SPI power.

        // Configure raw SPI to match better how it was used in PICAXE V0.09 code.
        // CPOL = 0, CPHA = 0
        // Enable SPI, set master mode, set speed.
        const uint8_t ENABLE_MASTER = _BV(SPE) | _BV(MSTR);
#if F_CPU <= 2000000 // Needs minimum prescale (x2) with slow (<=2MHz) CPU clock.
        SPCR = ENABLE_MASTER; // 2x clock prescale for <=1MHz SPI clock from <=2MHz CPU clock (500kHz SPI @ 1MHz CPU).
        SPSR = _BV(SPI2X);
#elif F_CPU <= 8000000
        SPCR = ENABLE_MASTER; // 4x clock prescale for <=2MHz SPI clock from nominal <=8MHz CPU clock.
        SPSR = 0;
#else // Needs setting for fast (~16MHz) CPU clock.
        SPCR = _BV(SPR0) | ENABLE_MASTER; // 8x clock prescale for ~2MHz SPI clock from nominal ~16MHz CPU clock.
        SPSR = _BV(SPI2X);
#endif
        }
    return(true);
    }
//
// Power down SPI.
template <uint8_t SPI_nSS, uint8_t SPI_SCK, uint8_t SPI_MOSI, uint8_t SPI_MISO>
void t_powerDownSPI()
    {
    ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
        {
        SPCR &= ~_BV(SPE); // Disable SPI.
        PRR |= _BV(PRSPI); // Power down...

        pinMode(SPI_nSS, OUTPUT); // Ensure that nSS is an output to avoid forcing SPI to slave mode by accident.
        fastDigitalWrite(SPI_nSS, HIGH); // Ensure that nSS is HIGH and thus any slave deselected when powering up SPI.

        // Avoid pins from floating when SPI is disabled.
        // Try to preserve general I/O direction and restore previous output values for outputs.
        pinMode(SPI_SCK, OUTPUT);
        pinMode(SPI_MOSI, OUTPUT);
        pinMode(SPI_MISO, INPUT_PULLUP);

        // If sharing SPI SCK with LED indicator then return this pin to being an output (retaining previous value).
        //if(LED_HEATCALL == PIN_SPI_SCK) { pinMode(LED_HEATCALL, OUTPUT); }
        }
    }

// STANDARD UNTEMPLATED DEFINITIONS OF SPI power up/down if PIN_SPI_nSS is defined.
// If SPI was disabled, power it up, enable it as master and with a sensible clock speed, etc, and return true.
// If already powered up then do nothing other than return false.
// If this returns true then a matching powerDownSPI() may be advisable.
inline bool powerUpSPIIfDisabled() { return(t_powerUpSPIIfDisabled<V0p2_PIN_SPI_nSS>()); }
// Power down SPI.
inline void powerDownSPI() { t_powerDownSPI<V0p2_PIN_SPI_nSS, V0p2_PIN_SPI_SCK, V0p2_PIN_SPI_MOSI, V0p2_PIN_SPI_MISO>(); }

}
#endif
