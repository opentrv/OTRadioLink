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
                           Deniz Erbilgin   2016
*/

/*
 Voice sensor.

 EXPERIMENTAL!!! API IS SUBJECT TO CHANGE!

 NOTE: Currently does not have a good way of clearing its count and still
       actually sending voice data. As a workaround, clears data every 4 mins,
       meaning that if Txing more frequently than that, will repeat send
       previous value

 Functionality and code only enabled if ENABLE_VOICE_SENSOR is defined.

   @todo   Check functions:
               - isAvailable?
               - isVoiceDetected (should the sensors have a common api?)

 Only V0p2/AVR for now.
 */

#include <stdint.h>

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h> // Arduino I2C library.
#endif

#include "OTV0P2BASE_SensorQM1.h"


namespace OTV0P2BASE
{


#ifdef VoiceDetectionQM1_DEFINED

// If count meets or exceeds this threshold in one poll period then
// the room is deemed to be occupied.
// Strictly positive.
// DHD20151119: even now it seems a threshold of >= 2 is needed to avoid false positives.
// DE20160101:  Lowered detection threshold as new boards have lower sensitivity
static const uint8_t voiceDetectionThreshold = 4; // TODO move into class?
static const uint8_t QM1_I2C_ADDR = 0x09;
static const uint8_t QM1_I2C_CMD_PERIOD_MASK  = 0x40;   // Mask for setting time between measurements, in 10s of seconds. Bitwise AND with a value between 0x01 and 0x3F
static const uint8_t QM1_I2C_CMD_RST_PERIOD   = 0x01;   // Reset period to default (4 mins)
static const uint8_t QM1_I2C_CMD_SET_PERIOD_3 = 0x03;   // Set period to 3 times measurement time (40 secs?)
static const uint8_t QM1_I2C_CMD_SET_LOW_PWR  = 0x04;   // Set processor to low power mode
static const uint8_t QM1_I2C_CMD_SET_NORM_PWR = 0x05;   // Set processor to normal mode

// Set true once QM-1 has been initialised.
static volatile bool QM1_initialised = false;

/**
 * @brief   Set up QM1 for low power operation
 * @todo    How to do this TBC.
 */
static void QM1_init()
{
	const bool neededPowerUp = OTV0P2BASE::powerUpTWIIfDisabled();

	Wire.beginTransmission(QM1_I2C_ADDR);
	Wire.write( (byte)(0b01000001) );
	Wire.endTransmission();
	Wire.beginTransmission(QM1_I2C_ADDR);
	Wire.write( (byte)QM1_I2C_CMD_SET_LOW_PWR );
	Wire.endTransmission();
	QM1_initialised = true;

	// Power down TWI ASAP.
	if(neededPowerUp) { OTV0P2BASE::powerDownTWI(); }
}

// Force a read/poll of the voice level and return the value sensed.
// Thread-safe and ISR-safe.
uint8_t VoiceDetectionQM1::read()
{
  if(!QM1_initialised) QM1_init();

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    isDetected = ((value = count) >= voiceDetectionThreshold);
    // clear count every 4 mins
    // sensor is only triggered every 4 mins so this *should* work
    if( !(getMinutesSinceMidnightLT() & 0x03) ) count = 0; // FIXME ugly hack. When are we going to have proper sensor read scheduling?
  }
  return(value);
}

// Handle simple interrupt.
// Fast and ISR (Interrupt Service Routines) safe.
// Returns true if interrupt was successfully handled and cleared
// else another interrupt handler in the chain may be called
// to attempt to clear the interrupt.
bool VoiceDetectionQM1::handleInterruptSimple()
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    // Count of voice activations since last poll, avoiding overflow.
    if ((count < 255) && (++count >= voiceDetectionThreshold))
    {
      // Act as soon as voice is detected.
      isDetected = true;
      // Don't regard this as a very strong indication,
      // as it could be a TV or radio on in the room.
      possOccCallback();
    }
  }

  OTV0P2BASE::serialPrintAndFlush("v");
  // No further work to be done to 'clear' interrupt.
  return (true);
}

#endif // VoiceDetectionQM1_DEFINED


}
