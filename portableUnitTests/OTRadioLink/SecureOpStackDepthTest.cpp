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

#include <OTV0p2Base.h>
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

TEST(SecureOpStackDepth, StackCheckerWorks)
{
    // Set up stack usage checks
    OTV0P2BASE::RAMEND = OTV0P2BASE::getSP();
    OTV0P2BASE::MemoryChecks::resetMinSP();
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
    const size_t baseStack = OTV0P2BASE::MemoryChecks::getMinSP();
    EXPECT_NE((size_t)0, baseStack);
}

namespace SOSDT{
// write dummy implemtation of SimpleSecureFrame32or0BodyRXBase for non AVR and write tests.
class SimpleSecureFrame32or0BodyRXFixedCounter final : public OTRadioLink::SimpleSecureFrame32or0BodyRXBase
{
private:
    constexpr SimpleSecureFrame32or0BodyRXFixedCounter() { }
    virtual int8_t _getNextMatchingNodeID(const uint8_t index, const OTRadioLink::SecurableFrameHeader *const /* sfh */, uint8_t *nodeID) const override
    {
        *nodeID = 0;
        return (index + 1);
    }
public:
    static SimpleSecureFrame32or0BodyRXFixedCounter &getInstance()
    {
        // Lazily create/initialise singleton on first use, NOT statically.
        static SimpleSecureFrame32or0BodyRXFixedCounter instance;
        return(instance);
    }

    // Read current (last-authenticated) RX message count for specified node, or return false if failed.
    // Will fail for invalid node ID and for unrecoverable memory corruption.
    // Both args must be non-NULL, with counter pointing to enough space to copy the message counter value to.
    virtual bool getLastRXMessageCounter(const uint8_t * const /*ID*/, uint8_t * counter) const override
    {
        *counter = 0;
        return (true);
    }
    // // Check message counter for given ID, ie that it is high enough to be eligible for authenticating/processing.
    // // ID is full (8-byte) node ID; counter is full (6-byte) counter.
    // // Returns false if this counter value is not higher than the last received authenticated value.
    // virtual bool validateRXMessageCount(const uint8_t *ID, const uint8_t *counter) const override { return (true); }
    // Update persistent message counter for received frame AFTER successful authentication.
    // ID is full (8-byte) node ID; counter is full (6-byte) counter.
    // Returns false on failure, eg if message counter is not higher than the previous value for this node.
    // The implementation should allow several years of life typical message rates (see above).
    // The implementation should be robust in the face of power failures / reboots, accidental or malicious,
    // not allowing replays nor other cryptographic attacks, nor forcing node dissociation.
    // Must only be called once the RXed message has passed authentication.
    virtual bool updateRXMessageCountAfterAuthentication(const uint8_t * /*ID*/, const uint8_t * /*newCounterValue*/) override
    {
        return (true);
    }
};
// SimpleSecureFrame32or0BodyRXFixedCounter sfrx;

}

TEST(SecureOpStackDepth, SimpleSecureFrame32or0BodyRXFixedCounterBasic)
{
    const uint8_t counter = 0;
    const uint8_t id = 0;
    SOSDT::SimpleSecureFrame32or0BodyRXFixedCounter &sfrx = SOSDT::SimpleSecureFrame32or0BodyRXFixedCounter::getInstance();
    EXPECT_TRUE(sfrx.updateRXMessageCountAfterAuthentication(&id, &counter));
}

#endif // ARDUINO_LIB_OTAESGCM
