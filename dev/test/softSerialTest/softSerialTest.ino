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
*/

#include <OTV0p2Base.h>

OTV0P2BASE::OTSoftSerial ser(8, 5);
//SoftwareSerial ser(7,8);
static const char sendStr1[] = "AT\n";
static const uint8_t pwr_pin = 6;

static const uint16_t baud = 19200;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(baud);
  ser.begin();  // defaults to 2400. This is what sim900 is currently set to.
  Serial.println("Start Program");
}

void loop() {
  // put your main code here, to run repeatedly:
  
  if(Serial.available() > 0) {
    char input = Serial.read();
    Serial.println(input);
    if(input == 'a') {
      Serial.println("Go");
      uint8_t i = 5;
      uint8_t val[6];
      memset(val, 0, sizeof(val));
      
      sendAT();

      Serial.print(ser.read(val, sizeof(val)-1));
      Serial.print("\t:");

      Serial.print((char *)val);
      Serial.print(" Raw: ");
      for(uint8_t i = 0; i < (sizeof(val)); i++) {
        Serial.print(val[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
}

void sendAT()
{
  ser.write(sendStr1, sizeof(sendStr1)-1);
}

void onOff()
{
  digitalWrite(pwr_pin, HIGH);
  delay(1000);
  digitalWrite(pwr_pin, LOW);
}

