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
V0p2_SIM900_AT_DEFN(AT_START, "AT");
V0p2_SIM900_AT_DEFN(AT_SIGNAL, "+CSQ");
V0p2_SIM900_AT_DEFN(AT_NETWORK, "+COPS");
V0p2_SIM900_AT_DEFN(AT_REGISTRATION, "+CREG"); // GSM registration.
V0p2_SIM900_AT_DEFN(AT_GPRS_REGISTRATION0, "+CGATT"); // GPRS registration.
V0p2_SIM900_AT_DEFN(AT_GPRS_REGISTRATION, "+CGREG"); // GPRS registration.
V0p2_SIM900_AT_DEFN(AT_SET_APN, "+CSTT");
V0p2_SIM900_AT_DEFN(AT_START_GPRS, "+CIICR");
V0p2_SIM900_AT_DEFN(AT_GET_IP, "+CIFSR");
V0p2_SIM900_AT_DEFN(AT_PIN, "+CPIN");
V0p2_SIM900_AT_DEFN(AT_STATUS, "+CIPSTATUS");
V0p2_SIM900_AT_DEFN(AT_START_UDP, "+CIPSTART");
V0p2_SIM900_AT_DEFN(AT_SEND_UDP, "+CIPSEND");
V0p2_SIM900_AT_DEFN(AT_CLOSE_UDP, "+CIPCLOSE");
V0p2_SIM900_AT_DEFN(AT_SHUT_GPRS, "+CIPSHUT");
V0p2_SIM900_AT_DEFN(AT_VERBOSE_ERRORS, "+CMEE");


} // OTSIM900Link
