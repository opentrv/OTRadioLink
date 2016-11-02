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
               - isUnavailable?
               - isVoiceDetected (should the sensors have a common api?)

 Only V0p2/AVR for now.
 */

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SENSORQM1_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SENSORQM1_H_

#include "OTV0P2BASE_Util.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_Serial_IO.h"

namespace OTV0P2BASE
{


#ifdef ARDUINO_ARCH_AVR

#define VoiceDetectionQM1_DEFINED
class VoiceDetectionQM1 final : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  private:
    // Activity count.
    // Marked volatile for thread-safe (simple) lock-free access.
    volatile uint8_t count;
    // True if voice is detected.
    // Marked volatile for thread-safe lock-free access.
    volatile bool isDetected;
    // True if in low power mode.
    // Marked volatile for thread-safe (simple) lock-free access.
//    volatile bool isLowPwr;
    // Last time sensor was polled
    // Marked volatile for thread-safe (simple) lock-free access.
//    volatile uint16_t endOfLocking;
//    // True if there is new data to poll
//    // Marked volatile for thread-safe (simple) lock-free access.
//    volatile bool isTriggered;
//    // Lock out time after interrupt
//    // only needs to be > 10secs, but go for between 2 mins to make sure (we have a 4 min cycle anyway)
//    static const uint8_t lockingPeriod = 2;
    // 'Possible occupancy' callback function (for moderate confidence of human presence).
    // If not NULL, is called when this sensor detects indications of occupancy.
    void (*possOccCallback)();


  public:
    // Initialise to cautious values.
    VoiceDetectionQM1() : count(0), isDetected(false), possOccCallback(NULL) { }

    // Force a read/poll of the voice level and return the value sensed.
    // Potentially expensive/slow.
    // Thread-safe and usable within ISRs (Interrupt Service Routines), though not recommended.
    virtual uint8_t read();

    // Returns preferred poll interval (in seconds); non-zero.
    virtual uint8_t preferredPollInterval_s() const { return(240); }

    // Handle simple interrupt.
    // Fast and ISR (Interrupt Service Routines) safe.
    // Returns true if interrupt was successfully handled and cleared
    // else another interrupt handler in the chain may be called
    // to attempt to clear the interrupt.
    virtual bool handleInterruptSimple();

    // Set 'possible occupancy' callback function (for moderate confidence of human presence); NULL for no callback.
    void setPossOccCallback(void (*possOccCallback_)()) { possOccCallback = possOccCallback_; }


    // Returns true if voice has been detected in this or previous poll period.
    bool isVoiceDetected() { return(isDetected); }

    // Returns true if more than a minute has passed since last interrupt and sensor has not been polled.
//    bool isVoiceReady() { return (isTriggered && (OTV0P2BASE::getMinutesSinceMidnightLT() >= endOfLocking)); }

    // Returns a suggested (JSON) tag/field/key name including units of get(); NULL means no recommended tag.
    // The lifetime of the pointed-to text must be at least that of the Sensor instance.
    virtual const char *tag() const { return("av"); }

  };

#endif // ARDUINO_ARCH_AVR


}


#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SENSORQM1_H_ */
