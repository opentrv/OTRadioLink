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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
                            Deniz Erbilgin 2016
*/

#ifndef ARDUINO_LIB_OTRADVALVE_H
#define ARDUINO_LIB_OTRADVALVE_H

#define ARDUINO_LIB_OTRADVALVE_VERSION_MAJOR 1
#define ARDUINO_LIB_OTRADVALVE_VERSION_MINOR 0

// Radiator valve support, abstract and common implementations.

// OpenTRV radiator valve basic parameters..
#include "utility/OTRadValve_Parameters.h"

// Abstract/base interface for basic (thermostatic) radiator valves.
#include "utility/OTRadValve_AbstractRadValve.h"

// OpenTRV model and smart control of (thermostatic) radiator valve.
#include "utility/OTRadValve_ModelledRadValve.h"

// Driver for FHT8V wireless valve actuator (and FS20 protocol encode/decode).
#include "utility/OTRadValve_FHT8VRadValve.h"

// Hardware-independent logic for direct proportional valve motor drive..
#include "utility/OTRadValve_CurrentSenseValveMotorDirect.h"

// Base for TRV1 (DORM1/REV7) and TRV2 direct valve motor drive.
#include "utility/OTRadValve_ValveMotorBase.h"

// Driver for DORM1/REV7 direct valve motor drive.
#include "utility/OTRadValve_ValveMotorDirectV1.h"

// Driver for DRV8850 valve motor drive.
#include "utility/OTRadValve_ValveMotorDRV8850.h"

// Driver for boiler.
#include "utility/OTRadValve_BoilerDriver.h"

// Test motor driver
#include "utility/OTRadValve_TestValve.h"

#endif
