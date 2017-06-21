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

Author(s) / Copyright (s): Damon Hart-Davis 2016
                           Deniz Erbilgin 2017
*/

/*
 * OTRadValve tests of secure frame operations dependent on OTASEGCM
 * with a particular view to managing stack depth to avoid overflow
 * especially on very limited RAM platforms such as AVR.
 */

// Only enable these tests if the OTAESGCM library is marked as available.
#if defined(EXT_AVAILABLE_ARDUINO_LIB_OTAESGCM)


#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include <OTAESGCM.h>
#include <OTRadioLink.h>


static const int AES_KEY_SIZE = 128; // in bits
static const int GCM_NONCE_LENGTH = 12; // in bytes
static const int GCM_TAG_LENGTH = 16; // in bytes (default 16, 12 possible)

// All-zeros const 16-byte/128-bit key.
// Can be used for other purposes.
static const uint8_t zeroBlock[16] = { };

// Max stack usage in bytes
// 20170511
//           enc, dec, enc*, dec*
// - DE:     208, 208, 208,  208
// - DHD:    ???, ???, 358,  ???
// - Travis: 192, 224, ???,  ???
// * using a workspace
#ifndef __APPLE__
static constexpr unsigned int maxStackSecureFrameEncode = 328;
static constexpr unsigned int maxStackSecureFrameDecode = 328;
#else
// On DHD's system, secure frame enc/decode uses 358 bytes (20170511)
static constexpr unsigned int maxStackSecureFrameEncode = 416;
static constexpr unsigned int maxStackSecureFrameDecode = 416;
#endif // __APPLE__

TEST(SimpleSecureFrame, StackCheckerWorks)
{
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();
    // Uncomment to print stack usage
    EXPECT_NE((size_t)0, baseStack);
}



#endif // ARDUINO_LIB_OTAESGCM
