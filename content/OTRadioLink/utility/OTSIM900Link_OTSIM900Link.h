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
#include <OTV0p2Base.h>
#include <string.h>
#include <stdint.h>

#define OTSIM900LINK_DEBUG

/**
 * @note	To use library:
 * 			- create \0 terminated array containing pin, apn, and udp data
 * 			- create OTSIM900LinkConfig_t, initing pointing to above arrays
 * 			- create OTRadioLinkConfig_t with a pointer to above struct
 * 			- create OTSIM900Link
 * 			- pass pointer to radiolink structure to OTSIM900Link::configure()
 * 			- begin starts radio and sets up PGP instance, before returning to GPRS off mode
 * 			- queueToSend starts GPRS, opens UDP, sends message then deactivates GPRS. Process takes 5-10 seconds
 * 			- poll goes in interrupt and sets boolean when message sent(?) //FIXME not yet implemented
 * @todo	implement eeprom string copy helper (possibly checking if in ram or in eeprom
 *			Assign last ~256 bytes of eeprom to radio config struct
 *
 */

namespace OTSIM900Link
{

/**
 * @struct	OTSIM900LinkConfig_t
 * @brief	Structure containing config data for OTSIM900Link
 * @note	Struct and internal pointers must last as long as OTSIM900Link object
 * @param	PIN		Pointer to \0 terminated array containing SIM pin code
 * @param	APN		Pointer to \0 terminated array containing access point name
 * @param	UDP_Address	Pointer to \0 terminated array containing UDP address to send to
 * @param	UDP_Port	Pointer to \0 terminated array containing  UDP port
 */
typedef struct OTSIM900LinkConfig_t {
	// Is in eeprom?
	const bool bEEPROM;	// FIXME not yet implemented
	const char *PIN;
	const char *APN;
	const char *UDP_Address;
	const char *UDP_Port;
};


/**
 * @note	To enable serial debug define 'OTSIM900LINK_DEBUG'
 * @todo	SIM900 has a low power state which stays connected to network
 * 			- Not sure how much power reduced
 * 			- If not sending often may be more efficient to power up and wait for connect each time
 * 			Make OTSIM900LinkBase to abstract serial interface and allow templating?
 * 			Make read & write inline?
 */
class OTSIM900Link : public OTRadioLink::OTRadioLink
{
public:
  OTSIM900Link(const OTSIM900LinkConfig_t *_config, uint8_t pwrPin, uint8_t rxPin, uint8_t txPin);

/************************* Public Methods *****************************/
    bool begin();
    bool end();

    bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);
    bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal);

    inline bool isAvailable(){ return bAvailable; };	 // checks radio is there independant of power state
  // set max frame bytes
    //void setMaxTypicalFrameBytes(uint8_t maxTypicalFrameBytes);
    void poll();

#ifndef OTSIM900LINK_DEBUG
private:
#endif // OTSIM900LINK_DEBUG
   //SoftwareSerial softSerial;
   OTV0P2BASE::OTSoftSerial softSerial;

 /***************** AT Commands and Private Constants and variables ******************/
    // set AT commands here
    // These may not be supported by all sim modules so may need to move
    // to concrete implementation
  	  static const char AT_START[3];
      static const char AT_NETWORK[6];
      static const char AT_REGISTRATION[6];
      static const char AT_GPRS_REGISTRATION0[7];
      static const char AT_GPRS_REGISTRATION[7];
      static const char AT_SET_APN[6];
      static const char AT_START_GPRS[7];
      static const char AT_SHUT_GPRS[9];
      static const char AT_GET_IP[7];
      static const char AT_PIN[6];
      static const char AT_STATUS[11];
      static const char AT_START_UDP[10];
      static const char AT_SEND_UDP[9];
      static const char AT_CLOSE_UDP[10];
      static const char AT_VERBOSE_ERRORS[6];

      static const char AT_GET_MODULE = 'I';
      static const char AT_SET = '=';
      static const char AT_QUERY = '?';
  	  static const char AT_END = '\r';

    // Standard Responses

  // pins for software serial
  const uint8_t PWR_PIN;

  // variables
  bool bAvailable;
  bool bPowered;
  const OTSIM900LinkConfig_t *config;
  const uint16_t baud = 2400; // max reliable baud
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
    	delay(1500);
    }

    // Serial functions
    uint8_t read();
    //uint8_t serAvailable() {return softSerial.available();}	//FIXME delete?
    //void flush();
    uint8_t timedBlockingRead(char *data, uint8_t length);
    //uint8_t timedBlockingRead(char *data, uint8_t length);
    void write(const char *data, uint8_t length);
    void print(const char data);
    void print(const uint8_t value);
    void print(const char *string);

    // write AT commands
    bool checkModule();
    bool checkNetwork();
    bool isRegistered();

    bool setAPN();
    bool startGPRS();
    bool shutGPRS();
    uint8_t getIP();
    bool isOpenUDP();

    void verbose(uint8_t level);
    void setPIN();
    bool checkPIN();

    bool flushUntil(uint8_t terminatingChar);

    const char *getResponse(uint8_t &newLength, const char *data, uint8_t dataLength, char _startChar);

    bool openUDP();
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

/* other methods (copied from OTRadioLink as is)
virtual bool _doconfig() { return(true); }		// could this replace something?
virtual void preinit(const void *preconfig) {}	// not really relevant?
virtual void panicShutdown() { preinit(NULL); }	// see above
*/
};

}	// namespace OTSIM900Link

#endif /* OTSIM900LINK_H_ */
