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

// NOTE!!! Implementation details are in OTV0P2BASE_SoftSerial_NOTES.txt!!!

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_

#include <stdint.h>
#include "Arduino.h"
#include <Stream.h>
#include <OTV0p2Base.h>

namespace OTV0P2BASE
{

static const uint8_t OTSOFTSERIAL2_BUFFER_SIZE = 32;

/**
 * @class   OTSoftSerial2
 * @brief   Software serial with optional blocking read and settable interrupt pins.
 *          Extends Stream.h from the Arduino core libraries.
 */
class OTSoftSerial2 : public Stream
{
protected:
    static const uint16_t timeOut = 60000; // fed into loop...

    const uint8_t rxPin;
    const uint8_t txPin;
    uint8_t fullDelay;
    uint8_t halfDelay;
//    volatile uint8_t rxBufferHead;
//    volatile uint8_t rxBufferTail;
//    uint8_t rxBuffer[OTSOFTSERIAL2_BUFFER_SIZE];

public:
    /**
     * @brief   Constructor for OTSoftSerial2
     * @param   rxPin: Pin to receive from.
     * @param   txPin: Pin to send from.
     */
    OTSoftSerial2(uint8_t rxPin, uint8_t txPin);
    /**
     * @brief   Initialises OTSoftSerial2 and sets up pins.
     * @param   speed: The baud to listen at.
     * @fixme   Long is excessive
     * @todo    what to do about optional stuff.
     */
    void begin(unsigned long speed) { begin(speed, 0); }
    void begin(unsigned long speed, uint8_t);
    /**
     * @brief   Disables serial and releases pins.
     */
    void end();
    /**
     * @brief   Write a byte to serial as a binary value.
     * @param   byte: Byte to write.
     * @retval  Number of bytes written.
     */
    virtual size_t write(uint8_t byte);
    /**
     * @brief   Read next character in the input buffer without removing it.
     * @retval  Next character in input buffer.
     */
    int peek();
    /**
     * @brief   Reads a byte from the serial and removes it from the buffer.
     * @retval  Next character in input buffer.
     */
    virtual int read();
    /**
     * @brief   Get the number of bytes available to read in the input buffer.
     * @retval  The number of bytes available.
     */
    virtual int available();
    /**
     * @brief   Check if serial port is ready for use.
     * @todo    Implement the time checks using this?
     */
    operator bool() { return true; }
    using Print::write; // write(str) and write(buf, size) from Print
    
    /**************************************************************************
     * -------------------------- Non Standard ------------------------------ *
     *************************************************************************/
    void sendBreak();
    /**************************************************************************
     * ------------------------ Unimplemented ------------------------------- *
     *************************************************************************/
    /**
     * @brief   Destuctor for OTSoftSerial.
     * @note    Not implemented to reduce code size.
     */
    //    ~OTSoftSerial2() {};    // TODO Does this actually work?
    /**
     * @brief   Waits for transmission of outgoing serial data to complete.
     * @note    This is not used for OTSoftSerial2 as all writes are synchronous.
     */
    virtual void flush() {}
    /**
     * @brief   Returns the number of elements in the Tx buffer.
     * @retval  0 as no Tx buffer implemented.
     * @note    This is not used for OTSoftSerial2 as all writes are synchronous.
     */
    int availableForWrite() { return 0; }  //

};


}

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_ */
