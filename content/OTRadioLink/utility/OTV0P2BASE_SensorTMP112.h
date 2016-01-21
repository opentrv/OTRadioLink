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
 TMP112 temperature sensor.
 */

#ifndef OTV0P2BASE_SENSORTMP112_H
#define OTV0P2BASE_SENSORTMP112_H

#include "OTV0P2BASE_SensorTemperatureC16Base.h"


namespace OTV0P2BASE
{


// TMP112 sensor for ambient/room temperature in 1/16th of one degree Celsius.
class RoomTemperatureC16_TMP112 : public OTV0P2BASE::TemperatureC16Base
  { public: virtual int16_t read(); };


}
#endif
