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

#include "OTV0P2BASE_CLI.h"

namespace OTV0P2BASE {

/**
 * @brief   Convert 2 hex characters into a a binary value
 * @param   pointer to a token containing characters between 0-9, a-f or A-F
 * @retval  byte containing converted value
 * @todo    write good comments
 *          add checks
 */
uint8_t parseHex(const uint8_t *tok)
{
  uint8_t hiNibble = *tok++;
  uint8_t lowNibble = *tok;

  hiNibble = parseHexVal(hiNibble) << 4;
  hiNibble |= parseHexVal(lowNibble);
  return hiNibble;
}

}
