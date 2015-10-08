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

#ifndef OTSIM900LINK_H_
#define OTSIM900LINK_H_

#include <Arduino.h>
#include <OTRadioLink.h>
#include <SoftwareSerial.h>
#include <string.h>
#include <stdint.h>

#ifndef OTSIM900LINK_DEBUG
#define OTSIM900LINK_DEBUG
#endif // OTSIM900LINK_DEBUG

/**
 * @note	To enable serial debug define 'OTSIM900LINK_DEBUG'
 * @todo	SIM900 has a low power state which stays connected to network
 * 			- Not sure how much power reduced
 * 			- If not sending often may be more efficient to power up and wait for connect each time
 * 			Make OTSIM900LinkBase to abstract serial interface?
 * 			Add methods to set APN, PIN and UDP send address
 * 			- These require variable length arrays and I'm not sure how to implement in class
 * 			Make read & write inline?
 * 			Will need to change error checking based on CME state
 */

//template<uint8_t rxPin, uint8_t txPin>	//FIXME	gave up on templating as breaks functions in cpp file
class OTSIM900Link : public OTRadioLink::OTRadioLink
{
public:
  OTSIM900Link(uint8_t pwrPin, uint8_t rxPin, uint8_t txPin);

/************************* Public Methods *****************************/
    bool begin();
    bool end();

    bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal);

    inline bool isAvailable(){ return bAvailable; };	 // checks radio is there independant of power state
  // set max frame bytes
    //void setMaxTypicalFrameBytes(uint8_t maxTypicalFrameBytes);
    //poll()
    // bool queueToSend(const uint8_t *buf, const uint8_t len, txPower = 0);

//private:
  //SoftwareSerial *softSerial;
   SoftwareSerial softSerial;

 /***************** AT Commands and Private Constants and variables ******************/
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
      static const char AT_SHUT_GPRS[8];
      static const char AT_GET_IP[6];
      static const char AT_PIN[5];
      static const char AT_START_UDP[9];
      static const char AT_SEND_UDP[8];
      static const char AT_CLOSE_UDP[9];
      static const char AT_VERBOSE_ERRORS[5];
      
      static const char AT_GET_MODULE = 'I';
      static const char AT_SET = '=';
      static const char AT_QUERY = '?';
  	  static const char AT_END = '\r';

    // Standard Responses

  // pins for software serial
  const uint8_t PWR_PIN;

  // variables
  /// @todo	How do I set these? Want them set outside class but may be variable length
  // char APN[];
  // char PIN[];
  bool bAvailable;
  bool bPowered;

/************************* Private Methods *******************************/
  	// Power up/down
  /**
   * @brief	check if module has power
   * @todo	is this needed?
   * @retval	true if module is powered up
   */
    inline bool isPowered() { return bPowered; };

    /**
     * @brief 	Power up module
     */
    inline void powerOn()
    {
      digitalWrite(PWR_PIN, LOW);
      if(!isPowered()) powerToggle();
    }

    /**
     * @brief 	Close UDP if necessary and power down module.
     */
    inline void powerOff()
    {
      digitalWrite(PWR_PIN, LOW);
      if(isPowered()) powerToggle();
    }

    /**
     * @brief	toggles power
     * @todo	replace digitalWrite with fastDigitalWrite
     * 			Does this need to be inline?
     */
    inline void powerToggle()
    {
    	delay(500);
    	digitalWrite(PWR_PIN, HIGH);
    	delay(1000);
    	digitalWrite(PWR_PIN, LOW);
    	bPowered = !bPowered;
    	delay(500);
    }

    // Serial functions
    uint8_t read();
    uint8_t serAvailable() {return softSerial.available();}	//FIXME delete?
    uint8_t timedBlockingRead(char *data, uint8_t length, char terminatingChar = 0);
    //uint8_t timedBlockingRead(char *data, uint8_t length);
    void write(const char *data, uint8_t length);
    void write(const char data);
    void print(const int value);

    // write AT commands
    bool checkModule(/*const char *name, uint8_t length*/);
    bool checkNetwork(char *buffer, uint8_t length);
    bool isRegistered();

    bool setAPN(const char *APN, uint8_t length);
    bool startGPRS();
    bool shutGPRS();
    uint8_t getIP(char *IPAddress);
    bool isOpenUDP();

    void verbose();
    void setPIN(const char *pin, uint8_t length);
    bool checkPIN();

    bool waitForTerm(uint8_t terminatingChar);

    const char *getResponse(uint8_t &newLength, const char *data, uint8_t dataLength, char _startChar);

    bool openUDP(const char *address, uint8_t addressLength, const char *port, uint8_t portLength);
    bool closeUDP();
    bool sendUDP(const char *frame, uint8_t length);
    //void setupUDP(const char *ipAddress, uint8_t ipAddressLength, const char *port, uint8_t portLength)

    bool getInitState();

public:	// define abstract methods here
    // These are unused as no RX
    virtual void _dolisten() {};
    /**
     * @todo	function to get maxTXMsgLen?
     */
    virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const {
    	queueRXMsgsMin = 0;
    	maxRXMsgLen = 0;
    	maxTXMsgLen = 64;
    };
    virtual uint8_t getRXMsgsQueued() const {return 0;}
    virtual const volatile uint8_t *peekRXMsg(uint8_t &len) const {len = 0; return 0;}
    virtual void removeRXMsg() {}
    bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false) {return false;};

/* other methods (copied from OTRadioLink as is)
virtual bool _doconfig() { return(true); }		// could this replace something?
virtual void preinit(const void *preconfig) {}	// not really relevant?
virtual void panicShutdown() { preinit(NULL); }	// see above
*/
};

#endif /* OTSIM900LINK_H_ */
