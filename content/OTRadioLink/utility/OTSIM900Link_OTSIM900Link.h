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

Author(s) / Copyright (s): Deniz Erbilgin 2015-2016
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

// If DEFINED: Prints debug information to serial.
//             !!! WARNING! THIS WILL CAUSE BLOCKING OF OVER 300 MS!!!
#define OTSIM900LINK_DEBUG


// OTSIM900Link macros for printing debug information to serial.
#ifndef OTSIM900LINK_DEBUG
#define OTSIM900LINK_DEBUG_SERIAL_PRINT(s) // Do nothing.
#define OTSIM900LINK_DEBUG_SERIAL_PRINTFMT(s, format) // Do nothing.
#define OTSIM900LINK_DEBUG_SERIAL_PRINT_FLASHSTRING(fs) // Do nothing.
#define OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) // Do nothing.
#define OTSIM900LINK_DEBUG_SERIAL_PRINTLN(s) // Do nothing.
#else
// Send simple string or numeric to serial port and wait for it to have been sent.
// Make sure that Serial.begin() has been invoked, etc.
#define OTSIM900LINK_DEBUG_SERIAL_PRINT(s) { OTV0P2BASE::serialPrintAndFlush(s); }
#define OTSIM900LINK_DEBUG_SERIAL_PRINTFMT(s, fmt) { OTV0P2BASE::serialPrintAndFlush((s), (fmt)); }
#define OTSIM900LINK_DEBUG_SERIAL_PRINT_FLASHSTRING(fs) { OTV0P2BASE::serialPrintAndFlush(F(fs)); }
#define OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) { OTV0P2BASE::serialPrintlnAndFlush(F(fs)); }
#define OTSIM900LINK_DEBUG_SERIAL_PRINTLN(s) { OTV0P2BASE::serialPrintlnAndFlush(s); }
#endif // OTSIM900LINK_DEBUG



/**
 * @note    To use library:
 *             - create \0 terminated array containing pin, apn, and udp data
 *             - create OTSIM900LinkConfig_t, initing pointing to above arrays
 *             - create OTRadioLinkConfig_t with a pointer to above struct
 *             - create OTSIM900Link
 *             - pass pointer to radiolink structure to OTSIM900Link::configure()
 *             - begin starts radio and sets up PGP instance, before returning to GPRS off mode
 *             - queueToSend starts GPRS, opens UDP, sends message then deactivates GPRS. Process takes 5-10 seconds
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

/**
 * @brief   Enum containing states of SIM900.
 */
enum OTSIM900LinkState {
        GET_STATE,
        RETRY_GET_STATE,
        START_UP,
        CHECK_PIN,
        WAIT_FOR_REGISTRATION,
        SET_APN,
        START_GPRS,
        GET_IP,
        OPEN_UDP,
        IDLE,
        WAIT_FOR_UDP,
        SENDING,
        PANIC
    };


/**
 * @note    To enable serial debug define 'OTSIM900LINK_DEBUG'
 * @todo    SIM900 has a low power state which stays connected to network
 *             - Not sure how much power reduced
 *             - If not sending often may be more efficient to power up and wait for connect each time
 *             Make OTSIM900LinkBase to abstract serial interface and allow templating?
 */
template<uint8_t rxPin, uint8_t txPin>
class OTSIM900Link : public OTRadioLink::OTRadioLink
{
  // Maximum number of significant chars in the SIM900 response.
  // Minimising this reduces stack and/or global space pressures.
  static const uint8_t MAX_SIM900_RESPONSE_CHARS = 64;

public:
	/**
	* @brief    Constructor. Initializes softSerial and sets PWR_PIN
	* @param    pwrPin        SIM900 power on/off pin
	* @param    rxPin        Rx pin for software serial
	* @param    txPin        Tx pin for software serial
	*
	* Cannot do anything with side-effects,
	* as may be called before run-time fully initialised!
	*/
    OTSIM900Link(uint8_t hardPwrPin, uint8_t pwrPin) : HARD_PWR_PIN(hardPwrPin), PWR_PIN(pwrPin)
    {
        bAvailable = false;
        bPowered = false;
        bPowerLock = false;
        powerTimer = 0;
        config = NULL;
        state = IDLE;
        memset(txQueue, 0, sizeof(txQueue));
        txMsgLen = 0;
        txMessageQueue = 0;
    }

/************************* Public Methods *****************************/
    /**
     * @brief    Starts software serial, checks for module and inits state machine.
     */
    bool begin()
    {
        pinMode(PWR_PIN, OUTPUT);
        fastDigitalWrite(PWR_PIN, LOW);
        ser.begin(baud);
        state = GET_STATE;
        return true;
    }
    /**
     * @brief    close UDP connection and power down SIM module
     */
    bool end()
    {
        closeUDP();
    //    powerOff(); TODO fix this
        return false;
    }

    /**
     * @brief   Sends message.
     * @param   buf     pointer to buffer to send
     * @param   buflen  length of buffer to send
     * @param   channel ignored
     * @param   Txpower ignored
     * @retval  returns true if send process inited.
     * @note    requires calling of poll() to check if message sent successfully
     */
    bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false)
    {
        bool bSent = false;
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Send Raw")
        bSent = sendUDP((const char *)buf, buflen);
        return bSent;
    }
    /**
     * @brief   Puts message in queue to send on wakeup.
     * @param   buf     pointer to buffer to send.
     * @param   buflen  length of buffer to send.
     * @param   channel ignored.
     * @param   Txpower ignored.
     * @retval  returns true if send process inited.
     * @note    requires calling of poll() to check if message sent successfully.
     */
    bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t , TXpower )
    {
        if ((buf == NULL) || (buflen > sizeof(txQueue)) || (txMessageQueue >= maxTxQueueLength)) return false;    // TODO check logic and sort out maxTXMsgLen problem
        // Increment message queue
        txMessageQueue++;
        // copy into queue here?
        memcpy(txQueue, buf, buflen);
        txMsgLen = buflen;
        return true;
    }
    inline bool isAvailable(){ return bAvailable; };     // checks radio is there independent of power state
    /**
     * @brief   Polling routine steps through 4 stage state machine
     */
    void poll()
    {
        if (bPowerLock == false) {
            if (OTV0P2BASE::getSubCycleTime() < 10) {
                switch (state) {
                case GET_STATE:  // Check SIM900 is present and can be talked to. Takes up to 220 ticks?
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*GET_STATE")
                    memset(txQueue, 0, sizeof(txQueue));
                    txMsgLen = 0;
                    txMessageQueue = 0;
                    bAvailable = false;
                    bPowered = false;
                    if(!interrogateSIM900()) {
                        bAvailable = true;
                        bPowered = true;
                        state = START_UP;
                    } else {
                        state = RETRY_GET_STATE;
                    }
                    powerToggle(); // Power down for START_UP/toggle for RETRY_GET_STATE.
                    break;
                case RETRY_GET_STATE:
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*RETRY_GET_STATE")
                    if(!interrogateSIM900()) {
                        bAvailable = true;
                        bPowered = true;
                        state = START_UP;
                    } else state = PANIC;
                    powerToggle(); // Power down for START_UP
                    break;
                case START_UP: // takes up to 150 ticks
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*START_UP")
                    powerOn();
                    state = CHECK_PIN;
    //                state = WAIT_FOR_REGISTRATION; // FIXME
                    break;
                case CHECK_PIN:  // Set pin if required. Takes ~100 ticks to exit.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*CHECK_PIN")
                    if(checkPIN()) state = WAIT_FOR_REGISTRATION;
    //                if(setPIN()) state = PANIC;// TODO make sure setPin returns true or false
                    break;
                case WAIT_FOR_REGISTRATION:  // Wait for registration to GSM network. Stuck in this state until success. Takes ~150 ticks to exit.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*WAIT_FOR_REG")
                    if(isRegistered()) state = SET_APN;
                    break;
                case SET_APN:  // Attempt to set the APN. Stuck in this state until success. Takes up to 200 ticks to exit.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*SET_APN")
                    if(!setAPN()) state = START_GPRS; // TODO: Watchdog resets here
                    break;
                case START_GPRS:  // Start GPRS context.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN("*START_GPRS")
                    if(isOpenUDP() != 3) startGPRS();
                    else state = GET_IP;
    //                if(!startGPRS()) state = GET_IP;  // TODO: Add retries, Option to shut GPRS here (probably needs a new state)
    //                else state = PANIC; // If startGPRS() ever returns an error here then something has probably gone very wrong
                    // FIXME 20160505: Need to work out how to handle this. If signal is marginal this will fail.
                    break;
                case GET_IP: // Takes up to 200 ticks to exit.
                    // For some reason, AT+CIFSR must done to be able to do any networking.
                    // It is the way recommended in SIM900_Appication_Note.pdf section 3: Single Connections.
                    // This was not necessary when opening and shutting GPRS as in OTSIM900Link v1.0
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*GET IP")
//                    if(getIP()) state = OPEN_UDP;
                    getIP();
                    state = OPEN_UDP;
                    break;
                case OPEN_UDP:  // Open a udp socket. Takes ~200 ticks to exit.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*OPEN UDP")
                    if(openUDP()) state = IDLE;
                    break;
                case IDLE:  // Waiting for outbound message.
                    if (txMessageQueue > 0) {    // If message is queued, go to WAIT_FOR_UDP
                        state = WAIT_FOR_UDP; // TODO-748
                    }
                    break;
                case WAIT_FOR_UDP:  // Make sure UDP context is open. Takes up to 200 ticks to exit.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*WAIT_FOR_UDP")
                {
                    uint8_t udpState = isOpenUDP();
                    if(udpState == 1) state = SENDING;
    //                else if (udpState == 0) state = GET_STATE; // START_GPRS; // TODO needed for optional wake GPRS to send. FIXME normally commented, set to get_state for testing reset.
                    else if (udpState == 2) state = GET_STATE;
                }
                break;
                case SENDING:  // Attempt to send a message. Takes ~100 ticks to exit.
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*SENDING")
                    if (txMessageQueue > 0) {  // Check to make sure it is near the start of the subcycle to avoid overrunning.
                        // TODO logic to check if send attempt successful
                        sendRaw(txQueue, txMsgLen); /// @note can't use strlen with encrypted/binary packets
                        if(!(--txMessageQueue)) state = IDLE;  // // Once done, decrement number of messages in queue and return to IDLE
                    } else if (txMessageQueue == 0) state = IDLE;
                    break;
                case PANIC:
                    OTV0P2BASE::serialPrintlnAndFlush(F("SIM900_PANIC!"));
                    break;
                default:
                    break;
                }
            }
        } else if (OTV0P2BASE::getElapsedSecondsLT(powerTimer) > duration) {  // Check if ready to stop waiting after power toggled.
            bPowerLock = false;
        }
    }

    /**
     * @brief    This will be called in interrupt while waiting for send prompt
     * @retval    returns true on successful exit
     */
    inline bool handleInterruptSimple()
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ser.handle_interrupt();
        uint8_t c = ser.peek();
        OTSIM900LINK_DEBUG_SERIAL_PRINTFMT(c, HEX)
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN();
        return true;
        }
    }

#ifndef OTSIM900LINK_DEBUG // This is included to ease unit testing.
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
//    static const char AT_END = '\r';

    // Standard Responses

  // pins for software serial
  const uint8_t HARD_PWR_PIN;
  const uint8_t PWR_PIN;
  //SoftwareSerial softSerial;
  OTV0P2BASE::OTSoftSerial3<rxPin, txPin> ser;

  // variables
  bool bAvailable;
  bool bPowered;
  bool bPowerLock;
  int8_t powerTimer;
  static const uint8_t duration = 3;
  volatile uint8_t txMessageQueue; // Number of frames currently queued for TX.
  const OTSIM900LinkConfig_t *config;
  static const uint16_t baud = 9600; // max reliable baud
  static const uint8_t flushTimeOut = 10;
/************************* Private Methods *******************************/
      // Power up/down
  /**
   * @brief    check if module has power
   * @retval    true if module is powered up
   */
    inline bool isPowered() { return bPowered; };

    /**
     * @brief     Power up module
     */
    inline void powerOn()
    {
      fastDigitalWrite(PWR_PIN, LOW);
      if(!isPowered()) powerToggle();
    }

    /**
     * @brief     Close UDP if necessary and power down module.
     */
    inline void powerOff()
    {
      fastDigitalWrite(PWR_PIN, LOW);
      if(isPowered()) powerToggle();
    }

    /**
     * @brief   toggles power and sets power lock.
     * @fixme   proper ovf testing not implemented so the SIM900 may not power on/off near the end of a 60 second cycle.
     */
    void powerToggle()
    {
        fastDigitalWrite(PWR_PIN, HIGH);
        delay(1000);  // This is the minimum value that worked reliably
        fastDigitalWrite(PWR_PIN, LOW);
        bPowered = !bPowered;
    //    delay(3000);
        bPowerLock = true;
        powerTimer = OTV0P2BASE::getSecondsLT();
    }

    // Serial functions
    /**
     * @brief   Enter blocking read. Fills buffer or times out after 100 ms.
     * @param   data:   Data buffer to write to.
     * @param   length: Length of data buffer.
     * @retval  Number of characters received before time out.
     */
    uint8_t timedBlockingRead(char *data, uint8_t length)
    {
        // clear buffer, get time and init i to 0
        uint8_t counter = 0;
        uint8_t len = length;
        char *pdata = data;
        memset(data, 0, length);
        while(len--) {
            char c = ser.read();
            if(c == -1) break;
            *pdata++ = c;
            counter++;
        }
    #if 0
      OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("\n--Buffer Length: ")
      OTSIM900LINK_DEBUG_SERIAL_PRINTLN(i)
    #endif
      return counter;
    }
    /**
     * @brief   Utility function for printing from config structure.
     * @param   src:    Source to print from. Should be passed as a config-> pointer.
     */
    void printConfig(const void * src)
    {
        char c = 0xff;    // to avoid exiting the while loop without \0 getting written
        const uint8_t *ptr = (const uint8_t *) src;
        // loop through and print each value
        while (1) {
            c = config->get(ptr);
            if (c == '\0') return;
            ser.print(c);
            ptr++;
        }
    }

    // write AT commands
    /**
     * @brief   Checks module ID.
     * @todo    Implement check?
     * @param   name:   pointer to array to compare name with.
     * @param   length: length of array name.
     * @retval  True if ID recovered successfully.
     */
    bool checkModule()
    {
    char data[min(32, MAX_SIM900_RESPONSE_CHARS)];
        ser.print(AT_START);
        ser.println(AT_GET_MODULE);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));
        OTSIM900LINK_DEBUG_SERIAL_PRINT(data)
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN()
        return true;
    }
    /**
     * @brief   Checks connected network.
     * @todo    implement check.
     * @param   buffer: pointer to array to store network name in.
     * @param   length: length of buffer.
     * @param   True if connected to network.
     */
    bool checkNetwork()
    {
        char data[MAX_SIM900_RESPONSE_CHARS];
        ser.print(AT_START);
        ser.print(AT_NETWORK);
        ser.println(AT_QUERY);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));
        return true;
    }
    /**
     * @brief   Check if module connected and registered (GSM and GPRS).
     * @retval  True if registered.
     */
    bool isRegistered()
    {
    //  Check the GSM registration via AT commands ( "AT+CREG?" returns "+CREG:x,1" or "+CREG:x,5"; where "x" is 0, 1 or 2).
    //  Check the GPRS registration via AT commands ("AT+CGATT?" returns "+CGATT:1" and "AT+CGREG?" returns "+CGREG:x,1" or "+CGREG:x,5"; where "x" is 0, 1 or 2).
        char data[MAX_SIM900_RESPONSE_CHARS];
        ser.print(AT_START);
        ser.print(AT_REGISTRATION);
        ser.println(AT_QUERY);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));
        // response stuff
        const char *dataCut;
        uint8_t dataCutLength = 0;
        dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
        if (dataCut[2] == '1' || dataCut[2] == '5' ) return true;    // expected response '1' or '5'
        else return false;
    }

    /**
     * @brief   Set Access Point Name and start task.
     * @retval  0 if APN set.
     * @retval  -1 if failed to set.
     */
    uint8_t setAPN()
    {
        char data[MAX_SIM900_RESPONSE_CHARS]; // FIXME: was 96: that's a LOT of stack!
        ser.print(AT_START);
        ser.print(AT_SET_APN);
        ser.print(AT_SET);
        printConfig(config->APN);
        ser.println();
        timedBlockingRead(data, sizeof(data));
        // response stuff
        const char *dataCut;
        uint8_t dataCutLength = 0;
        dataCut = getResponse(dataCutLength, data, sizeof(data), 0x0A);
    //    OTSIM900LINK_DEBUG_SERIAL_PRINTLN(dataCut)
        if (dataCut[2] == 'O') return 0;    // expected response 'OK'
        else return -1;
    }
    /**
     * @brief   Start GPRS connection.
     * @retval  0 if connected.
     *          -1 if failed.
     * @note    check power, check registered, check gprs active.
     */
    uint8_t startGPRS()
    {
        char data[min(16, MAX_SIM900_RESPONSE_CHARS)];
        ser.print(AT_START);
        ser.println(AT_START_GPRS);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));

        // response stuff
        const char *dataCut;
        uint8_t dataCutLength = 0;
        dataCut = getResponse(dataCutLength, data, sizeof(data), 0x0A);    // unreliable
        if ((dataCut[0] == 'O') & (dataCut[1] == 'K')) return 0;
        else return -1;
    }
    /**
     * @brief   Shut GPRS connection.
     * @retval  0 if shut.
     * @retval  -1 if failed to shut.
     */
    uint8_t shutGPRS()
    {
        char data[MAX_SIM900_RESPONSE_CHARS]; // Was 96: that's a LOT of stack!
        ser.print(AT_START);
        ser.println(AT_SHUT_GPRS);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));

        // response stuff
        const char *dataCut;
        uint8_t dataCutLength = 0;
        dataCut= getResponse(dataCutLength, data, sizeof(data), 0x0A);
        if (*dataCut == 'S') return 0;    // expected response 'SHUT OK'
        else return -1;
    }
    /**
     * @brief   Get IP address
     * @todo    How should I return the string?
     * @retval  return length of IP address. Return 0 if no connection
     */
    uint8_t getIP()
    {
      char data[MAX_SIM900_RESPONSE_CHARS];
      ser.print(AT_START);
      ser.println(AT_GET_IP);
    //  ser.print(AT_END);
      timedBlockingRead(data, sizeof(data));
      // response stuff
      const char *dataCut;
      uint8_t dataCutLength = 0;
      dataCut= getResponse(dataCutLength, data, sizeof(data), 0x0A);

      if(*dataCut == '+') return 0; // all error messages will start with a '+'
      else {
          return dataCutLength;
      }
    }
    /**
     * @brief   Check if UDP open.
     * @retval  0 if GPRS closed.
     * @retval  1 if UDP socket open.
     * @retval  2 if in dead end state.
     */
    uint8_t isOpenUDP()
    {
        char data[MAX_SIM900_RESPONSE_CHARS];
        ser.print(AT_START);
        ser.println(AT_STATUS);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));

        // response stuff
        const char *dataCut;
        uint8_t dataCutLength = 0;
        dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
        if (*dataCut == 'C') return 1; // expected string is 'CONNECT OK'. no other possible string begins with C
        else if (*dataCut == 'P') return 2;
        else if (dataCut[3] == 'G') return 3;
        else return false;
    }
    /**
     * @brief   Get signal strength.
     * @todo    Return /print something?
     */
    void getSignalStrength()
    {
        char data[min(32, MAX_SIM900_RESPONSE_CHARS)];
        ser.print(AT_START);
        ser.println(AT_SIGNAL);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
        // response stuff
        uint8_t dataCutLength = 0;
        getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
    }

    /**
     * @brief   Set verbose errors
     * @param   level: 0 is no error codes, 1 is with codes, 2 is human readable descriptions.
     */
    void verbose(uint8_t level)
    {
        char data[MAX_SIM900_RESPONSE_CHARS];
        ser.print(AT_START);
        ser.print(AT_VERBOSE_ERRORS);
        ser.print(AT_SET);
        ser.println((char)(level + '0'));
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
    }
    /**
     * @brief   Enter PIN code
     * @todo    Check return value?
     */
    uint8_t setPIN()
    {
        char data[MAX_SIM900_RESPONSE_CHARS];
        ser.print(AT_START);
        ser.print(AT_PIN);
        ser.print(AT_SET);
        printConfig(config->PIN);
        ser.println();
        timedBlockingRead(data, sizeof(data));
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
        return 0;
    }
    /**
     * @brief   Check if PIN required
     */
    bool checkPIN()
    {
        char data[min(40, MAX_SIM900_RESPONSE_CHARS)];
        ser.print(AT_START);
        ser.print(AT_PIN);
        ser.println(AT_QUERY);
    //    ser.print(AT_END);
        timedBlockingRead(data, sizeof(data));

        // response stuff
        const char *dataCut;
        uint8_t dataCutLength = 0;
        dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
        if (*dataCut == 'R') return true; // expected string is 'READY'. no other possible string begins with R
        else return false;
    }

    /**
     * @brief   Blocks process until terminatingChar received.
     * @param   terminatingChar:    Character to block until.
     * @todo    Make sure this doesn't block longer than 250 ms.
     * @retval  True if character found, or false on 1000ms timeout
     */
    bool flushUntil(uint8_t _terminatingChar)
    {
        const uint8_t terminatingChar = _terminatingChar;
        const uint8_t endTime = OTV0P2BASE::getSecondsLT() + flushTimeOut;
        while (OTV0P2BASE::getSecondsLT() <= endTime) { // FIXME Replace this logic
            const uint8_t c = ser.read();
            if (c == terminatingChar) return true;
        }
    #if 0
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" Timeout")
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN(OTV0P2BASE::getSecondsLT())
    #endif
        return false;
    }

    /**
     * @brief   Returns a pointer to section of response containing important data
     *          and sets its length to a variable
     * @param   newLength:  length of useful data.
     * @param   data:       pointer to array containing response from device.
     * @param   dataLength: length of array.
     * @param   startChar:  Ignores everything up to and including this character.
     * @retval  pointer to start of useful data.
     */
    const char *getResponse(uint8_t &newLength, const char *data, uint8_t dataLength, char _startChar)
    {
        char startChar = _startChar;
        const char *newPtr = NULL;
        uint8_t  i = 0;    // 'AT' + command + 0x0D
        uint8_t i0 = 0; // start index
        newLength = 0;
        // Ignore echo of command
        while (*data !=  startChar) {
            data++;
            i++;
            if(i >= dataLength) return NULL;
        }
        data++;
        i++;
        // Set pointer to start of and index
        newPtr = data;
        i0 = i;
        // Find end of response
        while(*data != 0x0D) {    // find end of response
            data++;
            i++;
            if(i >= dataLength) return NULL;
        }
        newLength = i - i0;
    #if 0 && defined(OTSIM900LINK_DEBUG)
        char *stringEnd = (char *)data;
         *stringEnd = '\0';
        OTV0P2BASE::serialPrintAndFlush(newPtr);
        OTV0P2BASE::serialPrintlnAndFlush();
    #endif // OTSIM900LINK_DEBUG
        return newPtr;    // return length of new array
    }

    /**
     * @brief   Open UDP socket.
     * @todo    Find better way of printing this (maybe combine as in APN).
     * @param   array containing server IP
     * @retval  Returns true if UDP opened
     */
    bool openUDP()
    {
        char data[MAX_SIM900_RESPONSE_CHARS];
        memset(data, 0, sizeof(data));
        ser.print(AT_START);
        ser.print(AT_START_UDP);
        ser.print("=\"UDP\",");
        ser.print('\"');
        printConfig(config->UDP_Address);
        ser.print("\",\"");
        printConfig(config->UDP_Port);
        ser.println('\"');
    //    ser.print(AT_END);
        // Implement check here
        timedBlockingRead(data, sizeof(data));

        // response stuff
        const char *datacut;
        uint8_t dataCutLength = 0;
        datacut = getResponse(dataCutLength, data, sizeof(data), 0x0A);
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN(datacut)
        if(datacut[0] == 'E') return false;  // Returns ERROR on fail...
        else return true;  // Successfully opened UDP.
    }
    /**
     * @brief   Close UDP connection.
     * @todo    Implement checks.
     * @retval  True if UDP closed.
     * @note    Check UDP open?
     */
    bool closeUDP()
    {
        ser.print(AT_START);
        ser.println(AT_CLOSE_UDP);
    //    ser.print(AT_END);
        return false;
    }
    /**
     * @brief   Send a UDP frame.
     * @todo    Split this into init sending and write message? Need to check how long it blocks.
     * @param   frame:  Pointer to array containing frame to send.
     * @param   length: Length of frame.
     * @retval  True if send successful.
     */
    bool sendUDP(const char *frame, uint8_t length)
    {
        ser.print(AT_START);
        ser.print(AT_SEND_UDP);
        ser.print('=');
        ser.println(length);
    //    ser.print(AT_END);
        if (flushUntil('>')) {  // '>' indicates module is ready for UDP frame
            ser.write(frame, length);
            OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*success")
            return true;
        } else {
            OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*fail")
            return false;
        }
    }
//    OTSIM900LinkState getInitState();
    /**
     * @brief   Checks module for response.
     * @retval  0 if correct response.
     * @retval  -1 if unexpected response.
     */
    uint8_t interrogateSIM900()
    {
        ser.println(AT_START);
    //    ser.print(AT_END);
    //    uint8_t c = 0;
        // Debug code...
    //    uint8_t startTime = OTV0P2BASE::getSecondsLT();
    //    c = ser.read();
    //    uint8_t endTime = OTV0P2BASE::getSecondsLT();
    //    OTSIM900LINK_DEBUG_SERIAL_PRINT("T: ")
    //    OTSIM900LINK_DEBUG_SERIAL_PRINT(startTime)
    //  OTSIM900LINK_DEBUG_SERIAL_PRINT("\t")
    //    OTSIM900LINK_DEBUG_SERIAL_PRINT(endTime)
    //  OTSIM900LINK_DEBUG_SERIAL_PRINT("\t")
    //    OTSIM900LINK_DEBUG_SERIAL_PRINTFMT(c, HEX)
    //    OTSIM900LINK_DEBUG_SERIAL_PRINTLN()
    //    if (c == 'A') {
        if (ser.read() == 'A') {  // This is the expected response.
            return 0;
        } else {  // This means no response.
            return -1;
        }
    }
//    uint8_t checkInterrogationResponse();

    /**
    * @brief     Assigns OTSIM900LinkConfig config. Must be called before begin()
    * @retval    returns true if assigned or false if config is NULL
    */
    bool _doconfig()
    {
       if (channelConfig->config == NULL) return false;
       else {
           config = (const OTSIM900LinkConfig_t *) channelConfig->config;
           return true;
       }
    }

    volatile OTSIM900LinkState state;     // TODO check this is in correct place
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
virtual void preinit(const void *preconfig) {}    // not really relevant?
virtual void panicShutdown() { preinit(NULL); }    // see above
*/
};


//const char AT_[] = "";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_START[3] = "AT";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_SIGNAL[5] = "+CSQ";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_NETWORK[6] = "+COPS";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_REGISTRATION[6] = "+CREG"; // GSM registration.
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_GPRS_REGISTRATION0[7] = "+CGATT"; // GPRS registration.
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_GPRS_REGISTRATION[7] = "+CGREG"; // GPRS registration.
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_SET_APN[6] = "+CSTT";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_START_GPRS[7] = "+CIICR";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_GET_IP[7] = "+CIFSR";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_PIN[6] = "+CPIN";

template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_STATUS[11] = "+CIPSTATUS";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_START_UDP[10] = "+CIPSTART";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_SEND_UDP[9] = "+CIPSEND";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_CLOSE_UDP[10] = "+CIPCLOSE";
template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_SHUT_GPRS[9] = "+CIPSHUT";

template<uint8_t rxPin, uint8_t txPin>
const char OTSIM900Link<rxPin, txPin>::AT_VERBOSE_ERRORS[6] = "+CMEE";



}    // namespace OTSIM900Link



#endif /* OTSIM900LINK_H_ */
