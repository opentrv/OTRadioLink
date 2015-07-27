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

/*
 * Specialist simple CRC support.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_CRC_H
#define ARDUINO_LIB_OTRADIOLINK_CRC_H

#include <stdint.h>

// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {
    // Simple small CRC definitions, typically not readily available elsewhere.

    /**Update 7-bit CRC with next byte; result always has top bit zero.
     * Polynomial 0x5B (1011011, Koopman) = (x+1)(x^6 + x^5 + x^3 + x^2 + 1) = 0x37 (0110111, Normal)
     * <p>
     * Should consider initialising with with 0x7f rather than 0.
     * <p>
     * See: http://users.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
     * <p>
     * Should detect all 3-bit errors in up to 7 bytes of payload,
     * see: http://users.ece.cmu.edu/~koopman/crc/0x5b.txt
     * <p>
     * For 2 or 3 byte payloads this should have a Hamming distance of 4 and be within a factor of 2 of optimal error detection.
     */
    extern uint8_t crc7_5B_update(uint8_t crc, uint8_t datum);

    // Value to use in place of 0 for final CRC value, eg for crc7_5B_update_nz_final();
    static const uint8_t crc7_5B_update_nz_ALT = 0x80;

    /**As crc7_5B_update() but if the output would be 0, this returns 0x80 instead.
     * This allows use where 0x00 (and 0xff) is not allowed or preferred,
     * but without weakening the CRC protection (eg all result values are distinct)
     * Use this ONLY on the final byte.
     */
    extern uint8_t crc7_5B_update_nz_final(uint8_t crc, uint8_t datum);
    }


#endif
