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

// CLI support routines

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_CLI_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_CLI_H_

#include <stdint.h>

namespace OTV0P2BASE {


/**
 * @brief   Convert a single hex character into a 4 bit nibble
 * @param   ascii character between 0-9, a-f or A-F
 * @retval  nibble containing a value between 0 and 15
 * @todo    - way of doing this without all the branches?
 *          - error checking!!!
 *          - lower case characters!!!
 */
static inline uint8_t parseHexVal(const uint8_t value)
{
  uint8_t myByte = value;

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
 * @brief   Convert 2 hex characters into a a binary value
 * @param   pointer to a token containing characters between 0-9, a-f or A-F
 * @retval  byte containing converted value
 */
uint8_t parseHex(const uint8_t *tok);


}



#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_CLI_H_ */
