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


static const char UDP_SEND_STR[] = "The cat in the hat";



const OTSIM900Link::OTSIM900LinkConfig_t radioConfig = {bEEPROM, pin, apn, UDP_ADDR, UDP_PORT};

OTSIM900Link::OTSIM900Link gprs(&radioConfig, 6, 7, 8);

void setup()
{
  Serial.begin(4800);
  Serial.println("Setup Start");
  gprs.begin();
  delay(1000);
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
/*  if(gprs.serAvailable() >0)
  {
    char incoming_char=(char)gprs.read(); //Get the character from the cellular serial port.
    Serial.print(incoming_char); //Print the incoming character to the terminal.
  }*/
}

void serialInput(uint8_t input)
{
  switch(input) {
    case 'P': // Attempt to force power on if not already so.
      gprs.powerOn();
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
    gprs.checkNetwork();
    break;

    case 'r':
    //Serial.println(gprs.setAPN(apn, sizeof(apn)-1));  // dont send null termination
    Serial.println(gprs.setAPN());
    break;

    case 'R':
    Serial.println(gprs.isRegistered());
    break;

    case 'g':
    Serial.println(gprs.startGPRS());
    break;

    case 'i':
    Serial.println(gprs.getIP());
    break;

    case 'O':
    Serial.println(gprs.isOpenUDP());
    break;

    case'u':
    gprs.setPIN(); // dont send null termination
    break;

    case 'o':
    gprs.openUDP(); // don't send null termination
    break;

    case 'e':
    gprs.closeUDP();
    break;

    case 'E':
    Serial.println(gprs.shutGPRS());
    break;
    
    case 'v':
    gprs.verbose(0);
    break;

    case 'V':
    gprs.verbose(2);
    break;

    case 'S':
    gprs.sendUDP(UDP_SEND_STR, sizeof(UDP_SEND_STR));
    break;
    
    case 's':
    gprs.queueToSend((uint8_t *)UDP_SEND_STR, sizeof(UDP_SEND_STR));
    break;

    default:
    break;
  }
}
