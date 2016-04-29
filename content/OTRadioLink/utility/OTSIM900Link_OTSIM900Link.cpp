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

#include "OTSIM900Link_OTSIM900Link.h"

namespace OTSIM900Link
{

/**
 * @brief    Constructor. Initializes softSerial and sets PWR_PIN
 * @param    pwrPin        SIM900 power on/off pin
 * @param    rxPin        Rx pin for software serial
 * @param    txPin        Tx pin for software serial
 *
 * Cannot do anything with side-effects,
 * as may be called before run-time fully initialised!
 */
OTSIM900Link::OTSIM900Link(uint8_t hardPwrPin, uint8_t pwrPin, uint8_t rxPin, uint8_t txPin)
  : HARD_PWR_PIN(hardPwrPin), PWR_PIN(pwrPin), softSerial(rxPin, txPin)
{
  bAvailable = false;
  bPowered = false;
  bPowerLock = false;
  powerTimer = 0;
  duration = 3; // seconds
  config = NULL;
  state = IDLE;
  memset(txQueue, 0, sizeof(txQueue));
  txMsgLen = 0;
  txMessageQueue = 0;
}

/**
 * @brief     Assigns OTSIM900LinkConfig config. Must be called before begin()
 * @retval    returns true if assigned or false if config is NULL
 */
bool OTSIM900Link::_doconfig()
{
    if (channelConfig->config == NULL) return false;
    else {
        config = (const OTSIM900LinkConfig_t *) channelConfig->config;
        return true;
    }
}

/**
 * @brief    Starts software serial, checks for module and inits state machine.
 */
bool OTSIM900Link::begin()
{
    pinMode(PWR_PIN, OUTPUT);
    fastDigitalWrite(PWR_PIN, LOW);
    softSerial.begin(baud);
    state = GET_STATE;
    return true;
}

/**
 * @brief    close UDP connection and power down SIM module
 */
bool OTSIM900Link::end()
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
bool OTSIM900Link::sendRaw(const uint8_t *buf, uint8_t buflen, int8_t , TXpower , bool)
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
bool OTSIM900Link::queueToSend(const uint8_t *buf, uint8_t buflen, int8_t , TXpower )
{
    if ((buf == NULL) || (buflen > sizeof(txQueue)) || (txMessageQueue >= maxTxQueueLength)) return false;    // TODO check logic and sort out maxTXMsgLen problem
    // Increment message queue
    txMessageQueue++;
    // copy into queue here?
    memcpy(txQueue, buf, buflen);
    txMsgLen = buflen;
    return true;
}

/**
 * @brief   Polling routine steps through 4 stage state machine
 */
void OTSIM900Link::poll()
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
                break;
            case CHECK_PIN:  // Set pin if required. Takes ~100 ticks to exit.
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*CHECK_PIN")
                if(checkPIN()) state = WAIT_FOR_REGISTRATION;
                else if(setPIN()) state = PANIC;// TODO make sure setPin returns true or false
                break;
            case WAIT_FOR_REGISTRATION:  // Wait for registration to GSM network. Stuck in this state until success. Takes ~150 ticks to exit.
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*WAIT_FOR_REG")
                if(isRegistered()) state = SET_APN;
                break;
            case SET_APN:  // Attempt to set the APN. Stuck in this state until success. Takes up to 200 ticks to exit.
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*SET_APN")
                if(!setAPN()) state = START_GPRS; // TODO: Watchdog resets here
                break;
            case START_GPRS:  // Start GPRS context. Takes ~50 ticks to exit
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN("*START_GPRS")
                if(!startGPRS()) state = GET_IP;  // TODO: Add retries, Option to shut GPRS here (probably needs a new state)
                else state = PANIC; // If startGPRS() ever returns an error here then something has probably gone very wrong
                break;
            case GET_IP: // Takes up to 200 ticks to exit.
                // For some reason, AT+CIFSR must done to be able to do any networking.
                // It is the way recommended in SIM900_Appication_Note.pdf section 3: Single Connections.
                // This was not necessary when opening and shutting GPRS as in OTSIM900Link v1.0
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*GET IP")
                OTSIM900LINK_DEBUG_SERIAL_PRINT(OTV0P2BASE::getSubCycleTime())
                OTSIM900LINK_DEBUG_SERIAL_PRINT('\t')
                if(getIP()) state = OPEN_UDP;
                OTSIM900LINK_DEBUG_SERIAL_PRINT(OTV0P2BASE::getSubCycleTime())
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN()
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
        //            else if (udpState == 0) state = START_GPRS; // TODO needed for optional wake GPRS to send.
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
    } else if (OTV0P2BASE::getSecondsLT() > powerTimer) {  // Check if ready to stop waiting after power toggled.
        bPowerLock = false;
    }
}

/**
 * @brief   Open UDP socket.
 * @todo    Find better way of printing this (maybe combine as in APN).
 * @param   array containing server IP
 * @retval  Returns true if UDP opened
 */
bool OTSIM900Link::openUDP()
{
    char data[MAX_SIM900_RESPONSE_CHARS];
    memset(data, 0, sizeof(data));
    print(AT_START);
    print(AT_START_UDP);
    print("=\"UDP\",");
    print('\"');
    print(config->UDP_Address);
    print("\",\"");
    print(config->UDP_Port);
    print('\"');
    print(AT_END);
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
bool OTSIM900Link::closeUDP()
{
    print(AT_START);
    print(AT_CLOSE_UDP);
    print(AT_END);
    return false;
}

/**
 * @brief   Send a UDP frame.
 * @todo    Split this into init sending and write message? Need to check how long it blocks.
 * @param   frame:  Pointer to array containing frame to send.
 * @param   length: Length of frame.
 * @retval  True if send successful.
 */
bool OTSIM900Link::sendUDP(const char *frame, uint8_t length)
{
    print(AT_START);
    print(AT_SEND_UDP);
    print('=');
    print(length);
    print(AT_END);
    if (flushUntil('>')) {  // '>' indicates module is ready for UDP frame
        write(frame, length);
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*success")
        return true;
    } else {
        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*fail")
        return false;
    }
}

/**
 * @brief   Reads a single character from softSerial.
 * @retval  Character read, or -1 if none.
 */
//uint8_t OTSIM900Link::read()
//{
//    uint8_t data;
//    data = softSerial.read();
//    return data;
//}

/**
 * @brief   Enter blocking read. Fills buffer or times out after 100 ms.
 * @param   data:   Data buffer to write to.
 * @param   length: Length of data buffer.
 * @retval  Number of characters received before time out.
 */
uint8_t OTSIM900Link::timedBlockingRead(char *data, uint8_t length)
{
    // clear buffer, get time and init i to 0
    uint8_t counter = 0;
    uint8_t len = length;
    char *pdata = data;
    memset(data, 0, length);
    while(len--) {
        char c = softSerial.read();
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
 * @brief   Blocks process until terminatingChar received.
 * @param   terminatingChar:    Character to block until.
 * @todo    Make sure this doesn't block longer than 250 ms.
 * @retval  True if character found, or false on 1000ms timeout
 */
bool OTSIM900Link::flushUntil(uint8_t _terminatingChar)
{
    const uint8_t terminatingChar = _terminatingChar;
    const uint8_t endTime = OTV0P2BASE::getSecondsLT() + flushTimeOut;
    while (OTV0P2BASE::getSecondsLT() <= endTime) { // FIXME Replace this logic
        const uint8_t c = softSerial.read();
        if (c == terminatingChar) return true;
    }
#if 0
    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" Timeout")
    OTSIM900LINK_DEBUG_SERIAL_PRINTLN(OTV0P2BASE::getSecondsLT())
#endif
    return false;
}

/// @todo:  Are these print and write functions redundant?
/**
 * @brief   Writes an array to software serial.
 * @param   data:   data buffer to write from.
 * @param   length: length of data buffer.
 */
void OTSIM900Link::write(const char *data, uint8_t length)
{
    softSerial.write(data, length);
}

/**
 * @brief   Writes a character to software serial.
 * @param   data:   character to write.
 */
void OTSIM900Link::print(char data)
{
    softSerial.print(data);
}

/**
 * @brief   Writes a character to software serial
 * @param   data:   character to write.
 */
void OTSIM900Link::print(const uint8_t value)
{
  softSerial.print(value);    // FIXME
}

void OTSIM900Link::print(const char *string)
{
    softSerial.print(string);
}

/**
 * @brief    Copies string from EEPROM and prints to softSerial
 * @fixme    Potential for infinite loop
 * @param    pointer to eeprom location string is stored in
 */
void OTSIM900Link::print(const void *src)
{
    char c = 0xff;    // to avoid exiting the while loop without \0 getting written
    const uint8_t *ptr = (const uint8_t *) src;
    // loop through and print each value
    while (1) {
        c = config->get(ptr);
        if (c == '\0') return;
        print(c);
        ptr++;
    }
}

/**
 * @brief   Checks module ID.
 * @todo    Implement check?
 * @param   name:   pointer to array to compare name with.
 * @param   length: length of array name.
 * @retval  True if ID recovered successfully.
 */
bool OTSIM900Link::checkModule()
{
char data[min(32, MAX_SIM900_RESPONSE_CHARS)];
    print(AT_START);
    print(AT_GET_MODULE);
    print(AT_END);
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
bool OTSIM900Link::checkNetwork()
{
    char data[MAX_SIM900_RESPONSE_CHARS];
    print(AT_START);
    print(AT_NETWORK);
    print(AT_QUERY);
    print(AT_END);
    timedBlockingRead(data, sizeof(data));
    return true;
}

/**
 * @brief   Check if module connected and registered (GSM and GPRS).
 * @retval  True if registered.
 */
bool OTSIM900Link::isRegistered()
{
//  Check the GSM registration via AT commands ( "AT+CREG?" returns "+CREG:x,1" or "+CREG:x,5"; where "x" is 0, 1 or 2).
//  Check the GPRS registration via AT commands ("AT+CGATT?" returns "+CGATT:1" and "AT+CGREG?" returns "+CGREG:x,1" or "+CGREG:x,5"; where "x" is 0, 1 or 2). 
    char data[MAX_SIM900_RESPONSE_CHARS];
    print(AT_START);
    print(AT_REGISTRATION);
    print(AT_QUERY);
    print(AT_END);
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
uint8_t OTSIM900Link::setAPN()
{
    char data[MAX_SIM900_RESPONSE_CHARS]; // FIXME: was 96: that's a LOT of stack!
    print(AT_START);
    print(AT_SET_APN);
    print(AT_SET);
    print(config->APN);
    print(AT_END);
    timedBlockingRead(data, sizeof(data));
    // response stuff
    const char *dataCut;
    uint8_t dataCutLength = 0;
    dataCut = getResponse(dataCutLength, data, sizeof(data), 0x0A);
//    OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
    if (*dataCut == 'O') return 0;    // expected response 'OK'
    else return -1;
}

/**
 * @brief   Start GPRS connection.
 * @retval  0 if connected.
 *          -1 if failed.
 * @note    check power, check registered, check gprs active.
 */
uint8_t OTSIM900Link::startGPRS()
{
    char data[min(16, MAX_SIM900_RESPONSE_CHARS)];
    print(AT_START);
    print(AT_START_GPRS);
    print(AT_END);
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
uint8_t OTSIM900Link::shutGPRS()
{
    char data[MAX_SIM900_RESPONSE_CHARS]; // Was 96: that's a LOT of stack!
    print(AT_START);
    print(AT_SHUT_GPRS);
    print(AT_END);
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
uint8_t OTSIM900Link::getIP()
{
  char data[MAX_SIM900_RESPONSE_CHARS];
  print(AT_START);
  print(AT_GET_IP);
  print(AT_END);
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
uint8_t OTSIM900Link::isOpenUDP()
{
    char data[MAX_SIM900_RESPONSE_CHARS];
    print(AT_START);
    print(AT_STATUS);
    print(AT_END);
    timedBlockingRead(data, sizeof(data));

    // response stuff
    const char *dataCut;
    uint8_t dataCutLength = 0;
    dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
    if (*dataCut == 'C') return 1; // expected string is 'CONNECT OK'. no other possible string begins with C
    else if (*dataCut == 'P') return 2;
    else return false;
}

/**
 * @brief   Set verbose errors
 * @param   level: 0 is no error codes, 1 is with codes, 2 is human readable descriptions.
 */
void OTSIM900Link::verbose(uint8_t level)
{
    char data[MAX_SIM900_RESPONSE_CHARS];
    print(AT_START);
    print(AT_VERBOSE_ERRORS);
    print(AT_SET);
    print((char)(level + '0'));
    print(AT_END);
    timedBlockingRead(data, sizeof(data));
    OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
}

/**
 * @brief   Enter PIN code
 * @todo    Check return value?
 */
uint8_t OTSIM900Link::setPIN()
{
    char data[MAX_SIM900_RESPONSE_CHARS];
    print(AT_START);
    print(AT_PIN);
    print(AT_SET);
    print(config->PIN);
    print(AT_END);
    timedBlockingRead(data, sizeof(data));
    OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
    return 0;
}

/**
 * @brief   Check if PIN required
 */
bool OTSIM900Link::checkPIN()
{
    char data[min(40, MAX_SIM900_RESPONSE_CHARS)];
    print(AT_START);
    print(AT_PIN);
    print(AT_QUERY);
    print(AT_END);
    timedBlockingRead(data, sizeof(data));

    // response stuff
    const char *dataCut;
    uint8_t dataCutLength = 0;
    dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
    if (*dataCut == 'R') return true; // expected string is 'READY'. no other possible string begins with R
    else return false;
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
const char *OTSIM900Link::getResponse(uint8_t &newLength, const char *data, uint8_t dataLength, char _startChar)
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
 * @brief   Test if radio is available and set available and power flags
 *          returns to powered off state?
 * @retval  START_UP if module found and returns correct start value.
 *          PANIC if failed
 * @note    Possible states at start up:
 *             1. no module - No response
 *             2. module not powered - No response
 *             3. module powered - correct response
 *             4. wrong module - unexpected response
 */
//OTSIM900LinkState OTSIM900Link::getInitState()
//{
//    if (interrogateSIM900()) {
//        powerToggle();
//        if(interrogateSIM900()) {
//            bAvailable = false;
//            bPowered = false;
//            return PANIC;
//        }
//    }
//    bAvailable = true;
//    bPowered = true;
//    return START_UP;
//}

/**
 * @brief   Checks module for response.
 * @retval  0 if correct response.
 * @retval  -1 if unexpected response.
 */
uint8_t OTSIM900Link::interrogateSIM900()
{
    print(AT_START);
    print(AT_END);
    if (softSerial.read() == 'A') {  // This is the expected response.
        return 0;
    } else {  // This means no response.
        return -1;
    }
}

/**
 * @brief   Get signal strength.
 * @todo    Return /print something?
 */
void OTSIM900Link::getSignalStrength()
{
    char data[min(32, MAX_SIM900_RESPONSE_CHARS)];
    print(AT_START);
    print(AT_SIGNAL);
    print(AT_END);
    timedBlockingRead(data, sizeof(data));
    OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
    // response stuff
    uint8_t dataCutLength = 0;
    getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
}

/**
 * @brief   placeholder
 * @retval  returns true on successful exit
 */
bool OTSIM900Link::handleInterruptSimple()
{
    return true;
}

/**
 * @brief   toggles power and sets power lock.
 * @fixme   proper ovf testing not implemented so the SIM900 may not power on/off near the end of a 60 second cycle.
 */
void OTSIM900Link::powerToggle()
{
    fastDigitalWrite(PWR_PIN, HIGH);
    delay(1000);  // This is the minimum value that worked reliably
    fastDigitalWrite(PWR_PIN, LOW);
    bPowered = !bPowered;
//    delay(3000);
    bPowerLock = true;
    powerTimer = min((OTV0P2BASE::getSecondsLT() + duration), 58);  // fixme!
}


//const char OTSIM900Link::AT_[] = "";
const char OTSIM900Link::AT_START[3] = "AT";
const char OTSIM900Link::AT_SIGNAL[5] = "+CSQ";
const char OTSIM900Link::AT_NETWORK[6] = "+COPS";
const char OTSIM900Link::AT_REGISTRATION[6] = "+CREG"; // GSM registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION0[7] = "+CGATT"; // GPRS registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION[7] = "+CGREG"; // GPRS registration.
const char OTSIM900Link::AT_SET_APN[6] = "+CSTT";
const char OTSIM900Link::AT_START_GPRS[7] = "+CIICR";
const char OTSIM900Link::AT_GET_IP[7] = "+CIFSR";
const char OTSIM900Link::AT_PIN[6] = "+CPIN";

const char OTSIM900Link::AT_STATUS[11] = "+CIPSTATUS";
const char OTSIM900Link::AT_START_UDP[10] = "+CIPSTART";
const char OTSIM900Link::AT_SEND_UDP[9] = "+CIPSEND";
const char OTSIM900Link::AT_CLOSE_UDP[10] = "+CIPCLOSE";
const char OTSIM900Link::AT_SHUT_GPRS[9] = "+CIPSHUT";

const char OTSIM900Link::AT_VERBOSE_ERRORS[6] = "+CMEE";

// tcpdump -Avv udp and dst port 9999


} // OTSIM900Link
