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

Author(s) / Copyright (s): Deniz Erbilgin 2016
                           Damon Hart-Davis 2016
*/

/*
 * OpenTRV RN2483 LoRA Radio Link base class.
 *
 * Currently V0p2/AVR ONLY.
 */

//Collection of useful links:
//http://openlora.com/forum/viewtopic.php?f=5&t=6
//http://forum.thethingsnetwork.org/t/ttn-uno-beta-release-documentation/290/47?u=nestorayuso
//http://thinginnovations.uk/getting-started-with-microchip-rn2483-lorawan-modules
//
//Command list:
//http://ww1.microchip.com/downloads/en/DeviceDoc/40001784C.pdf


/**
 * @brief   Set dev addr on line 215 of OTRN2483Link_OTRN2483Link.cpp
 *          Set data rate on line 57 of OTRN2483Link_OTRN2483Link.cpp
 *          Set adaptive data rate by uncommmenting #define RN2483_ENABLE_ADR below and setting limits on line 55 of #define RN2483_ENABLE_ADR
 * @todo    - Add config functionality
 *          - Move commands to progmem
 *          - Add intelligent way of utilising device eeprom (w/ mac save)
 *          - Is there any special low power mode?
 *          - Save stuff into EEPROM
 */

#ifndef OTRN2483LINK_OTRN2483LINK_H_
#define OTRN2483LINK_OTRN2483LINK_H_

// IF DEFINED: Puts RN2483 to sleep after send
//#define RN2483_ALLOW_SLEEP
// IF DEFINED: Assumes that LoRaWAN settings are saved in RN2483 EEPROM and does not attempt to set them itself
//#define RN2483_CONFIG_IN_EEPROM
// IF DEFINED: Enables adaptive data rate. This is only relevant if RN2483_CONFIG_IN_EEPROM is UNDEFINED
#define RN2483_ENABLE_ADR // TODO: Untested but may reduce power consumption

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifdef ARDUINO_ARCH_AVR
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#endif

#include <OTRadioLink.h>
#include <OTV0p2Base.h>
#include <string.h>
#include <stdint.h>

namespace OTRN2483Link
{


#ifdef ARDUINO_ARCH_AVR
/**
 * @struct  OTRN2483LinkConfig
 * @brief   Structure containing config data for OTRN2483LinkConfig
 * @todo    Work out other required variables for LoRa
 */
#define OTRN2483LinkConfig_DEFINED
typedef struct OTRN2483LinkConfig {
    // Is in eeprom?
    const bool bEEPROM;
    const void * const UDP_Address;
    const void * const UDP_Port;

    OTRN2483LinkConfig(bool e, const void */*p*/, const void */*a*/, const void *ua, const void *up)
      : bEEPROM(e), UDP_Address(ua), UDP_Port(up) { }
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
} OTRN2483LinkConfig_t;


/**
 * @brief   This is a class that extends OTRadioLink to communicate via LoRaWAN
 *          using the RN2483 radio module.
 */
#define OTRN2483Link_DEFINED
class OTRN2483Link final : public OTRadioLink::OTRadioLink
{
public:
	// Public interface
    OTRN2483Link(uint8_t _nRstPin, uint8_t rxPin, uint8_t txPin);

    void preinit(const void */*preconfig*/) {};
    bool begin();
    bool end();

    bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false);
//    bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal);
    inline bool isAvailable(){ return bAvailable; };     // checks radio is there independant of power state
    bool handleInterruptSimple() { return true;};

    /**
     * @brief   Unused. For compatibility with OTRadioLink.
     */
    void poll();
    void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const;
    uint8_t getRXMsgsQueued() const;
    const volatile uint8_t *peekRXMsg() const;
    void removeRXMsg() {};


private:
// Private methods
    // Serial
    uint8_t read();
    uint8_t timedBlockingRead(char *data, uint8_t length);
    void write(const char *data, uint8_t length);
    void print(const char data);
    void print(const uint8_t value);	// todo change this so it prints in hex?
    void print(const char *string);
    void print(const void *src);

    // Commands
    void factoryReset();
    void reset();
    void setBaud();
    void setDevAddr(const uint8_t *address);
    void setKeys(const uint8_t *appKey, const uint8_t *networkKey);
    void joinABP();
    bool getStatus();
    void save();
    void setDataRate(uint8_t dataRate);
    void setAdaptiveDataRate(uint8_t minRate, uint8_t maxRate);
    void setTxPower(uint8_t power);

    // Setup
    bool _doconfig() { return true; };

    // misc
    bool getHex(const uint8_t *string, uint8_t *output, uint8_t outputLen);

// Private consts and variables
    const OTRN2483LinkConfig *config;  // Pointer to radio config
    OTV0P2BASE::OTSoftSerial ser;
    static const uint16_t baud = 2400;	 // OTSoftSer baud rate. todo switch to template to allow higher speed
    bool bAvailable;
    const uint8_t nRstPin;


    static const char SYS_START[5];	  // Beginning of "sys" command set
    static const char SYS_SLEEP[7];   // Sleep mode
    static const char SYS_RESET[6]; // todo this can be removed on board with working reset line

    static const char MAC_START[5];   // Beginning of "mac" command set
#ifndef RN2483_CONFIG_IN_EEPROM
    static const char MAC_DEVADDR[9]; // device address (required for ABP)
    static const char MAC_APPSKEY[9]; // Application session key (required for ABP)
    static const char MAC_NWKSKEY[9]; // Network session key (required for ABP)
    static const char MAC_ADR[7];     // Set Adaptive Datarate "on"
    static const char MAC_SET_DR[4];   // Set data rate.
    static const char MAC_SET_CH[4];  // Channel stuff
    static const char MAC_SET_DRRANGE[9]; // Set data rate range
    static const char MAC_POWER[9]; // Set Tx power
#endif // RN2483_CONFIG_IN_EEPROM
    static const char MAC_JOINABP[9]; // Join LoRaWAN network by ABP (activation by personalisation)
    static const char MAC_STATUS[7];
    static const char MAC_SEND[12];		// Sends an unconfirmed packet on channel 1
    static const char MAC_SAVE[5];

    static const char RN2483_SET[5];  // Set command
    static const char RN2483_GET[5];  // Get command
    static const char RN2483_END[3];  // End of command (CR LF)

    /**
     * @brief   Unused. For compatibility with OTRadioLink.
     */
    void _dolisten() {};
};
#endif // ARDUINO_ARCH_AVR


} // namespace OTRN2483Link
#endif /* OTRN2483LINK_OTRN2483LINK_H_ */
