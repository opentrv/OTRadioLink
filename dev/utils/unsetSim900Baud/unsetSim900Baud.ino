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
                           Mark Hill 2016
                           Damon Hart-Davis 2015--2016
*/

#include <SoftwareSerial.h>

/**
 * How to use:
 * *** Use Arduino. V0p2 does not have high enough clock speed to run this program reliably ***
 * Set initialBaud to current SIM900 baud, or to 19200 if on autobaud
 * 'l' lists supported bauds
 * 'r' displays current baud
 * 's' sets a new baud
 * 
 * This assumes a SIM900 already set to 2400,
 * with the aim of resetting them to autobaud.
 */

SoftwareSerial ser(7, 8);

static const char setBaudCommand[] = "AT+IPR";
static const uint16_t initialBaud = 2400;
static const uint16_t targetBaud  = 0; // 0 => autobaud

void setup() {
  // put your setup code here, to run once:
  Serial.begin(4800);
  ser.begin(initialBaud);

  // power up shield
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
  delay(500);
  digitalWrite(9, HIGH);
  delay(500);
  digitalWrite(9, LOW);

  delay(5000);
  getBaud();
  delay(500);
  setBaud();
  delay(500);
  ser.begin(2400);
  delay(500);
  getBaud();
  while (ser.available() > 0) ser.read();

  delay(500);
  loop();
}

void loop() {
  char c = 0;
  if (Serial.available() > 0) c = Serial.read();
  if (c == 'l') getPossibleBauds();
  else if (c == 'r') getBaud();
  else if (c == 's') setBaud();

  if (ser.available() > 0) Serial.print((char)ser.read());
}

void getPossibleBauds()
{
  Serial.println("\n++ List Supported Bauds ++");
  ser.write(setBaudCommand, sizeof(setBaudCommand) - 1);
  ser.println("=?");
}

void getBaud()
{
  Serial.println("\n++ Current Baud ++ - if correct you'll see <+IPR: 2400>");
  ser.write(setBaudCommand, sizeof(setBaudCommand) - 1);
  ser.println("?");
}

void setBaud()
{
  Serial.print("\n++ Setting Baud to ");
  Serial.print(targetBaud);
  Serial.println(" ++");

  ser.write(setBaudCommand, sizeof(setBaudCommand) - 1);
  ser.print("=");
  ser.println(targetBaud);

  ser.begin(targetBaud);

  //getRate();
}
