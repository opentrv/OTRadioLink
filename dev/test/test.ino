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

#include <SoftwareSerial.h>
#include <OTRadioLink.h>
#include <OTSIM900Link.h>

SoftwareSerial softSer(7,8);  // rx and tx pins for SIM900 module
OTSIM900Link gprs(9, &softSer); // power pin and pointer to SoftwareSerial
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
 * - For verbose errors, enter 'v'
 * - Board will close PGP on shutdown (Will add in a method to do this seperately later)
 */

static const int baud = 19200;  // don't chage this - may break softwareserial

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


static const char UDP_SEND_STR[] = "The cat in the hat";

void setup()
{
  Serial.begin(baud);
  softSer.begin(baud);
  gprs.begin();
  gprs.powerOn();
  delay(1000);
//  gprs.verbose();
  Serial.println("Setup Done");
}

void loop()
{
  if(Serial.available() > 0)
  {
    uint8_t input = Serial.read();
    if (input != '\n') {
      Serial.print("\nInput\t");
      Serial.println((char)input);
      serialInput(input);
    }
  }
  if(softSer.available() >0)
  {
    char incoming_char=softSer.read(); //Get the character from the cellular serial port.
    Serial.print(incoming_char); //Print the incoming character to the terminal.
  }
}

void serialInput(uint8_t input)
{
  char buffer[64];
  int n = 0;
  switch(input) {
    case 'P': // Attempt to force power on if not already so.
      if(!gprs.isPowered()) { gprs.powerOn(); }
      delay(1000);
    case 'p':
      if(gprs.isPowered()) Serial.println("True");
      else Serial.println("False");
      break;

    case 'm':
      gprs.checkModule();
      break;
      
    case 'c':
    Serial.println(gprs.checkPIN() );
    break;

    case 'n':
    gprs.checkNetwork(buffer, sizeof(buffer));
    break;

    case 'r':
    Serial.println(gprs.setAPN(apn, sizeof(apn)-1));  // dont send null termination
    break;

    case 'R':
    Serial.println(gprs.isRegistered());
    break;

    case 'g':
    Serial.println(gprs.startGPRS());
    break;

    case 'i':
    Serial.println(gprs.getIP(buffer));
    Serial.println(buffer);
    break;

    case'u':
    gprs.setPIN(pin, sizeof(pin)-1); // dont send null termination
    break;

    case 'o':
    gprs.openUDP(UDP_ADDR, sizeof(UDP_ADDR)-1, UDP_PORT, sizeof(UDP_PORT)-1); // don't send null termination
    break;

    case 's':
    gprs.sendUDP(UDP_SEND_STR, sizeof(UDP_SEND_STR));
    break;

    case 'e':
    gprs.closeUDP();
    break;

    case 'v':
    gprs.verbose();
    break;

    default:
    break;
  }
}
