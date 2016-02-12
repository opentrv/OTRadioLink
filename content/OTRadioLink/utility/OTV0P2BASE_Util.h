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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2016
                           Deniz Erbilgin 2016
*/

/*
 Base sensor type for simple sensors returning scalar values.

 Most sensors should derive from this.

 May also be used for pseudo-sensors
 such as synthesised from multiple sensors combined.
 */

#ifndef OTV0P2BASE_UTIL_H
#define OTV0P2BASE_UTIL_H

#include <stdint.h>
#include <stddef.h>


namespace OTV0P2BASE
{


// Templated function versions of min/max that do not evaluate the arguments twice.
template <class T> const T& fnmin(const T& a, const T& b) { return((a>b)?b:a); }
template <class T> const T& fnmax(const T& a, const T& b) { return((a<b)?b:a); }


// Extract ASCII hex digit in range [0-9][a-f] (ie lowercase) from bottom 4 bits of argument.
// Eg, passing in 0xa (10) returns 'a'.
// The top 4 bits are ignored.
inline char hexDigit(const uint8_t value) { const uint8_t v = 0xf&value; if(v<10) { return('0'+v); } return('a'+(v-10)); }
//static inline char hexDigit(const uint8_t value) { const uint8_t v = *("0123456789abcdef" + (0xf&value)); }
// Fill in the first two bytes of buf with the ASCII hex digits of the value passed.
// Eg, passing in a value 0x4e sets buf[0] to '4' and buf[1] to 'e'.
inline void hexDigits(const uint8_t value, char * const buf) { buf[0] = hexDigit(value>>4); buf[1] = hexDigit(value); }


/**
 * @brief   Convert a single hex character into a 4 bit nibble
 * @param   ascii character between 0-9, a-f or A-F
 * @retval  nibble containing a value between 0 and 15
 * @todo    - way of doing this without all the branches?
 *          - error checking!!!
 *          - lower case characters!!!
 */
static inline uint8_t parseHexVal(const uint8_t hexchar)
{
  uint8_t myByte = hexchar;

  if(myByte >= 'A') {
    myByte -= 'A';
    myByte += 10;
    return myByte;
  } else if (myByte >= '0') {
    myByte -= '0';
    return myByte;
  } else return 0;
}

/**
 * @brief   Convert 2 hex characters (eg "0a") into a binary value
 * @param   pointer to a token containing characters between 0-9, a-f or A-F
 * @retval  byte containing converted value
 * @todo    write good comments
 *          add checks
 */
uint8_t parseHex(const uint8_t *tok);


}

#endif
