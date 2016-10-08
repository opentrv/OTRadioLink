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
*/

/*
 Plug-in for ambient light sensor to provide occupancy detection.

 Provides an interface and a reference implementation.
 */

#ifndef OTV0P2BASE_SENSORAMBLIGHTOCCUPANCYDETECTION_H
#define OTV0P2BASE_SENSORAMBLIGHTOCCUPANCYDETECTION_H

#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE
{


// Sensor for ambient light level; 0 is dark, 255 is bright.
#define SensorAmbientLightOccupancyDetectorInterface_DEFINED
class SensorAmbientLightOccupancyDetectorInterface
  {
  public:
    // Default value for (default)LightThreshold.
    // Works for normal LDR pointing forward.
    static const uint8_t DEFAULT_LIGHT_THRESHOLD = 50;

  private:

  public:
  };


}
#endif
