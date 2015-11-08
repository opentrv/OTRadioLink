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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
*/

//#include <OTV0p2Base.h>

#include "OTRadValve_ModelledRadValve.h"

#include "utility/OTRadValve_AbstractRadValve.h"

namespace OTRadValve
    {


// Offset from raw temperature to get reference temperature in C/16.
static const int8_t refTempOffsetC16 = 8;

ModelledRadValveInputState::ModelledRadValveInputState(const int realTempC16) :
    targetTempC(12 /* FROST */),
    minPCOpen(OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN), maxPCOpen(100),
    widenDeadband(false), glacial(false), hasEcoBias(false), inBakeMode(false)
    { setReferenceTemperatures(realTempC16); }

// Calculate reference temperature from real temperature.
// Proportional temperature regulation is in a 1C band.
// By default, for a given target XC the rad is off at (X+1)C so temperature oscillates around that point.
// This routine shifts the reference point at which the rad is off to (X+0.5C)
// ie to the middle of the specified degree, which is more intuitive,
// and which may save a little energy if users target the specified temperatures.
// Suggestion c/o GG ~2014/10 code, and generally less misleading anyway!
void ModelledRadValveInputState::setReferenceTemperatures(const int currentTempC16)
  {
  const int referenceTempC16 = currentTempC16 + refTempOffsetC16; // TODO-386: push targeted temperature down by 0.5C to middle of degree.
  refTempC16 = referenceTempC16;
  }


    }
