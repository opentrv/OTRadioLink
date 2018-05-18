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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2017
*/

#include <stdlib.h>
#include "utility/OTRadValve_AbstractRadValve.h"

#include "OTRadValve_ModelledRadValve.h"

namespace OTRadValve
    {

#ifdef ModelledRadValve_DEFINED

// // Return minimum valve percentage open to be considered actually/significantly open; [1,100].
// // At the boiler hub this is also the threshold percentage-open on eavesdropped requests that will call for heat.
// // If no override is set then OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN is used.
// uint8_t ModelledRadValve::getMinValvePcReallyOpen() const
//   {
// #ifdef ARDUINO_ARCH_AVR
//   const uint8_t stored = eeprom_read_byte((uint8_t *)V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN);
//   const uint8_t result = ((stored > 0) && (stored <= 100)) ? stored : OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN;
//   return(result);
// #else
//   return(OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN);
// #endif // ARDUINO_ARCH_AVR
//   }

// // Set and cache minimum valve percentage open to be considered really open.
// // Applies to local valve and, at hub, to calls for remote calls for heat.
// // Any out-of-range value (eg >100) clears the override and OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN will be used.
// #ifdef ARDUINO_ARCH_AVR
// void ModelledRadValve::setMinValvePcReallyOpen(const uint8_t percent)
//   {
//   if((percent > 100) || (percent == 0) || (percent == OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN))
//     {
//     // Bad / out-of-range / default value so erase stored value if not already erased.
//     OTV0P2BASE::eeprom_smart_erase_byte((uint8_t *)V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN);
//     return;
//     }
//   // Store specified value with as low wear as possible.
//   OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_MIN_VALVE_PC_REALLY_OPEN, percent);
//   }
// #else
// void ModelledRadValve::setMinValvePcReallyOpen(const uint8_t /*percent*/) { }
// #endif // ARDUINO_ARCH_AVR

// // True if the controlled physical valve is thought to be at least partially open right now.
// // If multiple valves are controlled then is this true only if all are at least partially open.
// // Used to help avoid running boiler pump against closed valves.
// // The default is to use the check the current computed position
// // against the minimum open percentage.
// bool ModelledRadValve::isControlledValveReallyOpen() const
//   {
// //  if(isRecalibrating()) { return(false); }
//   if(NULL != physicalDeviceOpt) { if(!physicalDeviceOpt->isControlledValveReallyOpen()) { return(false); } }
//   return(value >= getMinPercentOpen());
//   }

// // Compute target temperature and set heat demand for TRV and boiler; update state.
// // CALL REGULARLY APPROXIMATELY ONCE PER MINUTE TO ALLOW SIMPLE TIME-BASED CONTROLS.
// //
// // This routine may take significant CPU time.
// //
// // Internal state is updated, and the target updated on any attached physical valve.
// //
// // Will clear any BAKE mode if the newly-computed target temperature is already exceeded.
// void ModelledRadValve::computeCallForHeat()
//   {
//   valveModeRW->read();
//   // Compute target temperature,
//   // ensure that input state is set for computeRequiredTRVPercentOpen().
//   computeTargetTemperature();
//   // Invoke computeRequiredTRVPercentOpen()
//   // and convey new target to the backing valve if any,
//   // while tracking any cumulative movement.
//   retainedState.tick(value, inputState, physicalDeviceOpt);
//   }

// // Compute/update target temperature and set up state for computeRequiredTRVPercentOpen().
// // Can be called as often as required though may be slowish/expensive.
// // Can be called after any UI/CLI/etc operation
// // that may cause the target temperature to change.
// // (Will also be called by computeCallForHeat().)
// // One aim is to allow reasonable energy savings (10--30%)
// // even if the device is left in WARM mode all the time,
// // using occupancy/light/etc to determine when temperature can be set back
// // without annoying users.
// //
// // Will clear any BAKE mode if the newly-computed target temperature is already exceeded.
// void ModelledRadValve::computeTargetTemperature()
//   {
//   // Compute basic target temperature statelessly.
//   const uint8_t newTargetTemp = ctt->computeTargetTemp();

//   // Set up state for computeRequiredTRVPercentOpen().
//   ctt->setupInputState(inputState,
//     retainedState.isFiltering,
//     newTargetTemp, getMinPercentOpen(), getMaxPercentageOpenAllowed(), glacial);

//   // Explicitly compute the actual setback when in WARM mode for monitoring purposes.
//   // TODO: also consider showing full setback to FROST when a schedule is set but not on.
//   // By default, the setback is regarded as zero/off.
//   setbackC = 0;
//   if(valveModeRW->inWarmMode())
//     {
//     const uint8_t wt = tempControl->getWARMTargetC();
//     if(newTargetTemp < wt) { setbackC = wt - newTargetTemp; }
//     }

//   // True if the target temperature has been reached or exceeded.
//   const bool targetReached = (newTargetTemp <= (inputState.refTempC16 >> 4));
//   underTarget = !targetReached;
//   // If the target temperature is already reached then cancel any BAKE mode in progress (TODO-648).
//   if(targetReached) { valveModeRW->cancelBakeDebounced(); }
//   // Only report as calling for heat when actively doing so.
//   // (Eg opening the valve a little in case the boiler is already running does not count.)
//   callingForHeat = !targetReached &&
//     (value >= OTRadValve::DEFAULT_VALVE_PC_SAFER_OPEN) &&
//     isControlledValveReallyOpen();
//   }

#endif // ModelledRadValve_DEFINED


    }


//// Median filter.
//// Find mean of interquartile range of group of ints where sum can be computed in an int without loss.
//// FIXME: needs a unit test or three.
//template<uint8_t N> int smallIntIQMean(const int data[N])
//  {
//  // Copy array content.
//  int copy[N];
//  for(int8_t i = N; --i >= 0; ) { copy[i] = data[i]; }
//  // Sort in place with a bubble sort (yeuck) assuming the array to be small.
//  // FIXME: replace with insertion sort for efficiency.
//  // FIXME: break out sort as separate subroutine.
//  uint8_t n = N;
//  do
//    {
//    uint8_t newn = 0;
//    for(uint8_t i = 0; ++i < n; )
//      {
//      const int c0 = copy[i-1];
//      const int c1 = copy[i];
//      if(c0 > c1)
//         {
//         copy[i] = c0;
//         copy[i-1] = c1;
//         newn = i;
//         }
//      }
//    n = newn;
//    } while(0 != n);
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINT_FLASHSTRING("sorted: ");
//for(uint8_t i = 0; i < N; ++i) { DEBUG_SERIAL_PRINT(copy[i]); DEBUG_SERIAL_PRINT(' '); }
//DEBUG_SERIAL_PRINTLN();
//#endif
//  // Extract mean of interquartile range.
//  const size_t sampleSize = N/2;
//  const size_t start = N/4;
//  // Assume values will be nowhere near the extremes.
//  int sum = 0;
//  for(uint8_t i = start; i < start + sampleSize; ++i) { sum += copy[i]; }
//  // Compute rounded-up mean.
//  return((sum + sampleSize/2) / sampleSize);
//  }
