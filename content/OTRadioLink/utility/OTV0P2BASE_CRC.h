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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
 * Specialist simple CRC support.
 */

#ifndef ARDUINO_LIB_OTV0P2BASE_CRC_H
#define ARDUINO_LIB_OTV0P2BASE_CRC_H

#include <stdint.h>

// Use namespaces to help avoid collisions.
namespace OTV0P2BASE
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


// Note on CRCs
// ============
// See http://users.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
// Also: http://users.ece.cmu.edu/~koopman/crc/
// Also: http://www.ross.net/crc/crcpaper.html
// Also: http://en.wikipedia.org/wiki/Cyclic_redundancy_check
// 8-bit CRCs available in AVR (HD = Hamming distance):
//     Nickname | Within 1% of bound | Within 2x of bound | Same HD, but more than 2x bound | Worse HD than bound
// _crc8_ccitt_update() polynomial x^8 + x^2 + x + 1     (0x07/0xE0/0x83) aka ATM-8  | 53-119 | 18-52 | 10-17; 248-2048 | 8-9; 120-247 (not in Arduino 1.0.5)
// _crc_ibutton_update() polynomial: x^8 + x^5 + x^4 + 1 (0x31/0x8C/0x98) aka DOWCRC | 43-119 | 19-42 | 10-18; 248-2048 | 8-9; 120-247
// Provided:
// crc8_C2_update()                                      (..../..../0x97) aka C2     | 27-50; 52; 56-119 | 18-26; 51; 53-55 | 10-17; 248-2048 | 8-9; 120-247

// An implication is that for a 2-byte or 3-byte (16/24bit) message body
// either _crc8_ccitt_update() or _crc_ibutton_update() is as good as can be done
// which means that the supplied optimised implementations are probably good choices.


#endif
