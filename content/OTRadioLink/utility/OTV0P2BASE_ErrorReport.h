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

Author(s) / Copyright (s): Damon Hart-Davis 2017
*/

/*
 Simple low-frequency error reporting.
 */

#ifndef OTV0P2BASE_ERRORREPORT_H
#define OTV0P2BASE_ERRORREPORT_H

#include <stdint.h>

#include "OTV0P2BASE_Concurrency.h"

#include "OTV0P2BASE_Actuator.h"


namespace OTV0P2BASE
{

/*
 Simple low-frequency error reporting.

 This accepts simple reports of numbered errors (and warnings)
 from an error catalogue or elsewhere.

 The error reporting object is globally available,
 easy to include in stats reports,
 is fast to set an error/warning in,
 and has a mechanism to age old stats eg to save stats bandwidth.

 Errors are strictly positive and the latest is retained,
 no error is marked with zero,
 and warnings are negative.

 Warnings (and zero) do not overwrite extant errors
 until the last error has aged sufficiently.

 The error reporter is a pseudo-'Actuator'
 with the error/warning being set()
 and the last value being retrieved with get().

 Error values are aged with read().

 When an error has aged the 'Actuator' marks itself as unavailable
 to automatically disappear from stats reports for example.

 The error type is a single (signed) byte
 to make thread-/ISR- safety as cheap as possible.

 TODO: will have ISR-/thread- safe entry-point for reporting from such routines.
 */
#define OTV0P2BASE_ErrorReport_DEFINED
class ErrorReport final : public OTV0P2BASE::Actuator<int8_t>
    {
    public:
        // Error (and warning) catalogue.
        // Errors are positive.
        // Warnings are negative.
        // Zero is not an error nor a warning.
        enum errorCatalogue : int8_t
            {
            WARN_UNSPECIFIED = -1, // Unspecified warning.
            ERR_NONE = 0, // Not an error.
            ERR_UNSPECIFIED = 1, // Unspecified error.
            };

    private:
        // The current error value; zero means none, +ve error, -ve warning.
        volatile int8_t value = 0;

        // Ticks (minutes) that freshly-set error/warning takes to expire.
        // Kept long enough to eg make a successful stats transmission likely.
        static constexpr uint8_t DEFAULT_TIMEOUT = 10;
        // If non-zero then error/warning has recently been set; counts to zero.
        // Marked volatile for thread-safe lock-free non-read-modify-write
        // access to byte-wide value.
        // Compound operations on this value must block interrupts.
        volatile OTV0P2BASE::Atomic_UInt8T timeoutTicks;

        // True if any extant warning/error has aged out.
        bool isAged() const { return(0 == timeoutTicks.load()); }

    public:
        // Set new error (+ve) / warning (-ve), or zero to clear.
        // Errors cannot be overwritten by anything other than another error
        // unless the error is aged.
        // NOT thread-/ISR- safe.
        virtual bool set(const int8_t newValue)
          {
          if((newValue > 0) || isAged())
              {
              value = newValue;
              timeoutTicks.store(DEFAULT_TIMEOUT);
              return(true);
              }
          // Cannot override current value.
          return(false);
          }
        // Convenience method to set directly with the enum value.
        bool set(const errorCatalogue err) { return(set(int8_t(err))); }

        // Returns (JSON) tag/field/key name, no units, never NULL.
        virtual Sensor_tag_t tag() const override { return(V0p2_SENSOR_TAG_F("err")); }

        // Return last error/warning, or 0 if none.
        // Thread/ISR-safe.
        virtual int8_t get() const override { return(value); }

        // Typically called once per minute to age error.
        virtual uint8_t preferredPollInterval_s() const override { return(60); }

        // Age any live error/warning and return it; 0 if nothing set.
        virtual int8_t read() override
            { OTV0P2BASE::safeDecIfNZWeak(timeoutTicks); return(get()); }

        // Returns true if there is a non-aged error or warning set.
        virtual bool isAvailable() const override { return(!isAged()); }
    };

// Global instance.
extern ErrorReport ErrorReporter;

}

#endif
