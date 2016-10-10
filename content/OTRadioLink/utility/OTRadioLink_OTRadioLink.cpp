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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

#include <stdio.h>

#include "OTRadioLink_OTRadioLink.h"

#ifdef ARDUINO_ARCH_ARV
#include <util/atomic.h>
#endif

#ifdef ARDUINO
#include <Arduino.h>
#include <Print.h>
#endif

#include "OTV0P2BASE_Sleep.h"


// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {
    // Helper routine to compute the length of an 0xff-terminated frame,
    // excluding the trailing 0xff.
    // Returns 0 if NULL or unterminated (within 255 bytes).
    uint8_t frameLenFFTerminated(const uint8_t *buf)
        {
        if(NULL == buf) { return(0); } // Possibly should panic() instead.
        uint8_t len = 0;
        while(0xff != *buf++)
            {
            ++len;
            if(0 == len) { return(0); } // Too long/unterminated: possibly should panic() instead.
            }
        return(len);
        }

#ifdef ARDUINO
    // Helper routine to dump data frame to a Print output in human- and machine- readable format.
    // Dumps as pipe (|) then length (in decimal) then space then two characters for each byte:
    // printable characters in range 32--126 are rendered as a space then the character,
    // others are rendered as a two-digit upper-case hex value;
    // the line is terminated with CRLF.
    // eg:
    //     |5 a {  81FD
    // for the 5-byte message 0x61, 0x7b, 0x20, 0x81, 0xfd.
    //
    // Useful for debugging but also for RAD
    // to relay frames without decoding to more powerful host
    // on other end of serial cable.
    //
    // Serial has to be set up and running for this to work.
    void printRXMsg(Print *p, const uint8_t *buf, const uint8_t len)
        {
        p->print('|');
        p->print((int) len);
        p->print(' ');
        for(int i = 0; i < len; ++i)
            {
            const uint8_t b = *buf++;
            if(b < 16) { p->print('0'); p->print((int)b, HEX); }
            else if((b < 32) || (b >= 126)) { p->print((int)b, HEX); }
            else { p->print(' '); p->print((char)b); }
            }
        p->println();
        }
#endif // ARDUINO

    // Helper routine to dump data frame to Serial in human- and machine- readable format.
    // As per printRXMsg() but to Serial,
    // which has to be set up and running for this to work.#ifdef ARDUINO
#ifdef ARDUINO
    void dumpRXMsg(const uint8_t *buf, const uint8_t len) { printRXMsg(&Serial, buf, len); }
#else
    // On non-Arduino platform, dump to stdout.
    void dumpRXMsg(const uint8_t *buf, const uint8_t len) { fwrite(buf, 1, len, stdout); }
#endif // ARDUINO

    // Heuristic filter, especially useful for OOK carrier, to trim (all but first) trailing zeros.
    // Useful to fit more frames into RX queues if frame type is not explicit
    // and (eg with OOK operation) tail of frame buffer is filled with zeros.
    // Leaves first trailing zero for those frame types that may legitimately have one trailing zero.
    // Always returns true, ie never rejects a frame outright.
    bool frameFilterTrailingZeros(const volatile uint8_t *const buf, volatile uint8_t &buflen)
        {
        if(buflen <= 1) { return(true); } // Too short to trim.
        const volatile uint8_t *b = buf + buflen - 1;
        if(0 != *b) { return(true); } // No trailing nulls at all, so don't trim frame size.
        // Check for first non-zero byte from end backwards.
        while(0 == *--b) { if(b == buf) { buflen = 1; return(true); } }
        buflen = (uint8_t)(b - buf + 2);
        return(true);
        }

#ifdef ARDUINO_ARCH_AVR
    // Set (or clear) the optional fast filter for RX ISR/poll; NULL to clear.
    // The routine should return false to drop an inbound frame early in processing,
    // to save queue space and CPU, and cope better with a busy channel.
    void OTRadioLink::setFilterRXISR(quickFrameFilter_t *const filterRX)
        {
        // Lock out interrupts to ensure no partial pointer is seen.
        ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
            { filterRXISR = filterRX; }
        }
#endif // ARDUINO_ARCH_AVR
    }


