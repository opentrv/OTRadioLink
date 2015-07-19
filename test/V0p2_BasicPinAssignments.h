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
  GPIO underlying pin assignments that do not depend on board revision.
  */

#ifndef V0P2_BASICPINASSIGNMENTS_H
#define V0P2_BASICPINASSIGNMENTS_H

// SPI: SCK (dpin 13, also LED on Arduino boards that the bootloader may 'flash'), MISO (dpin 12), MOSI (dpin 11), nSS (dpin 10).
#define V0p2_PIN_SPI_SCK 13 // ATMega328P-PU PDIP pin 19, PB5.
#define V0p2_PIN_SPI_MISO 12 // ATMega328P-PU PDIP pin 18, PB4.
#define V0p2_PIN_SPI_MOSI 11 // ATMega328P-PU PDIP pin 17, PB3.
#define V0p2_PIN_SPI_nSS 10 // ATMega328P-PU PDIP pin 16, PB2.  Active low enable.

#endif
