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

Author(s) / Copyright (s): Deniz Erbilgin 2015--2016
                           Damon Hart-Davis 2015--2016
*/

/*
 * SIM900 Arduino (2G) GSM shield support.
 *
 * V0p2/AVR only.
 */

#include "OTSIM900Link_OTSIM900Link.h"

namespace OTSIM900Link
{


//const char *AT_ = "";
const char *OTSIM900LinkBase::AT_START = "AT";
const char *OTSIM900LinkBase::AT_SIGNAL = "+CSQ";
const char *OTSIM900LinkBase::AT_NETWORK = "+COPS";
const char *OTSIM900LinkBase::AT_REGISTRATION = "+CREG"; // GSM registration.
const char *OTSIM900LinkBase::AT_GPRS_REGISTRATION0 = "+CGATT"; // GPRS registration.
const char *OTSIM900LinkBase::AT_GPRS_REGISTRATION = "+CGREG"; // GPRS registration.
const char *OTSIM900LinkBase::AT_SET_APN = "+CSTT";
const char *OTSIM900LinkBase::AT_START_GPRS = "+CIICR";
const char *OTSIM900LinkBase::AT_GET_IP = "+CIFSR";
const char *OTSIM900LinkBase::AT_PIN = "+CPIN";
const char *OTSIM900LinkBase::AT_STATUS = "+CIPSTATUS";
const char *OTSIM900LinkBase::AT_START_UDP = "+CIPSTART";
const char *OTSIM900LinkBase::AT_SEND_UDP = "+CIPSEND";
const char *OTSIM900LinkBase::AT_CLOSE_UDP = "+CIPCLOSE";
const char *OTSIM900LinkBase::AT_SHUT_GPRS = "+CIPSHUT";
const char *OTSIM900LinkBase::AT_VERBOSE_ERRORS = "+CMEE";


} // OTSIM900Link
