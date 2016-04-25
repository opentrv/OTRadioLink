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
#include <util/atomic.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <OTRadioLink.h>
#include <OTV0p2Base.h>
#include <string.h>
#include <stdint.h>

#define OTSIM900LINK_DEBUG

/**
 * @note    To use library:
 *             - create \0 terminated array containing pin, apn, and udp data
 *             - create OTSIM900LinkConfig_t, initing pointing to above arrays
 *             - create OTRadioLinkConfig_t with a pointer to above struct
 *             - create OTSIM900Link
 *             - pass pointer to radiolink structure to OTSIM900Link::configure()
 *             - begin starts radio and sets up PGP instance, before returning to GPRS off mode
 *             - queueToSend starts GPRS, opens UDP, sends message then deactivates GPRS. Process takes 5-10 seconds
 *             - poll goes in interrupt and sets boolean when message sent(?) //FIXME not yet implemented
 */

namespace OTSIM900Link
{


/**
 * @struct    OTSIM900LinkConfig_t
 * @brief    Structure containing config data for OTSIM900Link
 * @todo    This is a bit weird - take pointer from struct and pass to helper function in struct
 * @note    Struct and internal pointers must last as long as OTSIM900Link object
 * @param    bEEPROM    true if strings stored in EEPROM, else held in FLASH
 * @param    PIN        Pointer to \0 terminated array containing SIM pin code
 * @param    APN        Pointer to \0 terminated array containing access point name
 * @param    UDP_Address    Pointer to \0 terminated array containing UDP address to send to as IPv4 dotted quad
 * @param    UDP_Port    Pointer to \0 terminated array containing UDP port in decimal
 */
// If stored in SRAM
typedef struct OTSIM900LinkConfig {
//private:
    // Is in eeprom?
    const bool bEEPROM;
    const void * const PIN;
    const void * const APN;
    const void * const UDP_Address;
    const void * const UDP_Port;

    OTSIM900LinkConfig(bool e, const void *p, const void *a, const void *ua, const void *up)
      : bEEPROM(e), PIN(p), APN(a), UDP_Address(ua), UDP_Port(up) { }
//public:
    /**
     * @brief    Copies radio config data from EEPROM to an array
     * @todo    memory bound check?
     *             Is it worth indexing from beginning of mem location and adding offset?
     *             check count returns correct number
     *             How to implement check if in eeprom?
     * @param    buf        pointer to destination buffer
     * @param    field    memory location
     * @retval    length of data copied to buffer
     */
    char get(const uint8_t *src) const{
        char c = 0;
        switch (bEEPROM) {
        case true:
            c = eeprom_read_byte(src);
            break;
        case false:
            c = pgm_read_byte(src);
            break;
        }
        return c;
    }
} OTSIM900LinkConfig_t;

enum SendState {
        IDLE,
        START_GPRS,
        WAIT_FOR_UDP,
        RESTART_CONNECTION,
    };


/**
 * @note    To enable serial debug define 'OTSIM900LINK_DEBUG'
 * @todo    SIM900 has a low power state which stays connected to network
 *             - Not sure how much power reduced
 *             - If not sending often may be more efficient to power up and wait for connect each time
 *             Make OTSIM900LinkBase to abstract serial interface and allow templating?
 *             Make read & write inline?
 */
class OTSIM900Link : public OTRadioLink::OTRadioLink
{
  // Maximum number of significant chars in the SIM900 response.
  // Minimising this reduces stack and/or global space pressures.
  static const uint8_t MAX_SIM900_RESPONSE_CHARS = 64;

public:
  OTSIM900Link(uint8_t pwrSwitchPin, uint8_t pwrPin, uint8_t rxPin, uint8_t txPin);

/************************* Public Methods *****************************/
    bool begin();
    bool end();

    bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);
    bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal);

    inline bool isAvailable(){ return bAvailable; };     // checks radio is there independant of power state
  // set max frame bytes
    //void setMaxTypicalFrameBytes(uint8_t maxTypicalFrameBytes);
    void poll();

    /**
     * @brief    This will be called in interrupt while waiting for send prompt
     * @retval    returns true on successful exit
     */
    bool handleInterruptSimple();

#ifndef OTSIM900LINK_DEBUG
private:
#endif // OTSIM900LINK_DEBUG

 /***************** AT Commands and Private Constants and variables ******************/
    // set AT commands here
    // These may not be supported by all sim modules so may need to move
    // to concrete implementation
        static const char AT_START[3];
        static const char AT_SIGNAL[5];
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
  const uint8_t HARD_PWR_PIN;
  //SoftwareSerial softSerial;
  OTV0P2BASE::OTSoftSerial2 softSerial;

  // variables
  bool bAvailable;
  bool bPowered;
  volatile uint8_t txMessageQueue; // Number of frames currently queued for TX.
  const OTSIM900LinkConfig_t *config;
  static const uint16_t baud = 2400; // max reliable baud
  static const uint8_t flushTimeOut = 10;
/************************* Private Methods *******************************/
      // Power up/down
  /**
   * @brief    check if module has power
   * @todo    is this needed?
   * @retval    true if module is powered up
   */
    inline bool isPowered() { return bPowered; };

    /**
     * @brief     Power up module
     */
    inline void powerOn()
    {
      digitalWrite(PWR_PIN, LOW);
      if(!isPowered()) powerToggle();
    }

    /**
     * @brief     Close UDP if necessary and power down module.
     */
    inline void powerOff()
    {
      digitalWrite(PWR_PIN, LOW);
      if(isPowered()) powerToggle();
    }

    /**
     * @brief    toggles power
     * @todo    replace digitalWrite with fastDigitalWrite
     *             Does this need to be inline?
     */
    inline void powerToggle()
    {
        delay(500);
        digitalWrite(PWR_PIN, HIGH);
        delay(1000);
        digitalWrite(PWR_PIN, LOW);
        bPowered = !bPowered;
        delay(3000);
    }

    // Serial functions
    uint8_t read();
    uint8_t timedBlockingRead(char *data, uint8_t length);
    void write(const char *data, uint8_t length);
    void print(const char data);
    void print(const uint8_t value);
    void print(const char *string);
    void print(const void *src);

    // write AT commands
    bool checkModule();
    bool checkNetwork();
    bool isRegistered();

    bool setAPN();
    bool startGPRS();
    bool shutGPRS();
    uint8_t getIP();
    uint8_t isOpenUDP();
    void getSignalStrength();

    void verbose(uint8_t level);
    void setPIN();
    bool checkPIN();

    bool flushUntil(uint8_t terminatingChar);

    const char *getResponse(uint8_t &newLength, const char *data, uint8_t dataLength, char _startChar);

    bool openUDP();
    bool closeUDP();
    bool sendUDP(const char *frame, uint8_t length);

    bool getInitState();

    bool _doconfig();

    bool printDiagnostics();

    // TODO check this is in correct place
    volatile SendState state;
    // TODO expand this so that it can take multiple messages
    uint8_t txQueue[64]; // 64 is maxTxMsgLen (from OTRadioLink)
    uint8_t txMsgLen;  // This stores the length of the tx message. will have to be redone for multiple txQueue
    static const uint8_t maxTxQueueLength = 1; // TODO Could this be moved out into OTRadioLink


public:    // define abstract methods here
    // These are unused as no RX
    virtual void _dolisten() {}
    /**
     * @todo    function to get maxTXMsgLen?
     */
    virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const {
        queueRXMsgsMin = 0;
        maxRXMsgLen = 0;
        maxTXMsgLen = 64;
    };
    virtual uint8_t getRXMsgsQueued() const {return 0;}
    virtual const volatile uint8_t *peekRXMsg() const {return 0;}
    virtual void removeRXMsg() {}


/* other methods (copied from OTRadioLink as is)
virtual bool _doconfig() { return(true); }        // could this replace something?
virtual void preinit(const void *preconfig) {}    // not really relevant?
virtual void panicShutdown() { preinit(NULL); }    // see above
*/
};

}    // namespace OTSIM900Link

#endif /* OTSIM900LINK_H_ */
