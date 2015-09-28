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

#ifndef SIM900LINK_H_
#define SIM900LINK_H_

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <string.h>
#include <stdint.h>

class OTSIM900Link
{
public:
  OTSIM900Link(uint8_t pwrPin, SoftwareSerial *_softSerial);

/************************* Public Methods *****************************/
    bool begin(uint8_t baud);
    bool end();

    bool openUDP(const char *address, uint8_t addressLength, const char *port, uint8_t portLength);
    bool closeUDP();
    bool sendUDP(const char *frame, uint8_t length);
    //void setupUDP(const char *ipAddress, uint8_t ipAddressLength, const char *port, uint8_t portLength)

  // set max frame bytes
    //void setMaxTypicalFrameBytes(uint8_t maxTypicalFrameBytes);
    //bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false) {};

//private:
  SoftwareSerial *softSerial;
 /***************** AT Commands and Private Constants ******************/
    // set AT commands here
    // These may not be supported by all sim modules so may need to move
    // to concrete implementation
  	  static const char AT_START[2];
      static const char AT_NETWORK[5];
      static const char AT_REGISTRATION[5];
      static const char AT_GPRS_REGISTRATION0[6];
      static const char AT_GPRS_REGISTRATION[6];
      static const char AT_SET_APN[5];
      static const char AT_START_GPRS[6];
      static const char AT_GET_IP[6];
      static const char AT_VERBOSE_ERRORS[5];
      static const char AT_PIN[5];
      static const char AT_START_UDP[9];
      static const char AT_SEND_UDP[8];
      static const char AT_CLOSE_UDP[9];
      
      static const char AT_GET_MODULE = 'I';
      static const char AT_SET = '=';
      static const char AT_QUERY = '?';
  	  static const char AT_END = '\r';

    // Standard Responses


  // put module name here

  // pins for software serial
  const uint8_t PWR_PIN;

  /************************* Private Methods *******************************/
    // Power up/down
  bool isPowered();
    /**
     * @brief power up module
     * @note  check if already powered
     */
    inline void powerOn()
    {
      digitalWrite(PWR_PIN, LOW);
      if(!isPowered()) {
        delay(500);
        digitalWrite(PWR_PIN, HIGH);
        delay(1000);
        digitalWrite(PWR_PIN, LOW);
      }
    }
    /**
     * @brief power down module
     * @note  check if powered and close UDP if required
     */
    inline void powerOff()
    {
      digitalWrite(PWR_PIN, LOW);
      if(isPowered()) {
        delay(500);
        digitalWrite(PWR_PIN, HIGH);
        delay(1000);
        digitalWrite(PWR_PIN, LOW);
      }
    }

    // Serial functions
    uint8_t timedBlockingRead(char *data, uint8_t length);
    void write(const char *data, uint8_t length);
    void write(const char data);
    void print(const int value);

    // write AT commands
    bool checkModule(/*const char *name, uint8_t length*/);
    bool checkNetwork(char *buffer, uint8_t length);
    bool isRegistered();

    void setAPN(const char *APN, uint8_t length);
    bool startGPRS();
    uint8_t getIP(char *IPAddress);
    bool isOpenUDP();

    void verbose();
    void setPIN(const char *pin, uint8_t length);
    bool checkPIN();

};

#endif /* SIM900LINK_H_ */
