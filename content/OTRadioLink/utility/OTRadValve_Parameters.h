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

Author(s) / Copyright (s): Damon Hart-Davis 2016
*/

/*
 * OpenTRV radiator valve basic parameters.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_PARAMETERS_H
#define ARDUINO_LIB_OTRADVALVE_PARAMETERS_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


    // Minimum and maximum bounds target temperatures; degrees C/Celsius/centigrade, strictly positive.
    // Minimum is some way above 0C to avoid freezing pipework even with small measurement errors and non-uniform temperatures.
    // Maximum is set a little below boiling/100C for DHW applications for safety.
    // Setbacks and uplifts cannot move temperature targets outside this range for safety.
    static const uint8_t MIN_TARGET_C = 5; // Minimum temperature setting allowed (to avoid freezing, allowing for offsets at temperature sensor, etc).
    static const uint8_t MAX_TARGET_C = 95; // Maximum temperature setting allowed (eg for DHW).

    // 18C is a safe room temperature even for the slightly infirm according to NHS England 2014:
    //    http://www.nhs.uk/Livewell/winterhealth/Pages/KeepWarmKeepWell.aspx
    // Small babies have relatively poor thermoregulation so a device with setbacks may not be suitable for them, else ~18C is good:
    //    http://www.nhs.uk/conditions/pregnancy-and-baby/pages/reducing-risk-cot-death.aspx
    // so could possibly be marked explicitly on the control.
    // 21C is recommended living temperature in retirement housing:
    //     http://ipc.brookes.ac.uk/publications/pdf/Identifying_the_health_gain_from_retirement_housing.pdf
    static const uint8_t SAFE_ROOM_TEMPERATURE = 18; // Safe for most purposes.


    }

#endif
