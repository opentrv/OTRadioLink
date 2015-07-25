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

#include "OTRadioLink_OTRadioLink.h"

#include <Arduino.h>
#include <Print.h>


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

    // Helper routine to dump data frame to Serial in human- and machine- readable format.
    // As per printRXMsg() but to Serial,
    // which has to be set up and running for this to work.
    void dumpRXMsg(const uint8_t *buf, const uint8_t len) { printRXMsg(&Serial, buf, len); }
    }


