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

Author(s) / Copyright (s): Deniz Erbilgin 2015
                           Damon Hart-Davis 2015
*/

#include <OTV0p2Base.h>
#include <OTRadioLink.h>
#include <OTSIM900Link.h>

// Geeetech board needs solder jumper made for D9 to drive power pin.
// http://www.geeetech.com/Documents/GPRSshield_sch.pdf

/**
 * Temporary class for OTSIM900 tests
 * How To:
 * - Set apn and pin below if required.
 * - Set UDP connection values (IP address and port)
 * - Flash arduino
 * 
 * - Board will automatically power up module when serial started.
 * - To check in software if module is powered, enter 'p'
 * - To attempt to force power up, enter 'P'
 * - To check if pin required, enter 'c'
 * - If pin required, enter 'u'
 * - To print module name to console, enter 'm'
 * - To print connected network to console, enter 'n'
 * - To check network registration, enter 'R'
 * - To set access point name, enter 'r'
 * - To start GPRS, enter 'g'
 * - To check IP address, enter 'i'
 * - To open UDP connection, enter 'o'
 * - To send UDP packet, enter 's'
 * - To close UDP connection, enter 'e'
 * - For non verbose errors, enter 'v'
 * - For verbose errors, enter 'V'
 * - Board will close PGP on shutdown (Will add in a method to do this seperately later)
 */

static const bool bEEPROM = false;  // Sets whether radio saved in flash or eeprom

#define CREDS 1 // Choose set of hardwired credentials and target address.


#if 1 == CREDS // DE
static const char apn[] = "m2mkit.telefonica.com"; // "m2mkit.telefonica.com"
static const char pin[] = "0000";
static const char UDP_ADDR[] = "46.101.52.242";
static const char UDP_PORT[] = "9999";
#elif 2 == CREDS // DHD
static const char apn[] = "m2mkit.telefonica.com";
static const char pin[] = "0000";
static const char UDP_ADDR[] = "79.135.97.66";
static const char UDP_PORT[] = "9999";
#endif


static const uint8_t UDP_SEND_STR[] = "The cat in the hat battery";



const OTSIM900Link::OTSIM900LinkConfig_t radioConfig = {bEEPROM, pin, apn, UDP_ADDR, UDP_PORT};

OTSIM900Link::OTSIM900Link gprs(&radioConfig, 6, 7, 8);

void setup()
{
 // Serial.begin(4800);
  gprs.begin();
  //Serial.println("Setup done");
  delay(5000);
  //Serial.println("sending");
  //Serial.print(gprs.queueToSend(UDP_SEND_STR, sizeof(UDP_SEND_STR)));
  gprs.queueToSend(UDP_SEND_STR, sizeof(UDP_SEND_STR));
}

void loop()
{

}
