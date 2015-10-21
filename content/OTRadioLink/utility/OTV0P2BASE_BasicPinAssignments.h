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
  GPIO underlying pin assignments and similar that do not depend on board revision.
  */

#ifndef OTV0P2BASE_BASICPINASSIGNMENTS_H
#define OTV0P2BASE_BASICPINASSIGNMENTS_H

#include <stdint.h>

namespace OTV0P2BASE
{
// Serial List
static const uint8_t V0p2_PIN_SERIAL_RX = 0; // ATMega328P-PU PDIP pin 2, PD0.
static const uint8_t V0p2_PIN_SERIAL_TX = 1; // ATMega328P-PU PDIP pin 3, PD1.

// SPI: SCK (dpin 13, also LED on Arduino boards that the bootloader may 'flash'), MISO (dpin 12), MOSI (dpin 11), nSS (dpin 10).
static const uint8_t V0p2_PIN_SPI_SCK = 13; // ATMega328P-PU PDIP pin 19, PB5.
static const uint8_t V0p2_PIN_SPI_MISO = 12; // ATMega328P-PU PDIP pin 18, PB4.
static const uint8_t V0p2_PIN_SPI_MOSI = 11; // ATMega328P-PU PDIP pin 17, PB3.
static const uint8_t V0p2_PIN_SPI_nSS = 10; // ATMega328P-PU PDIP pin 16, PB2.  Active low enable.

}

#endif
