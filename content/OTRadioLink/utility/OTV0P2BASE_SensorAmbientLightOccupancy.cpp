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
 Plug-in for ambient light sensor to provide occupancy detection.

 Provides an interface and a reference implementation.
 */

#include "OTV0P2BASE_SensorAmbientLightOccupancy.h"


namespace OTV0P2BASE
{


// Call regularly with the current ambient light level [0,255].
// Returns true if probably occupancy is detected.
// Does not block.
// Not thread-/ISR- safe.
bool SensorAmbientLightOccupancyDetectorSimple::update(const uint8_t newLightLevel)
    {
    const bool result = newLightLevel > prevLightLevel; // FIXME: needs further conditions.
	prevLightLevel = newLightLevel;
    return(result);
	}


}
