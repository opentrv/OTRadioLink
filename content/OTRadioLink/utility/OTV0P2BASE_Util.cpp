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
 Simple utilities.
 */

#include "OTV0P2BASE_Util.h"

namespace OTV0P2BASE {


/**
 * @brief   Convert 1-2 hex character string (eg "0a") into a binary value
 * @param   pointer to a token containing characters between 0-9, a-f or A-F
 * @retval  byte containing converted value [0,255]; -1 in case of error
 */
int parseHexByte(const char *const s)
    {
    if(NULL == s) { return(-1); } // ERROR
    const char c0 = s[0];
    if('\0' == c0) { return(-1); } // ERROR
    const int8_t d0 = parseHexDigit(c0);
    if(-1 == d0) { return(-1); } // ERROR
    // If input string only one character, treat as low nybble, eg "a" => 10.
    const char c1 = s[1];
    if('\0' == c1) { return(d0); }
    const int8_t d1 = parseHexDigit(c1);
    if(-1 == d1) { return(-1); } // ERROR
    return((d0 << 4) | d1);
    }


}
