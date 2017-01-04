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
class ErrorReport final : public OTV0P2BASE::Sensor<int8_t>
    {
    private:
        // The current error value; zero means none, +ve error, -ve warning.
        volatile int8_t value = 0;

    public:
        // Return last value fetched by read(); undefined before first read().
        // Usually fast.
        // Likely to be thread-safe or usable within ISRs (Interrupt Service Routines),
        // BUT READ IMPLEMENTATION DOCUMENTATION BEFORE TREATING AS thread/ISR-safe.
        virtual int8_t get() const override { return(value); }

    public:
        virtual int8_t read() override { return(get()); }
    };

}

#endif
