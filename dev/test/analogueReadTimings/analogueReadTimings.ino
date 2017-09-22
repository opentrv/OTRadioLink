
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

/**
 * @brief Tests length of OTV0p2Base_ADC routines
 *        Pin ?? is toggled at routine entry and exit
 *        The timing can be read with an oscilloscope
 * @note  Not testing _analogueNoiseReducedM as basically the same as analogueNoiseReduced
 *        
 *        'l' gives analogueNoiseReducedRead
 *        'c' gives analogueVsBandgapRead
 *        'n' gives noisyADCRead
 */
#include <OTV0p2Base.h>

const uint8_t testPin = 7;
const uint8_t aIn = 0; // LDR sensor

void setup() {
  pinMode(testPin, OUTPUT);

  Serial.begin(4800);
  Serial.println("Start");
}

void loop() {
//  uint8_t timer = 10;
  
  fastDigitalWrite(testPin,1);
  // uncomment function you want to test
    OTV0P2BASE::analogueNoiseReducedRead(aIn, INTERNAL); // use internal reference
//    OTV0P2BASE::analogueVsBandgapRead(aIn, false);
//    OTV0P2BASE::noisyADCRead(false);
  fastDigitalWrite(testPin,0);

  delay(1);
}
