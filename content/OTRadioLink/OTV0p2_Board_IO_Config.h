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
                           Deniz Erbilgin 2017
*/

/*
 * Selects/defines default I/O pins and other standard hardware config for 'standard' V0.2 build.
 *
 * May in some cases be adjusted by #includes/#defines ahead of this one,
 * so should in particular be #included after any ENABLE_...s and V0p2_REV definition.
 *
 * NOT to be included by ANY library routines,
 * though may be included by other application CONFIG heqders.
 */


#ifndef OTV0P2_BOARD_IO_CONFIG_H
#define OTV0P2_BOARD_IO_CONFIG_H


#ifdef ARDUINO

#include <OTV0p2Base.h> // Underlying hardware support definitions.



namespace OTV0P2BASE
{


#ifndef V0P2_UART_BAUD
#define V0P2_UART_BAUD 4800
#endif // V0P2_UART_BAUD

#if !defined(V0p2_REV)
#error Board revision not defined.
#endif
#if (V0p2_REV < 0) || (V0p2_REV > 20)
#error Board revision not defined correctly (out of range).
#endif


//-----------------------------------------
// Force definitions for peripherals that should be present on every V0.2 board
// (though may be ignored or not added to the board)
// to enable safe I/O setup and (eg) avoid bus conflicts.

// Note 'standard' allocations of (ATmega328P-PU) pins, to be nominally Arduino compatible,
// eg see here: http://www.practicalmaker.com/blog/arduino-shield-design-standards
//
// 32768Hz xtal between pins 9 and 10, async timer 2, for accurate timekeeping and low-power sleep.
//
// Serial (bootloader/general): RX (dpin 0), TX (dpin 1)
#define PIN_SERIAL_RX (::OTV0P2BASE::V0p2_PIN_SERIAL_RX) // 0: ATMega328P-PU PDIP pin 2, PD0.
#define PIN_SERIAL_TX (::OTV0P2BASE::V0p2_PIN_SERIAL_TX) // 1: ATMega328P-PU PDIP pin 2, PD1.
// SPI: SCK (dpin 13, also LED on Arduino boards that the bootloader may 'flash'), MISO (dpin 12), MOSI (dpin 11), nSS (dpin 10).
/// @note   These #defines are now used by the arduino IDE and cause warnings.
//#define PIN_SPI_SCK (::OTV0P2BASE::V0p2_PIN_SPI_SCK) // 13: ATMega328P-PU PDIP pin 19, PB5.
//#define PIN_SPI_MISO (::OTV0P2BASE::V0p2_PIN_SPI_MISO) // 12: ATMega328P-PU PDIP pin 18, PB4.
//#define PIN_SPI_MOSI (::OTV0P2BASE::V0p2_PIN_SPI_MOSI) // 11: ATMega328P-PU PDIP pin 17, PB3.
//#define PIN_SPI_nSS (::OTV0P2BASE::V0p2_PIN_SPI_nSS) // 10: ATMega328P-PU PDIP pin 16, PB2.  Active low enable.
#define V0P2_ENABLE_SPI  // temporary hack around above problem.
// I2C/TWI: SDA (ain 4), SCL (ain 5), interrupt (dpin3)
#define PIN_SDA_AIN (::OTV0P2BASE::V0p2_PIN_SDA_AIN) // 4: ATMega328P-PU PDIP pin 27, PC4.
#define PIN_SCL_AIN (::OTV0P2BASE::V0p2_PIN_SCL_AIN) // 5: ATMega328P-PU PDIP pin 28, PC5.
// One-wire (eg DS18B20) DQ/data/pullup line; REV1+.
#define PIN_OW_DQ_DATA (::OTV0P2BASE::V0p2_PIN_OW_DQ_DATA) // 2: ATMega328P-PU PDIP pin 4, PD2.

// OneWire: DQ (dpin2)
// PWM / general digital I/O: dpin 5, 6, 9, 10
// Interrupts: INT0 (dpin2, PD2, also OneWire), INT1 (dpin3, PD3, PCINT19)
// Analogue inputs (may need digital input buffers disabled to minimise power, so use as outputs): dpin 6, 7


// Primary UI LED for 'heat call' in OpenTRV controller units, digital out.
#if V0p2_REV == 1 // REV1 only
static const uint8_t LED_HEATCALL = 13; // ATMega328P-PU PDIP pin 19, PB5. SHARED WITH SPI DUTIES as per Arduino UNO...
inline void LED_HEATCALL_ON() { fastDigitalWrite(LED_HEATCALL, HIGH); }
inline void LED_HEATCALL_OFF() { fastDigitalWrite(LED_HEATCALL, LOW); }
// ISR-safe UI LED ON; does nothing if no ISR-safe version.
inline void LED_HEATCALL_ON_ISR_SAFE() { }
#else // REV0, REV2 dedicated output pin.
#define V0P2BASE_LED_HEATCALL_IS_L
static const uint8_t LED_HEATCALL_L = 4; // ATMega328P-PU PDIP pin 6, PD4.  PULL LOW TO ACTIVATE.  Not shared with SPI.
inline void LED_HEATCALL_ON() { fastDigitalWrite(LED_HEATCALL_L, LOW); }
inline void LED_HEATCALL_OFF() { fastDigitalWrite(LED_HEATCALL_L, HIGH); }
#if 0  // Alternate definition to disable heatcall led. For debugging ISRs.
inline void LED_HEATCALL_ON() { }
inline void LED_HEATCALL_OFF() { }
#endif // 0
// ISR-safe UI LED ON.
inline void LED_HEATCALL_ON_ISR_SAFE() { LED_HEATCALL_ON(); }

// Secondary UI LED available on some boards.
#if (V0p2_REV >= 7) && (V0p2_REV <= 9)
#define LED_UI2_EXISTS
#if (V0p2_REV == 7) || (V0p2_REV == 8)
static const uint8_t LED_UI2_L = 13; // ATMega328P-PU PDIP pin 19, PB5. SHARED WITH SPI DUTIES as per Arduino UNO.
#elif (V0p2_REV == 9)
static const uint8_t LED_UI2_L = 6; // ATMega328P-PU PDIP pin 6, PD4.  PULL LOW TO ACTIVATE.  Not shared with SPI.
#endif
inline void LED_UI2_ON() { fastDigitalWrite(LED_UI2_L, LOW); }
inline void LED_UI2_OFF() { fastDigitalWrite(LED_UI2_L, HIGH); }
#endif
#endif

// Digital output for radiator node to call for heat by wire and/or for boiler node to activate boiler.
// NOT AVAILABLE FOR REV9 (used to drive secondary/green LED).
#if (V0p2_REV != 9) || (V0p2_REV != 14)
#define OUT_HEATCALL 6  // ATMega328P-PU PDIP pin 12, PD6, no usable analogue input.
#define OUT_GPIO_1 OUT_HEATCALL // Alias for GPIO pin.
#endif

// UI main 'mode' button (active/pulled low by button, pref using weak internal pull-up), digital in.
// Should always be available where a local TRV is being controlled.
// NOT AVAILABLE FOR REV10 (used for GSM module TX pin).
#if (V0p2_REV == 10) || (V0p2_REV == 14)	// FIXME might be better to define pins by peripheral
#define SOFTSERIAL_RX_PIN 8
#define SOFTSERIAL_TX_PIN 5
#define RADIO_POWER_PIN   A2
#define REGULATOR_POWERUP A3
#else
#define BUTTON_MODE_L 5 // ATMega328P-PU PDIP pin 11, PD5, PCINT21, no analogue input.
#endif

#ifdef ENABLE_LEARN_BUTTON
// OPTIONAL UI 'learn' button  (active/pulled low by button, pref using weak internal pull-up), digital in.
#define BUTTON_LEARN_L 8 // ATMega328P-PU PDIP pin 14, PB0, PCINT0, no analogue input.
#ifndef ENABLE_VOICE_SENSOR // From REV2 onwards.
// OPTIONAL SECOND UI 'learn' button  (active/pulled low by button, pref using weak internal pull-up), digital in.
#define BUTTON_LEARN2_L 3 // ATMega328P-PU PDIP pin 5, PD3, PCINT19, no analogue input.
#endif // ENABLE_VOICE_SENSOR
#else
// For boards that have the LEARN circuitry fitted, evenm if not used,
// the lines must be pulled high anyway to avoid draining the battery.
#if (V0p2_REV == 1) || (V0p2_REV == 2) || (V0p2_REV == 3) || (V0p2_REV == 7)
#define BUTTON_LEARN_L_DUMMY 8 // ATMega328P-PU PDIP pin 14, PB0, PCINT0, no analogue input.
#endif
#if (V0p2_REV == 2) || (V0p2_REV == 3) || (V0p2_REV == 7)
#define BUTTON_LEARN2_L_DUMMY 3 // ATMega328P-PU PDIP pin 5, PD3, PCINT19, no analogue input.
#endif

#endif

// Setup voice NIRQ line
// TODO add check for if BUTTON_LEARN2_L also defined? 
#ifdef ENABLE_VOICE_SENSOR
#if (V0p2_REV == 10)
// Voice detect on falling edge.
#define VOICE_NIRQ 3 // ATMega328P-PU PDIP pin 5, PD3, PCINT19, no analogue input.
#elif (V0p2_REV == 14)
#define VOICE_NIRQ 6 //todo fill this bit in
#endif // V0p2_REV
#endif // ENABLE_VOICE_SENSOR

// Pin to power-up I/O devices only intermittently enabled, when high, digital out.
// Pref connected via 330R+ current limit and 100nF+ decoupling).
#define IO_POWER_UP (::OTV0P2BASE::V0p2_PIN_DEFAULT_IO_POWER_UP) // ATMega328P-PU PDIP pin 13, PD7, no usable analogue input.

// Ambient light sensor (eg LDR) analogue input: higher voltage means more light.
#define LDR_SENSOR_AIN (::OTV0P2BASE::V0p2_PIN_LDR_SENSOR_AIN) // 0: ATMega328P-PU PDIP pin 23, PC0.

// Temperature potentiometer is present in REV 2/3/4/7/20.
#if ((V0p2_REV >= 2) && (V0p2_REV <= 4)) || (V0p2_REV == 7) || (V0p2_REV == 20)
// Analogue input from pot.
#define TEMP_POT_AIN (::OTV0P2BASE::V0p2_PIN_TEMP_POT_AIN) // AI1: ATMega328P-PU PDIP pin 24, PC1.
// IF DEFINED: reverse the direction of temperature pot polarity.
#if (V0p2_REV != 7) && (V0p2_REV != 20) // For DORM1/REV7 natural direction for temp dial pot is correct.
#define TEMP_POT_REVERSE
#endif
#endif

// RFM23B nIRQ interrupt line; all boards *should* now have it incl REV0 as breadboard; REV0 *PCB* didn't.
#if (V0p2_REV != 1) // DHD20150825: REV1 board currently under test behaves as if IRQ not fitted.
#define PIN_RFM_NIRQ 9 // ATMega328P-PU PDIP pin 15, PB1, PCINT1.
#else
// Use weak pull-up to avoid contention current or floating.
#define PIN_RFM_NIRQ_DUMMY 9 // ATMega328P-PU PDIP pin 15, PB1, PCINT1.
#endif

// REV7/20 motor connections.
#if (V0p2_REV == 7) || (V0p2_REV == 20)
// MI: Motor Indicator (stalled current sensor) ADC6
// MC: Motor Count from shaft encoder optical ADC7
#define MOTOR_DRIVE_MI_AIN 6
#define MOTOR_DRIVE_MC_AIN 7
#endif
//
// ML and MR always defined so as to be able to set them to safe and low-power states on all boards.
// They would normally be analogue inputs which is safe but leaves inputs drifting,
// so if not being used they should be pulled up weakly (or possibly driven high).
// ML: Motor Left PC2 / AI2 / DI16 / p25 on PDIP
// MR: Motor Right PC3 / AI3 / DI17 / p26 on PDIP
//
// WARNING WARNING WARNING
// MR AND ML MUST NOT BE PULLED LOW AT THE SAME TIME
// ELSE THERE IS A SHORT THROUGH THE H-BRIDGE ACROSS THE SUPPLY.
// WARNING WARNING WARNING
//
// FIXME On boards with DRV8850 H-Bridge (i.e. REV20), pins should both be pulled low to reduce power consumption.
//		 As of REV21 the motor drive pins will be changed to prevent accidentally killing REV7s.
//
// Addressed as digital I/O.
#ifndef __AVR_ATmega328P__
#define MOTOR_DRIVE_ML A2
#define MOTOR_DRIVE_MR A3
#else
#define MOTOR_DRIVE_ML 16
#define MOTOR_DRIVE_MR 17
#endif



// Note: I/O budget for motor drive probably 4 pins minimum.
// 2D: To direct drive motor this will need 2 outputs for H-bridge.
// 1A: Then some sort of end-stop sensor (eg current draw) analogue input
// 1I: and/or pulse input/counter/interrupt
// ID: and some supply to pulse counter mechanism (eg LED for opto) maybe IO_POWER_UP.

// Call this ASAP in setup() to configure I/O safely for the board, avoid pins floating, etc.
static inline void IOSetup()
  {
  // Initialise all digital I/O to safe state ASAP and avoid floating lines where possible.
  // In absence of a specific alternative, drive low as an output to minimise consumption (eg from floating input).
  for(int i = 14; --i >= 0; ) // For all digital pins from 0 to 13 inclusive...
    {
    switch(i)
      {
      // Low output is good safe low-power default.
      // NOTE: not good for some such as DORM1/REV7 ML+MR motor H-bridge outputs!
      default: { digitalWrite(i, LOW); pinMode(i, OUTPUT); break; }

#if !defined(ALT_MAIN_LOOP)
      // Switch main UI LED on for the rest of initialisation in non-ALT code...
#ifndef V0P2BASE_LED_HEATCALL_IS_L
      case LED_HEATCALL: { pinMode(LED_HEATCALL, OUTPUT); digitalWrite(LED_HEATCALL, HIGH); break; }
#endif // V0P2BASE_LED_HEATCALL_IS_L
#ifdef V0P2BASE_LED_HEATCALL_IS_L
      case LED_HEATCALL_L: { pinMode(LED_HEATCALL_L, OUTPUT); digitalWrite(LED_HEATCALL_L, LOW); break; }
#endif // V0P2BASE_LED_HEATCALL_IS_L
#else // !defined(ALT_MAIN_LOOP)
#ifdef V0P2BASE_LED_HEATCALL_IS_L
      // Leave main UI LED off in ALT-mode eg in case on minimal power from energy harvesting.
      case LED_HEATCALL_L: { pinMode(LED_HEATCALL_L, OUTPUT); digitalWrite(LED_HEATCALL_L, HIGH); break; }
#endif // LED_HEATCALL_L
#endif // !defined(ALT_MAIN_LOOP)

      // Switch secondary UI LED off during initialisation.
#ifdef LED_UI2_EXISTS
      case LED_UI2_L: { pinMode(LED_UI2_L, OUTPUT); digitalWrite(LED_UI2_L, HIGH); break; }
#endif // LED_UI2_EXISTS
#ifdef VOICE_NIRQ
      // Weak pull-up for external activation by pull-down.
      case VOICE_NIRQ: { pinMode(VOICE_NIRQ, INPUT); break; }
#endif
#ifdef PIN_RFM_NIRQ 
      // Set as input to avoid contention current.
      case PIN_RFM_NIRQ: { pinMode(PIN_RFM_NIRQ, INPUT); break; }
#endif // PIN_RFN_NIRQ
#ifdef PIN_RFM_NIRQ_DUMMY
      // Set as input to avoid contention current or float.
      case PIN_RFM_NIRQ_DUMMY: { pinMode(PIN_RFM_NIRQ_DUMMY, INPUT_PULLUP); break; }
#endif // PIN_RFM_NIRQ_DUMMY
      // Make button pins (and others) inputs with internal weak pull-ups
      // (saving an external resistor in each case if aggressively reducing BOM costs).
#ifdef BUTTON_MODE_L
      case BUTTON_MODE_L: // Mode button is (usually!) mandatory, at least where a local TRV is being controlled.
#endif
#ifdef SOFTSERIAL_TX_PIN
      case SOFTSERIAL_TX_PIN: // When driving SIM900 this pin has external pull-up so should start high.
#endif
#ifdef SOFTSERIAL_RX_PIN
      case SOFTSERIAL_RX_PIN: // When driving SIM900 this pin has external pull-up so should start high.
#endif
#ifdef BUTTON_LEARN_L
      case BUTTON_LEARN_L: // Learn button is optional.
#elif defined(BUTTON_LEARN_L_DUMMY)
      case BUTTON_LEARN_L_DUMMY: // Learn button must still not be pulled low.
#endif
#ifdef BUTTON_LEARN2_L
      case BUTTON_LEARN2_L: // Learn button 2 is optional.
#elif defined(BUTTON_LEARN2_L_DUMMY)
      case BUTTON_LEARN2_L_DUMMY: // Learn button must still not be pulled low.
#endif
#ifdef V0P2_ENABLE_SPI//PIN_SPI_nSS // fixme temporary hack around IDE problems.
      // Do not leave/set SPI nSS as low output (or floating) to avoid waking up SPI slave(s).
      case V0p2_PIN_SPI_nSS://PIN_SPI_nSS: // FIXME these used to be #defines.
//#endif
//#ifdef PIN_SPI_MISO
      // Do not leave/set SPI MISO as low output (or floating).
      case V0p2_PIN_SPI_MISO://PIN_SPI_MISO:
#endif
#ifdef PIN_OW_DQ_DATA
      // Weak pull-up to avoid leakage current.
      case PIN_OW_DQ_DATA:
#endif
#ifdef PIN_SERIAL_RX
      // Weak TX and RX pull-up empirically found to produce lowest leakage current
      // when 2xAA NiMH battery powered and connected to TTL-232R-3V3 USB lead.
      case PIN_SERIAL_RX: case PIN_SERIAL_TX:
#endif
#ifdef MOTOR_DRIVE_ML
      // Weakly pull up both motor REV7 H-bridge driver lines by default.
      // Safe for all boards and may reduce parasitic floating power consumption on non-REV7 boards.
      case MOTOR_DRIVE_ML: case MOTOR_DRIVE_MR:
#endif
        { pinMode(i, INPUT_PULLUP); break; }
      }
    }
  }


}

#endif // ARDUINO

#endif


