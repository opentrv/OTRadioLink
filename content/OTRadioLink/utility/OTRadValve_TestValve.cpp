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

Author(s) / Copyright (s): Deniz Erbilgin 2016
*/

#include "OTRadValve_AbstractRadValve.h"
#include "OTRadValve_ValveMotorDirectV1.h"
#include "OTRadValve_TestValve.h"

#include "OTV0P2BASE_Serial_IO.h"

namespace OTRadValve
{

bool TestValveMotor::runFastTowardsEndStop(bool toOpen)
{
	// Clear the end-stop detection flag ready.
	endStopDetected = false;
	// Run motor as far as possible on this sub-cycle.
	hw->motorRun(~0, toOpen ?
		OTRadValve::HardwareMotorDriverInterface::motorDriveOpening
		: OTRadValve::HardwareMotorDriverInterface::motorDriveClosing, *this);
	// Stop motor and ensure power off.
	hw->motorRun(0, OTRadValve::HardwareMotorDriverInterface::motorOff, *this);
	// Report if end-stop has apparently been hit.
	if (endStopDetected) {
		counter++;
	}
	return(endStopDetected);
}

void TestValveMotor::poll()
{
	if (endStopDetected) state = !state;
	runFastTowardsEndStop(state);
}


}
