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
*/

//Collection of useful links:
//http://openlora.com/forum/viewtopic.php?f=5&t=6
//http://forum.thethingsnetwork.org/t/ttn-uno-beta-release-documentation/290/47?u=nestorayuso
//http://thinginnovations.uk/getting-started-with-microchip-rn2483-lorawan-modules
//
//Command list:
//http://ww1.microchip.com/downloads/en/DeviceDoc/40001784C.pdf


/**
 * @todo    - Add config functionality
 *          - Move commands to progmem
 *          - Add intelligent way of utilising device eeprom (w/ mac save)
 *          - Is there any special low power mode?
 *          - Save stuff into EEPROM
 */

#ifndef OTRN2483LINK_OTRN2483LINK_H_
#define OTRN2483LINK_OTRN2483LINK_H_

#include <Arduino.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <OTRadioLink.h>
#include <OTV0p2Base.h>
#include <string.h>
#include <stdint.h>

namespace OTRN2483Link
{

/**
 * @struct  OTRN2483LinkConfig
 * @brief   Structure containing config data for OTRN2483LinkConfig
 * @todo    Work out other required variables for LoRa
 */
typedef struct OTRN2483LinkConfig {
    // Is in eeprom?
    const bool bEEPROM;
    const void * const UDP_Address;
    const void * const UDP_Port;

    OTRN2483LinkConfig(bool e, const void *p, const void *a, const void *ua, const void *up)
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
class OTRN2483Link : public OTRadioLink::OTRadioLink
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
    void poll();
    bool handleInterruptSimple() { return true;};

    /**
     * @brief   Unused. For compatibility with OTRadioLink.
     */
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


    static const char SYS_START[5];
    static const char SYS_RESET[6]; // todo this can be removed on board with working reset line

    static const char MAC_START[5];
    static const char MAC_DEVADDR[9];
    static const char MAC_APPSKEY[9];
    static const char MAC_NWKSKEY[9];
    static const char MAC_ADR_OFF[8];
    static const char MAC_JOINABP[9];
    static const char MAC_STATUS[7];
    static const char MAC_SEND[12];		// Sends an unconfirmed packet on channel 1
    static const char MAC_SAVE[5];

    static const char RN2483_SET[5];
    static const char RN2483_GET[5];
    static const char RN2483_END[3];

    /**
     * @brief   Unused. For compatibility with OTRadioLink.
     */
    void _dolisten() {};

};



} // namespace OTRN2483Link
#endif /* OTRN2483LINK_OTRN2483LINK_H_ */
