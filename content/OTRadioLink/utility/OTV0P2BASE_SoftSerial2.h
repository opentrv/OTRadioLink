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

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_

#include <stdint.h>
#include "Arduino.h"
#include <Stream.h>
#include <OTV0p2Base.h>

namespace OTV0P2BASE
{

/**
 * @class   OTSoftSerial2
 * @brief   Software serial with optional blocking read and settable interrupt pins.
 *          Extends Stream.h from the Arduino core libraries.
 */
class OTSoftSerial2 : public Stream
{
private:
    static const uint8_t rxPin;
    static const uint8_t txPin;

public:
    OTSoftSerial2(uint8_t rxPin, uint8_t txPin);

    void begin(long speed);
    bool listen();
    void end();
    bool isListening();
    bool stopListening();
    bool overflow();
    int peek();

    virtual size_t write(uint8_t byte);
    virtual int read();
    virtual int available();
    virtual void flush();
    operator bool() { return true; }

    using Print::write;
};


}

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_SOFTSERIAL2_H_ */
