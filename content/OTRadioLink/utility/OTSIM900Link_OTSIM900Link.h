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
                            Damon Hart-Davis 2015--2016
 */

/*
 * SIM900 Arduino (2G) GSM shield support.
 *
 * Fully operative for V0p2/AVR only.
 */

#ifndef OTSIM900LINK_H_
#define OTSIM900LINK_H_

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <OTRadioLink.h>
#include <OTV0p2Base.h>
#include <string.h>
#include <stdint.h>

// If DEFINED: Prints debug information to serial.
//             !!! WARNING! THIS WILL CAUSE BLOCKING OF OVER 300 MS!!!
#undef OTSIM900LINK_DEBUG

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
     * @note    Struct and internal pointers must last as long as OTSIM900Link object
     * @param    bEEPROM    true if strings stored in EEPROM, else held in FLASH
     * @param    PIN        Pointer to \0 terminated array containing SIM pin code
     * @param    APN        Pointer to \0 terminated array containing access point name
     * @param    UDP_Address    Pointer to \0 terminated array containing UDP address to send to as IPv4 dotted quad
     * @param    UDP_Port    Pointer to \0 terminated array containing UDP port in decimal
     */
// If config is stored in SRAM...
#define OTSIM900LinkConfig_DEFINED
    typedef struct OTSIM900LinkConfig final
        {
//private:
            // True if the text is in EEPROM.
            const bool bEEPROM;

            const void * const PIN;
            const void * const APN;
            const void * const UDP_Address;
            const void * const UDP_Port;

            constexpr OTSIM900LinkConfig(bool e, const void *p, const void *a,
                    const void *ua, const void *up) :
                    bEEPROM(e), PIN(p), APN(a), UDP_Address(ua), UDP_Port(up)
                { }
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
            char get(const uint8_t *const src) const
                {
#ifdef ARDUINO_ARCH_AVR
                char c = '\0';
                switch (bEEPROM)
                    {
                    case true:
                        {
                        c = char(eeprom_read_byte(src));
                        break;
                        }
                    case false:
                        {
                        c = char(pgm_read_byte(src));
                        break;
                        }
                    }
#else
                const char c = char(*src);
#endif // ARDUINO_ARCH_AVR
                return c;
                }
        } OTSIM900LinkConfig_t;


    /**
     * @brief   Enum containing major states of SIM900.
     */
    enum OTSIM900LinkState : uint8_t
        {
        INIT = 0,
        GET_STATE,
        WAIT_PWR_HIGH,
        WAIT_PWR_LOW,
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
        RESET,
        PANIC
        };

    // Includes string constants.
    class OTSIM900LinkBase : public OTRadioLink::OTRadioLink
        {
        protected:

#if defined(ARDUINO)
// Place strings in Flash on Arduino.
#define V0p2_SIM900_AT_FlashStringHelper
typedef const __FlashStringHelper *AT_t;
// Static definition.
#define V0p2_SIM900_AT_DEFN(v, s) \
    static const char _f_##v[] PROGMEM = (s); \
    OTSIM900LinkBase::AT_t OTSIM900LinkBase::v = reinterpret_cast<const __FlashStringHelper *>(_f_##v)
#else
// Static definition.
typedef const char *AT_t;
#define V0p2_SIM900_AT_DEFN(v, s) OTSIM900LinkBase::AT_t OTSIM900LinkBase::v = (s)
#endif

            static AT_t AT_START;
            static AT_t AT_SIGNAL;
            static AT_t AT_NETWORK;
            static AT_t AT_REGISTRATION; // GSM registration.
            static AT_t AT_GPRS_REGISTRATION0; // GPRS registration.
            static AT_t AT_GPRS_REGISTRATION; // GPRS registration.
            static AT_t AT_SET_APN;
            static AT_t AT_START_GPRS;
            static AT_t AT_GET_IP;
            static AT_t AT_PIN;
            static AT_t AT_STATUS;
            static AT_t AT_START_UDP;
            static AT_t AT_SEND_UDP;
            static AT_t AT_CLOSE_UDP;
            static AT_t AT_SHUT_GPRS;
            static AT_t AT_VERBOSE_ERRORS;

            // Single characters.
            const char ATc_GET_MODULE = 'I';
            const char ATc_SET = '=';
            const char ATc_QUERY = '?';

        public:
            // Max reliable baud to talk to SIM900 over OTSoftSerial2.
            constexpr static const uint16_t SIM900_MAX_baud = 9600;
        };

    /**
     * @note    To enable serial debug define 'OTSIM900LINK_DEBUG'
     * @todo    SIM900 has a low power state which stays connected to network
     *             - Not sure how much power reduced
     *             - If not sending often may be more efficient to power up and wait for connect each time
     */
#define OTSIM900Link_DEFINED
    template<uint8_t rxPin, uint8_t txPin, uint8_t PWR_PIN,
    uint_fast8_t (*const getCurrentSeconds)(), // Fetches clock time in seconds; never NULL
    class ser_t
#ifdef OTSoftSerial2_DEFINED
        = OTV0P2BASE::OTSoftSerial2<rxPin, txPin, OTSIM900LinkBase::SIM900_MAX_baud>
#endif
    >
    class OTSIM900Link final : public OTSIM900LinkBase
        {
            // Maximum number of significant chars in the SIM900 response.
            // Minimising this reduces stack and/or global space pressures.
            static constexpr int MAX_SIM900_RESPONSE_CHARS = 64;

#ifdef ARDUINO_ARCH_AVR
            // Regard as true when within a few ticks of start of 2s major cycle.
            inline bool nearStartOfMajorCycle() const
                { return(OTV0P2BASE::getSubCycleTime() < 10); }
#else
            // Regard as always true when not running embedded.
            inline bool nearStartOfMajorCycle() const { return(true); }
#endif

#ifdef ARDUINO_ARCH_AVR
            // Sets power pin HIGH if true, LOW if false.
            inline void setPwrPinHigh(const bool high)
                { fastDigitalWrite(PWR_PIN, high ? HIGH : LOW); }
#else
            // Reflect pin state in bool for unit testing..
            bool pinHigh = false;
            inline void setPwrPinHigh(const bool high) { pinHigh = high; }
#endif
            /**
             * @brief   Check if waited long enough using RTC.
             * @param   duration: number of seconds we need to wait. Strictly positive.
             * @reval   True if waited enough, else false.
             */
            bool waitedLongEnough(uint_fast8_t oldTime, uint_fast8_t duration) const
                { return OTV0P2BASE::getElapsedSecondsLT(oldTime, getCurrentSeconds()) > duration; }

        public:
            /**
             * @brief    Constructor. Initializes softSerial and sets PWR_PIN.
             * @param    pwrPin       SIM900 power on/off pin
             * @param    rxPin        Rx pin for software serial
             * @param    txPin        Tx pin for software serial
             *
             * Cannot do anything with side-effects,
             * as may be called before run-time is fully initialised.
             */
            constexpr OTSIM900Link() { /* memset(txQueue, 0, sizeof(txQueue)); */ }

            /************************* Public Methods *****************************/
            /**
             * @brief    Starts software serial, checks for module and inits state machine.
             */
            virtual bool begin() override
                {
#ifdef ARDUINO_ARCH_AVR
                pinMode(PWR_PIN, OUTPUT);
#endif
                setPwrPinHigh(false);
                ser.begin(0);
                state = INIT;
                return true;
                }

            /**
             * @brief    close UDP connection and power down SIM module
             */
            virtual bool end() override
                {
                UDPClose();
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
            virtual bool sendRaw(const uint8_t *buf, uint8_t buflen,
                    int8_t /*channel*/ = 0, TXpower /*power*/ = TXnormal,
                    bool /*listenAfter*/ = false) override
                {
                bool bSent = false;
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("Send Raw")
                bSent = UDPSend((const char *) buf, buflen);
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
            virtual bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t,
                    TXpower) override
                {
                if ((buf == NULL) || (buflen > sizeof(txQueue)))
                    return false;    //
//        if ((buf == NULL) || (buflen > sizeof(txQueue)) || (txMessageQueue >= maxTxQueueLength)) return false;    //
//        // Increment message queue
//        txMessageQueue++; //
                    // copy into queue here?
                txMessageQueue = 1;
                memcpy(txQueue, buf, buflen); // Last message queued is copied to buffer, ensuring freshest message is sent.
                txMsgLen = buflen;
                return true;
                }

            // Returns true if radio is present, independent of its power state.
            virtual bool isAvailable() const override { return(bAvailable); }

            /**
             * @brief   Polling routine steps through 4 stage state machine
             * @note    If state <NEW_STATE> needs retries, retryCounter must be set in the previous state,
             *          i.e. along side the "state = NEW_STATE;" expression. This is awkward and may change in the future.
             */
            virtual void poll() override
            {
                if (-1 != retryTimer) {  // not locked out when retryTimer is -1.
                    retryLockOut();
                    return;
                } else if (messageCounter == 255) { // Force a hard restart every 255 messages.
                    messageCounter = 0;  // reset counter.
                    state = RESET;
                    return;
                } else if (!nearStartOfMajorCycle()) {
                    return;
                } else {  // If passes all checks, run the state machine.
                    switch (state) {
                    case INIT:
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*INIT")
                        memset(txQueue, 0, sizeof(txQueue));
                        messageCounter = 0;
                        retryTimer = -1;
                        retryCounter = 0;
                        txMsgLen = 0;
                        txMessageQueue = 0;
                        bAvailable = false;
                        state = GET_STATE;
                        break;
                    case GET_STATE: // Check SIM900 is present and can be talked to. Takes up to 220 ticks?
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*GET_STATE")
                        if (isSIM900Replying()) {
                            bAvailable = true;
                        }
                        setPwrPinHigh(true);
                        powerTimer = static_cast<int8_t>(getCurrentSeconds());
                        state = WAIT_PWR_HIGH;
                        break;
                    case WAIT_PWR_HIGH:  // Toggle the pin.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*WAIT_PWR_HIGH")
                        if (waitedLongEnough(powerTimer, 2)) {  // check more than 2 seconds have passed.
                            setPwrPinHigh(false);
                            state = WAIT_PWR_LOW;
                        }
                        break;
                    case WAIT_PWR_LOW:
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*START_UP")
                        if (waitedLongEnough(powerTimer, powerLockOutDuration)) state = START_UP;
                        break;
                    case START_UP: // takes up to 150 ticks
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*START_UP")
                        if (isSIM900Replying()) {
                            state = CHECK_PIN;
                            retryCounter = maxRetriesDefault;
                        } else {
                            state = GET_STATE;
                        }
                        break;
                    case CHECK_PIN: // Set pin if required. Takes ~100 ticks to exit.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*CHECK_PIN")
                        if (isPINRequired()) {
                            state = WAIT_FOR_REGISTRATION;
                            retryCounter = 30;  // more retries when waiting for registration.
                        } else {
                            setRetryLock();
                        }
                        //                if(setPIN()) state = PANIC;// TODO make sure setPin returns true or false
                        break;
                    case WAIT_FOR_REGISTRATION: // Wait for registration to GSM network. Stuck in this state until success. Takes ~150 ticks to exit.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*WAIT_FOR_REG")
                        if (isRegistered()) {
                            state = SET_APN;
                            retryCounter = maxRetriesDefault;
                        } else {
                            setRetryLock();
                        }
                        break;
                    case SET_APN: // Attempt to set the APN. Stuck in this state until success. Takes up to 200 ticks to exit.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*SET_APN")
                        if (setAPN()) {
                            messageCounter = 0;
                            state = START_GPRS;
                            retryCounter = maxRetriesDefault;
                        } else {
                            setRetryLock();
                        }
                        break;
                    case START_GPRS:  // Start GPRS context.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN("*START_GPRS")
                        {
                            uint8_t udpState = checkUDPStatus();
                            if (3 == udpState) {  // GPRS active, UDP shut.
                                state = GET_IP;
                            } else if(0 == udpState) {  // GPRS shut.
                                startGPRS();
                            } else {
                                setRetryLock();
                            }
                        }
//                          if(!startGPRS()) state = GET_IP;  // TODO: Add retries, Option to shut GPRS here (probably needs a new state)
                        // FIXME 20160505: Need to work out how to handle this. If signal is marginal this will fail.
                        break;
                    case GET_IP: // Takes up to 200 ticks to exit.
                        // For some reason, AT+CIFSR must done to be able to do any networking.
                        // It is the way recommended in SIM900_Appication_Note.pdf section 3: Single Connections.
                        // This was not necessary when opening and shutting GPRS as in OTSIM900Link v1.0
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*GET IP")
                        getIP();
                        state = OPEN_UDP;
                        retryCounter = maxRetriesDefault;
                        break;
                    case OPEN_UDP: // Open a udp socket. Takes ~200 ticks to exit.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*OPEN UDP")
                        if (openUDPSocket()) {
                            state = IDLE;
                        } else {
                            setRetryLock();
                        }
                        break;
                    case IDLE:  // Waiting for outbound message.
                        if (txMessageQueue > 0) { // If message is queued, go to WAIT_FOR_UDP
                            state = WAIT_FOR_UDP; // TODO-748
                            retryCounter = maxRetriesDefault;
                        }
                        break;
                    case WAIT_FOR_UDP: // Make sure UDP context is open. Takes up to 200 ticks to exit.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*WAIT_FOR_UDP")
                        {
                            uint8_t udpState = checkUDPStatus();
                            if (udpState == 1) {  // UDP connected
                                state = SENDING;
                                retryCounter = 0;
                            }
//                            else if (udpState == 0) state = GET_STATE; // START_GPRS; // TODO needed for optional wake GPRS to send.
                            else if (udpState == 2) {  // Dead end. SIM900 needs resetting.
                                state = RESET;
                            } else {
                                setRetryLock();
                            }
                        }
                        break;
                    case SENDING: // Attempt to send a message. Takes ~100 ticks to exit.
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*SENDING")
                        if (txMessageQueue > 0) { // Check to make sure it is near the start of the subcycle to avoid overrunning.
                            // TODO logic to check if send attempt successful
                            sendRaw(txQueue, txMsgLen); /// @note can't use strlen with encrypted/binary packets
                            if (!(--txMessageQueue)) state = IDLE; // Once done, decrement number of messages in queue and return to IDLE
                        }
                        else if (txMessageQueue == 0) state = IDLE;
                        break;
                    case RESET:
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*RESET")
                        retryCounter = 0; // reset retry counter.
                        state = GET_STATE;
                        break;
                    case PANIC:
                        OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("SIM900_PANIC!");
                        break;
                    default:
                        break;
                    }
                }
            }

#ifndef OTSIM900LINK_DEBUG // This is included to ease unit testing.
        private:
#endif // OTSIM900LINK_DEBUG

            /***************** AT Commands and Private Constants and variables ******************/
            static constexpr uint8_t powerPinToggleDuration = 2;
            static constexpr uint8_t powerLockOutDuration = 10 + powerPinToggleDuration; // DE20160703:Increased duration due to startup issues.
            static constexpr uint8_t flushTimeOut = 10;
            // Standard Responses

            // Software serial: for V0p2 boards (eg REV10) expected to be of type:
            //     OTV0P2BASE::OTSoftSerial2<rxPin, txPin, baud>
            ser_t ser;

            // variables
            bool bAvailable = false;
            int8_t powerTimer = 0;
            uint8_t messageCounter = 0; // Number of frames sent. Used to schedule a reset.
            uint8_t retryCounter = 0;   // Count the number of retries attempted
//            uint8_t retriesRemaining = 0;   // Count the number of retries attempted
            int8_t retryTimer = -1;     // Store the retry lockout time. This takes a value in range [0,60] and is set to (-1) when no lockout is desired.
            static constexpr uint8_t maxRetriesDefault = 10;  // Default number of retries.
            volatile uint8_t txMessageQueue = 0; // Number of frames currently queued for TX.
            const OTSIM900LinkConfig_t *config = NULL;
            OTSIM900LinkState oldState;
            /************************* Private Methods *******************************/

        private:
//            /**
//             * @brief   If a state has changed, make sure things like retries are reset.
//             */
//            void onStateChange(const OTSIM900LinkState newState)
//            {
//                if (newState != oldState) {
//                    oldState = newState;
//                    retriesRemaining = maxRetriesDefault;
//                }
//            }
            /**
             * @brief   Check if enough time has passed to retry again and update the retry counter.
             * @note    retryCounter must be set by the caller.
             * @retval  True if locked out.
             */
            inline void retryLockOut()
            {
                if (0 == retryCounter) {
                    OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("resetting!")
                    retryTimer = -1; // clear lockout and go into reset.
                    state = RESET;
                } else if(waitedLongEnough(retryTimer, 2)) {
                    retryTimer = -1;
                }
            }
            /**
             * @brief   Sets the retryTimer
             */
            inline void setRetryLock()
            {
                retryCounter -= 1;
                retryTimer = getCurrentSeconds();
                OTSIM900LINK_DEBUG_SERIAL_PRINT_FLASHSTRING("--LOCKED! ")
                OTSIM900LINK_DEBUG_SERIAL_PRINT(retryCounter)
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" tries left.")
            }

            // Serial functions
            /**
             * @brief   Fills a buffer with characters from ser.read(). Exits when array filled, or if ser.read() times out.
             * @param   data:   Data buffer to write to.
             * @param   length: Length of data buffer.
             * @retval  Number of characters received before time out.
             */
            uint8_t readMany(char * const data, const uint8_t length)
                {
                // pointer to write data too.
                char *dp = data;
                // Init array to 0.
                memset(data, 0, length);
                // Loop through filling array until full or ser.read() returns -1 (time out).
                while ((dp-data) < length) {
                    const char c = char(ser.read());
                    if (c == -1) break;
                    else *dp++ = c;
                }

#ifndef ARDUINO_ARCH_AVR
                while (-1 != char(ser.read())) {};
#endif // ARDUINO_ARCH_AVR
#if 0
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("\n--Buffer Length: ")
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data-dp)
#endif
                return(uint8_t(data-dp));  // Return length of array.
                }
            /**
             * @brief   Utility function for printing from config structure.
             * @param   src:    Source to print from. Should be passed as a config-> pointer.
             */
            void printConfig(const void * src)
                {
                char c = char(0xff); // to avoid exiting the while loop without \0 getting written
                const uint8_t *ptr = (const uint8_t *) src;
                // loop through and print each value
                while (1)
                    {
                    c = config->get(ptr);
                    if (c == '\0')
                        return;
                    ser.print(c);
                    ptr++;
                    }
                }

            // write AT commands
            /**
             * @brief   Checks module ID.
             * @fixme    DOES NOT CHECK
             * @param   name:   pointer to array to compare name with.
             * @param   length: length of array name.
             * @retval  True if ID recovered successfully.
             */
            bool isModulePresent()
                {
                char data[OTV0P2BASE::fnmin(32, MAX_SIM900_RESPONSE_CHARS)];
                ser.print(AT_START);
                ser.println(ATc_GET_MODULE);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));
                OTSIM900LINK_DEBUG_SERIAL_PRINT(data)
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN()
                return true;
                }
            /**
             * @brief   Checks connected network.
             * @fixme   DOES NOT CHECK
             * @param   buffer: pointer to array to store network name in.
             * @param   length: length of buffer.
             * @param   True if connected to network.
             * @note
             */
            bool isNetworkCorrect()
                {
                char data[MAX_SIM900_RESPONSE_CHARS];
                ser.print(AT_START);
                ser.print(AT_NETWORK);
                ser.println(ATc_QUERY);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));
                return true;
                }
            /**
             * @brief   Check if module connected and registered (GSM and GPRS).
             * @retval  True if registered.
             * @note    reply: b'AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n'
             */
            bool isRegistered()
                {
                //  Check the GSM registration via AT commands ( "AT+CREG?" returns "+CREG:x,1" or "+CREG:x,5"; where "x" is 0, 1 or 2).
                //  Check the GPRS registration via AT commands ("AT+CGATT?" returns "+CGATT:1" and "AT+CGREG?" returns "+CGREG:x,1" or "+CGREG:x,5"; where "x" is 0, 1 or 2).
                char data[MAX_SIM900_RESPONSE_CHARS];
                ser.print(AT_START);
                ser.print(AT_REGISTRATION);
                ser.println(ATc_QUERY);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));
                // response stuff
                const char *dataCut = getResponse(data, sizeof(data), ' '); // first ' ' appears right before useful part of message
                if(NULL == dataCut) { return(false); }
                // Expected response '1' or '5'.
                return((dataCut[2] == '1') || (dataCut[2] == '5'));
                }

            /**
             * @brief   Set Access Point Name and start task.
             * @retval  True if APN set.
             * @note    reply: b'AT+CSTT="mobiledata"\r\n\r\nOK\r\n'
             */
            bool setAPN()
                {
                char data[MAX_SIM900_RESPONSE_CHARS];
                ser.print(AT_START);
                ser.print(AT_SET_APN);
                ser.print(ATc_SET);
                printConfig(config->APN);
                ser.println();
                readMany(data, sizeof(data));
                // response stuff
                const char *dataCut = getResponse(data, sizeof(data), 0x0A);
                if(NULL == dataCut) { return(-1); }
                return(dataCut[2] == 'O'); // Expected response 'OK'.
                }
            /**
             * @brief   Start GPRS connection.
             * @retval  True if connected.
             * @note    check power, check registered, check gprs active.
             * @note    reply: b'AT+CIICR\r\n\r\nOK\r\nAT+CIICR\r\n\r\nERROR\r\n' (not sure why OK then ERROR happens.)
             */
            bool startGPRS()
                {
                char data[OTV0P2BASE::fnmin(16, MAX_SIM900_RESPONSE_CHARS)];
                ser.print(AT_START);
                ser.println(AT_START_GPRS);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));

                // response stuff
                const char *dataCut = getResponse(data, sizeof(data), 0x0A); // unreliable
                if(NULL == dataCut) { return(-1); }
                return ((dataCut[0] == 'O') & (dataCut[1] == 'K'));
                }
            /**
             * @brief   Shut GPRS connection.
             * @retval  True if shut.
             */
            bool shutGPRS()
                {
                char data[MAX_SIM900_RESPONSE_CHARS]; // Was 96: that's a LOT of stack!
                ser.print(AT_START);
                ser.println(AT_SHUT_GPRS);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));

                // response stuff
                const char *dataCut = getResponse(data, sizeof(data), 0x0A);
                if(NULL == dataCut) { return(false); }
                // Expected response 'SHUT OK'.
                return (*dataCut == 'S');
                }
            /**
             * @brief   Get IP address from SIM900. Note that the function just returns a bool
             *          as we currently have no need to know our IP address.
             * @retval  True if no errors.
             * @note    reply: b'AT+CIFSR\r\n\r\n172.16.101.199\r\n'
             */
            bool getIP()
                {
                char data[MAX_SIM900_RESPONSE_CHARS];
                ser.print(AT_START);
                ser.println(AT_GET_IP);
                //  ser.print(AT_END);
                readMany(data, sizeof(data));
                // response stuff
                const char *dataCut = getResponse(data, sizeof(data), 0x0A);
                if(NULL == dataCut) { return(false); }
                // All error messages will start with a '+'.
                if (*dataCut == '+')
                    { return(false); }
                else
                    { return(true); }
                }
            /**
             * @brief   Check if UDP open.
             * @retval  0 if GPRS closed.
             * @retval  1 if UDP socket open.
             * @retval  2 if in dead end state.
             * @retval  3 if GPRS is active but no UDP socket.
             * @note    GPRS inactive:      b'AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n'
             * @note    GPRS active:   b'AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n'
             * @note    UDP running:       b'AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n'
             */
            uint8_t checkUDPStatus()
                {
                char data[MAX_SIM900_RESPONSE_CHARS];
                ser.print(AT_START);
                ser.println(AT_STATUS);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));

                // First ' ' appears right before useful part of message.
                const char *dataCut = getResponse(data, sizeof(data), ' ');
                if(NULL == dataCut) { return(0); }
                if (*dataCut == 'C')
                    return 1; // expected string is 'CONNECT OK'. no other possible string begins with C
                else if (*dataCut == 'P')
                    return 2;
                else if (dataCut[3] == 'G')
                    return 3;
                else
                    return 0;
                }
            /**
             * @brief   Get signal strength.
             * @todo    Return /print something?
             */
            uint8_t getSignalStrength()
                {
                char data[OTV0P2BASE::fnmin(32, MAX_SIM900_RESPONSE_CHARS)];
                ser.print(AT_START);
                ser.println(AT_SIGNAL);
                //    ser.print(AT_END);
                readMany(data, sizeof(data));
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
                // response stuff
                getResponse(data, sizeof(data), ' '); // first ' ' appears right before useful part of message
                return 0;
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
                ser.print(ATc_SET);
                ser.println((char) (level + '0'));
                //    ser.print(AT_END);
                readMany(data, sizeof(data));
            OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
            }
        /**
         * @brief   Enter PIN code
         * @todo    Check return value?
         * @retval  True if pin set successfully.
         */
        bool setPIN()
            {
            if (NULL == config->PIN)
                {
                return 0;
                } // do not attempt to set PIN if NULL pointer.
            char data[MAX_SIM900_RESPONSE_CHARS];
            ser.print(AT_START);
            ser.print(AT_PIN);
            ser.print(ATc_SET);
            printConfig(config->PIN);
            ser.println();
//        readMany(data, sizeof(data));  // todo redundant until function properly implemented.
//            OTSIM900LINK_DEBUG_SERIAL_PRINTLN(data)
            return true;
            }
        /**
         * @brief   Check if PIN required
         * @retval  True if SIM card unlocked.
         * @note    reply: b'AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n'
         */
        bool isPINRequired()
            {
            char data[OTV0P2BASE::fnmin(40, MAX_SIM900_RESPONSE_CHARS)];
            ser.print(AT_START);
            ser.print(AT_PIN);
            ser.println(ATc_QUERY);
            //    ser.print(AT_END);
            readMany(data, sizeof(data));

            // response stuff
            // First ' ' appears right before useful part of message
            const char *dataCut = getResponse(data, sizeof(data), ' ');
            if(NULL == dataCut) { return(false); }
            return('R' == *dataCut);  // Expected string is 'READY'. no other possible string begins with R.
            }

        /**
         * @brief   Blocks process until terminatingChar received.
         * @param   terminatingChar:    Character to block until.
         * @retval  True if character found, or false on 1000ms timeout
         */
        bool flushUntil(uint8_t _terminatingChar)
            {
            const uint8_t terminatingChar = _terminatingChar;
            const uint8_t endTime = getCurrentSeconds() + flushTimeOut;
            while (getCurrentSeconds() <= endTime)
                {
                const uint8_t c = uint8_t(ser.read());
                if (c == terminatingChar)
                    return true;
                }
#if 0
            OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING(" Timeout")
            OTSIM900LINK_DEBUG_SERIAL_PRINTLN(getCurrentSeconds())
#endif
            return false;
            }

        /**
         * @brief   Finds the first occurrence of a character within a buffer and returns a pointer to the next element.
         * @param   data:       pointer to array containing response from device.
         * @param   dataLength: length of array.
         * @param   startChar:  Ignores everything up to and including this character.
         * @retval  pointer to start of useful data; NULL if not found
         *
         * MUST CHECK RESPONSE AGAINST NULL FIRST
         */
        const char *getResponse(const char * const data, const uint8_t dataLength, const char startChar)
        {
            const char *dp = data;  // read only pointer to data buffer.
            // Ignore echo of command
            // dp will be pointing to the character after the match when the loop exits.
            while  (*dp++ != startChar) { // check for match THEN increment.
                if ((dp - data) >= dataLength)
                    return NULL;
            }
            return dp;
        }

        /**
         * @brief   Open UDP socket.
         * @param   array containing server IP
         * @retval  True if UDP opened
         * @note    reply: b'AT+CIPSTART="UDP","0.0.0.0","9999"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n'
         */
        bool openUDPSocket()
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
            // Implement check here
            readMany(data, sizeof(data));
            // response stuff
            const char *dataCut = getResponse(data, sizeof(data), 0x0A);
            if(NULL == dataCut) { return(false); }
            OTSIM900LINK_DEBUG_SERIAL_PRINTLN(dataCut)
            return ~('E' == *dataCut);  // Returns ERROR on fail, else successfully opened UDP.
            }
        /**
         * @brief   Close UDP connection.
         * @todo    Implement checks.
         * @retval  True if UDP closed.
         * @note    Check UDP open?
         */
        bool UDPClose()
            {
            ser.print(AT_START);
            ser.println(AT_CLOSE_UDP);
            return true;
            }
        /**
         * @brief   Send a UDP frame.
         * @param   frame:  Pointer to array containing frame to send.
         * @param   length: Length of frame.
         * @retval  True if send successful.
         * @note    On Success: b'AT+CIPSEND=62\r\n\r\n>' echos back input b'\r\nSEND OK\r\n'
         */
        bool UDPSend(const char *frame, uint8_t length)
            {
            messageCounter++; // increment counter
            ser.print(AT_START);
            ser.print(AT_SEND_UDP);
            ser.print('=');
            ser.println(length);
            //    ser.print(AT_END);
            if (flushUntil('>'))
                {  // '>' indicates module is ready for UDP frame
                (static_cast<Print *>(&ser))->write(frame, length);
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*success")
                return true;
                }
            else
                {
                OTSIM900LINK_DEBUG_SERIAL_PRINTLN_FLASHSTRING("*fail")
                return false;
                }
            }

        /**
         * @brief   Checks module for response.
         * @retval  True if correct response.
         * @note     reply: b'AT\r\n\r\nOK\r\n'
         */
        bool isSIM900Replying()
            {
            char data[OTV0P2BASE::fnmin(16, MAX_SIM900_RESPONSE_CHARS)];
            ser.println(AT_START);
            readMany(data, sizeof(data));
            return ('A' == *data);
            }

        /**
         * @brief     Assigns OTSIM900LinkConfig config. Must be called before begin()
         * @retval    returns true if assigned or false if config is NULL
         */
        virtual bool _doconfig() override
        {
            if (channelConfig->config == NULL)
                return false;
            else {
                config = (const OTSIM900LinkConfig_t *) channelConfig->config;
                if ("\0" == config->get((const uint8_t *) config->PIN)) return false;
                else if ("\0" == config->get((const uint8_t *) config->APN)) return false;
                else if ("\0" == config->get((const uint8_t *) config->UDP_Address)) return false;
                else if ("\0" == config->get((const uint8_t *) config->UDP_Port)) return false;
                else return true;
            }
        }

        volatile OTSIM900LinkState state = INIT;
        uint8_t txMsgLen = 0; // This stores the length of the tx message. will have to be redone for multiple txQueue
        static constexpr uint8_t maxTxQueueLength = 1;

        // Putting this last in the structure.
        uint8_t txQueue[64]; // 64 is maxTxMsgLen (from OTRadioLink)

    public:
        // define abstract methods here
        // These are unused as no RX
        virtual void _dolisten() override
            {
            }
        virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen,
                uint8_t &maxTXMsgLen) const override
            {
            queueRXMsgsMin = 0;
            maxRXMsgLen = 0;
            maxTXMsgLen = 64;
            }
        ;
        virtual uint8_t getRXMsgsQueued() const override
            {
            return 0;
            }
        virtual const volatile uint8_t *peekRXMsg() const override
            {
            return 0;
            }
        virtual void removeRXMsg() override
            {
            }

        /* other methods (copied from OTRadioLink as is)
         virtual void preinit(const void *preconfig) {}    // not really relevant?
         virtual void panicShutdown() { preinit(NULL); }    // see above
         */

#ifdef ARDUINO_ARCH_AVR
        bool _isPinHigh() { return fastDigitalRead(PWR_PIN); }
#else
        // Provided to assist with "white-box" unit testing.
        OTSIM900LinkState _getState() { return(state); }
        bool _isPinHigh() {return pinHigh;}
#endif // ARDUINO_ARCH_AVR

    };

}    // namespace OTSIM900Link

#endif /* OTSIM900LINK_H_ */
