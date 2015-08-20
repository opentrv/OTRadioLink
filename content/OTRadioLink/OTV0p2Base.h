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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

#ifndef ARDUINO_LIB_OTV0P2BASE_H
#define ARDUINO_LIB_OTV0P2BASE_H

#define ARDUINO_LIB_OTV0P2BASE_VERSION_MAJOR 0
#define ARDUINO_LIB_OTV0P2BASE_VERSION_MINOR 9


// Basic immutable GPIO assignments and similar.
#include "utility/OTV0P2BASE_BasicPinAssignments.h"

// Fast GPIO support and micro timing routines.
#include "utility/OTV0P2BASE_FastDigitalIO.h"

// Power management and other misc support.
#include "utility/OTV0P2BASE_PowerManagement.h"

// Quick/simple PRNG (Pseudo-Random Number Generator).
#include "utility/OTV0P2BASE_QuickPRNG.h"

#endif
