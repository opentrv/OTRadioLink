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

// Min steady/grace time after lights on to confirm occupancy.
// Intended to prevent a brief flash of light,
// or quickly turning on lights in the night to find something,
// from firing up the entire heating system.
// This threshold may be applied conditionally,
// eg when previously v dark.
// Not so long as to fail to respond to genuine occupancy.
//
// This threshold may be useful elsewhere
// to suppress over-hasty response
// to a very brief lights-on, eg in the middle of the night.
#ifndef SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinWithLightOn
#define SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinWithLightOn 3
#endif // SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinWithLightOn

// Minimum steady time for detecting artificial light (ticks/minutes).
#ifndef SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinForArtificialLight
#define SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinForArtificialLight 30
#endif // SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinForArtificialLight

// Minimum steady time for detecting light on (ticks/minutes).
// Should be short enough to notice someone going to make a cuppa.
// Note that an interval <= TX interval may make it harder to validate
// algorithms from routinely collected data,
// eg <= 4 minutes with typical secure frame rate of 1 per ~4 minutes.
#ifndef SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinBeforeLightOn
#define SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinBeforeLightOn 3
#endif // SENSORAMBIENTLIGHTOCCUPANCY_steadyTicksMinBeforeLightOn

#endif /* OTV0P2BASE_SENSORAMBIENTLIGHTOCCUPANCY_TUNEABLE_H */
