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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
                           Deniz Erbilgin 2015
*/

/*
  GPIO underlying pin assignments and similar that do not depend on board revision.
  */

#ifndef OTV0P2BASE_BASICPINASSIGNMENTS_H
#define OTV0P2BASE_BASICPINASSIGNMENTS_H

#include <stdint.h>

namespace OTV0P2BASE
{


// DIGITAL I/O ASSIGNMENTS

// Serial: hardware UART.
static const uint8_t V0p2_PIN_SERIAL_RX = 0; // ATMega328P-PU PDIP pin 2, PD0.
static const uint8_t V0p2_PIN_SERIAL_TX = 1; // ATMega328P-PU PDIP pin 3, PD1.

// One-wire (eg DS18B20) DQ/data/pullup line.
static const uint8_t V0p2_PIN_OW_DQ_DATA = 2; // ATMega328P-PU PDIP pin 4, PD2.

// I2C/TWI: SDA (ain 4), SCL (ain 5), interrupt (dpin3).
static const uint8_t V0p2_PIN_SDA_AIN = 4; // ATMega328P-PU PDIP pin 27, PC4.
static const uint8_t V0p2_PIN_SCL_AIN = 5; // ATMega328P-PU PDIP pin 28, PC5.

// Default pin to power-up I/O low-power (sensors) only intermittently enabled, when high, digital out.
// Can be connected direct, or via 330R+ current limit and 100nF+ decoupling).
// NOTE: this is a feature of V0p2 boards rather inherent to the AVR/ATMega328P.
static const uint8_t V0p2_PIN_DEFAULT_IO_POWER_UP = 7; // ATMega328P-PU PDIP pin 13, PD7, no usable analogue input.

// SPI: SCK (dpin 13, also LED on Arduino UNO + REV1 boards that the bootloader may 'flash'), MISO (dpin 12), MOSI (dpin 11), nSS (dpin 10).
static const constexpr uint8_t V0p2_PIN_SPI_nSS = 10; // ATMega328P-PU PDIP pin 16, PB2.  Active low enable.
static const constexpr uint8_t V0p2_PIN_SPI_MOSI = 11; // ATMega328P-PU PDIP pin 17, PB3.
static const constexpr uint8_t V0p2_PIN_SPI_MISO = 12; // ATMega328P-PU PDIP pin 18, PB4.
static const constexpr uint8_t V0p2_PIN_SPI_SCK = 13; // ATMega328P-PU PDIP pin 19, PB5.


// ANALOGUE I/O ASSIGNMENTS

// Ambient light sensor (eg LDR) analogue input: higher voltage means more light.
static const uint8_t V0p2_PIN_LDR_SENSOR_AIN = 0; // ATMega328P-PU PDIP pin 23, PC0.

// Temperature potentiometer, present in REV 2/3/4/7.
static const uint8_t V0p2_PIN_TEMP_POT_AIN = 1; // ATMega328P-PU PDIP pin 24, PC1.


}

#endif
