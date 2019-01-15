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

Author(s) / Copyright (s): Damon Hart-Davis 2016--2018
                           Deniz Erbilgin 2019
*/


#ifndef OTV0P2BASE_SENSORAMBIENTLIGHTOCCUPANCY_TUNEABLE_H
#define OTV0P2BASE_SENSORAMBIENTLIGHTOCCUPANCY_TUNEABLE_H

// Tune the epsilon value in SensorAmbientLightOccupancyDetectorSimple.
// Minimum delta (rise) for probable occupancy to be detected.
// A simple noise floor.
#ifndef SENSORAMBIENTLIGHTOCCUPANCY_EPSILON
#define SENSORAMBIENTLIGHTOCCUPANCY_EPSILON 4
#endif // SENSORAMBIENTLIGHTOCCUPANCY_EPSILON

#endif /* OTV0P2BASE_SENSORAMBIENTLIGHTOCCUPANCY_TUNEABLE_H */
