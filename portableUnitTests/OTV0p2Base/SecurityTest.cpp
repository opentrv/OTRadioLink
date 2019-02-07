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
 * Unit tests for secure frame infrastructure, e.g. getNextMatchingNodeID
 */

// Only enable these tests if the OTAESGCM library is marked as available.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
// #include <stdio.h>
#include <stdlib.h>

// #include <OTAESGCM.h>
#include <OTV0p2Base.h>


// Test that MockNodeID compiles.
TEST(MockNodeID, basicCompilation)
{
    OTV0P2BASE::NodeAssociationTableMock nodes;
}


// Test that MockNodeID fails when passed nullptrs or the index is out of range.
TEST(MockNodeID, FailIfInvalidInputs)
{
    OTV0P2BASE::NodeAssociationTableMock nodes;

    uint8_t buf[nodes.idLength] = {};

    // Check some invalid indexes fail. 
    const auto isSet1 = nodes.set(8, buf);
    EXPECT_FALSE(isSet1);
    const auto isSet2 = nodes.set(255, buf);
    EXPECT_FALSE(isSet2);

    // Check nullptrs fail.
    const auto isSet3 = nodes.set(0, nullptr);
    EXPECT_FALSE(isSet3);

    // Check some invalid indexes fail. 
    nodes.get(8, buf);
    EXPECT_EQ(0, buf[0]);
    nodes.get(255, buf);
    EXPECT_EQ(0, buf[0]);

    // Check nullptrs fail.
    nodes.get(0, nullptr);
    EXPECT_EQ(0, buf[0]);
}


// Test that MockNodeID correctly sets nodeIDs.
TEST(MockNodeID, ModifyAndReturnAssociations)
{
    OTV0P2BASE::NodeAssociationTableMock nodes;

    for (auto i = 0U; i != nodes.maxSets; ++i) {
        uint8_t buf[nodes.idLength] = {};
        buf[0] = i;
        const auto isSet1 = nodes.set(i, buf);
        EXPECT_TRUE(isSet1);
    }

    uint8_t buf[nodes.idLength] = {};

    for (auto i = 0U; i != nodes.maxSets; ++i) {
        nodes.get(i, buf);
        EXPECT_EQ(buf[0], i);
        EXPECT_EQ(buf[1], 0);
        EXPECT_EQ(buf[2], 0);
        EXPECT_EQ(buf[3], 0);
        EXPECT_EQ(buf[4], 0);
        EXPECT_EQ(buf[5], 0);
        EXPECT_EQ(buf[6], 0);
        EXPECT_EQ(buf[7], 0);
    }
}


namespace GNMNID
{
OTV0P2BASE::NodeAssociationTableMock nodes;
int8_t getNextMatchingNodeID(uint8_t _index, const uint8_t *prefix, uint8_t prefixLen, uint8_t *nodeID)
{
    return (OTV0P2BASE::getNextMatchingNodeIDGeneric<decltype(GNMNID::nodes), GNMNID::nodes>(_index, prefix, prefixLen, nodeID));
}
}


// Test that MockNodeID fails when:
// - passed nullptrs
// - the index out of range.
// - The prefix length is out of range.
TEST(getNextMatchingNodeID, FailIfInvalidInputs)
{
    GNMNID::nodes._reset();
    uint8_t prefix[GNMNID::nodes.idLength] = {};
    uint8_t buf[GNMNID::nodes.idLength] = {};

    // Test invalid index
    const auto r0 = GNMNID::getNextMatchingNodeID(8, prefix, sizeof(prefix), buf);
    EXPECT_EQ(-1, r0);
    const auto r1 = GNMNID::getNextMatchingNodeID(255, prefix, sizeof(prefix), buf);
    EXPECT_EQ(-1, r1);

    // Test invalid prefixes
    // This can only be null if lenPrefix == 0.
    const auto r2 = GNMNID::getNextMatchingNodeID(0, nullptr, sizeof(prefix), buf);
    EXPECT_EQ(-1, r2);
    const auto r3 = GNMNID::getNextMatchingNodeID(0, nullptr, 1, buf);
    EXPECT_EQ(-1, r3);
    const auto r4 = GNMNID::getNextMatchingNodeID(0, nullptr, 255, buf);
    EXPECT_EQ(-1, r4);
}


// Test that getNextMatchingNodeID fails if no IDs match.
TEST(getNextMatchingNodeID, FailIfNoMatch)
{
    GNMNID::nodes._reset();
    uint8_t inbuf[GNMNID::nodes.idLength] = {};
    uint8_t prefix[GNMNID::nodes.idLength] = { 1, 1, 1, 1, 1, 1, 1, 1};
    uint8_t outbuf[GNMNID::nodes.idLength] = {};

    // Test with no entries in table.
    const auto r0 = GNMNID::getNextMatchingNodeID(8, prefix, sizeof(prefix), outbuf);
    EXPECT_EQ(-1, r0);

    // Set IDs and test again.
    inbuf[0] = 1;
    GNMNID::nodes.set(0, inbuf);
    const auto r1 = GNMNID::getNextMatchingNodeID(8, prefix, sizeof(prefix), outbuf);
    EXPECT_EQ(-1, r1);

    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[0] = i;
        const auto isSet1 = GNMNID::nodes.set(i, buf);
        EXPECT_TRUE(isSet1);
    }
    const auto r2 = GNMNID::getNextMatchingNodeID(8, prefix, sizeof(prefix), outbuf);
    EXPECT_EQ(-1, r2);
}


// Test that getNextMatchingNodeID returns the correct ID when passed a full prefix.
TEST(getNextMatchingNodeID, ReturnCorrectIDWithUniqueFirstByte)
{
    GNMNID::nodes._reset();
    const std::array<uint8_t, GNMNID::nodes.idLength> id0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id1 = { 1, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id2 = { 2, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id3 = { 3, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id4 = { 4, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id5 = { 5, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id6 = { 6, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id7 = { 7, 0, 0, 0, 0, 0, 0, 0 };
    std::array<uint8_t, GNMNID::nodes.idLength> outbuf = {};

    // Set IDs
    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[0] = i;
        const auto isSet = GNMNID::nodes.set(i, buf);
        ASSERT_TRUE(isSet);
    }

    // Check IDs copied correctly
    for (auto& x: outbuf) { x = 0xff; }
    const auto i0 = GNMNID::getNextMatchingNodeID(0, id0.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(0, i0);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id0));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i1 = GNMNID::getNextMatchingNodeID(0, id1.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(1, i1);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id1));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i2 = GNMNID::getNextMatchingNodeID(0, id2.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(2, i2);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id2));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i3 = GNMNID::getNextMatchingNodeID(0, id3.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(3, i3);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id3));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i4 = GNMNID::getNextMatchingNodeID(0, id4.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(4, i4);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id4));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i5 = GNMNID::getNextMatchingNodeID(0, id5.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(5, i5);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id5));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i6 = GNMNID::getNextMatchingNodeID(0, id6.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(6, i6);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id6));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i7 = GNMNID::getNextMatchingNodeID(0, id7.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(7, i7);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id7));
}


// Test that getNextMatchingNodeID returns the correct ID when passed a full prefix.
TEST(getNextMatchingNodeID, ReturnCorrectIDWithSameFirstByte)
{
    GNMNID::nodes._reset();
    const std::array<uint8_t, GNMNID::nodes.idLength> id0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id1 = { 0, 0, 0, 0, 0, 0, 0, 1 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id2 = { 0, 0, 0, 0, 0, 0, 0, 2 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id3 = { 0, 0, 0, 0, 0, 0, 0, 3 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id4 = { 0, 0, 0, 0, 0, 0, 0, 4 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id5 = { 0, 0, 0, 0, 0, 0, 0, 5 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id6 = { 0, 0, 0, 0, 0, 0, 0, 6 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id7 = { 0, 0, 0, 0, 0, 0, 0, 7 };
    std::array<uint8_t, GNMNID::nodes.idLength> outbuf = {};

    // Set IDs
    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[7] = i;
        const auto isSet = GNMNID::nodes.set(i, buf);
        EXPECT_TRUE(isSet);
    }

    // Check IDs copied correctly
    for (auto& x: outbuf) { x = 0xff; }
    const auto i0 = GNMNID::getNextMatchingNodeID(0, id0.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(0, i0);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id0));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i1 = GNMNID::getNextMatchingNodeID(0, id1.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(1, i1);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id1));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i2 = GNMNID::getNextMatchingNodeID(0, id2.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(2, i2);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id2));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i3 = GNMNID::getNextMatchingNodeID(0, id3.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(3, i3);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id3));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i4 = GNMNID::getNextMatchingNodeID(0, id4.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(4, i4);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id4));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i5 = GNMNID::getNextMatchingNodeID(0, id5.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(5, i5);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id5));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i6 = GNMNID::getNextMatchingNodeID(0, id6.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(6, i6);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id6));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i7 = GNMNID::getNextMatchingNodeID(0, id7.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(7, i7);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id7));
}


// Test that getNextMatchingNodeID exits with an error if the first byte of an id is 0xff.
TEST(getNextMatchingNodeID, FailOnFirstInvalidID)
{
    GNMNID::nodes._reset();
    const std::array<uint8_t, GNMNID::nodes.idLength> id0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id1 = { 0, 1, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id2 = { 0, 2, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id3 = { 0, 3, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id4 = { 0, 4, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id5 = { 0, 5, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id6 = { 0, 6, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id7 = { 0, 7, 0, 0, 0, 0, 0, 0 };
    std::array<uint8_t, GNMNID::nodes.idLength> outbuf = {};

    // Set IDs
    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[1] = i;
        const auto isSet = GNMNID::nodes.set(i, buf);
        EXPECT_TRUE(isSet);
    }
    const uint8_t invalidID[GNMNID::nodes.idLength] = { 0xff, 0, 0, 0, 0, 0, 0, 0 };
    const auto isSet = GNMNID::nodes.set(4, invalidID);
    EXPECT_TRUE(isSet);

    // Check IDs copied correctly
    for (auto& x: outbuf) { x = 0xff; }
    const auto i0 = GNMNID::getNextMatchingNodeID(0, id0.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(0, i0);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id0));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i1 = GNMNID::getNextMatchingNodeID(0, id1.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(1, i1);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id1));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i2 = GNMNID::getNextMatchingNodeID(0, id2.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(2, i2);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id2));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i3 = GNMNID::getNextMatchingNodeID(0, id3.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(3, i3);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id3));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i4 = GNMNID::getNextMatchingNodeID(0, id4.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(-1, i4);
    EXPECT_THAT(outbuf, ::testing::Each(0xff));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i5 = GNMNID::getNextMatchingNodeID(0, id5.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(-1, i5);
    EXPECT_THAT(outbuf, ::testing::Each(0xff));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i6 = GNMNID::getNextMatchingNodeID(0, id6.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(-1, i6);
    EXPECT_THAT(outbuf, ::testing::Each(0xff));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i7 = GNMNID::getNextMatchingNodeID(0, id7.begin(), GNMNID::nodes.idLength, outbuf.begin());
    EXPECT_EQ(-1, i7);
    EXPECT_THAT(outbuf, ::testing::Each(0xff));
}


// Test that getNextMatchingNodeID returns the correct index, but does not
// attempt to copy the ID if the nodeID pointer is a nullptr.
// FIXME: This test effectively depends on the program quitting with a SIGSEGV
// on failure. Is there a better way of dealing this?
TEST(getNextMatchingNodeID, DontCopyIDToNullIDBuffer)
{
    GNMNID::nodes._reset();
    const std::array<uint8_t, GNMNID::nodes.idLength> id0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id1 = { 1, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id2 = { 2, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id3 = { 3, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id4 = { 4, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id5 = { 5, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id6 = { 6, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id7 = { 7, 0, 0, 0, 0, 0, 0, 0 };

    // Set IDs
    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[0] = i;
        const auto isSet = GNMNID::nodes.set(i, buf);
        ASSERT_TRUE(isSet);
    }

    // Check IDs copied correctly
    const auto i0 = GNMNID::getNextMatchingNodeID(0, id0.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(0, i0);
    const auto i1 = GNMNID::getNextMatchingNodeID(0, id1.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(1, i1);
    const auto i2 = GNMNID::getNextMatchingNodeID(0, id2.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(2, i2);
    const auto i3 = GNMNID::getNextMatchingNodeID(0, id3.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(3, i3);
    const auto i4 = GNMNID::getNextMatchingNodeID(0, id4.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(4, i4);
    const auto i5 = GNMNID::getNextMatchingNodeID(0, id5.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(5, i5);
    const auto i6 = GNMNID::getNextMatchingNodeID(0, id6.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(6, i6);
    const auto i7 = GNMNID::getNextMatchingNodeID(0, id7.begin(), GNMNID::nodes.idLength, nullptr);
    EXPECT_EQ(7, i7);
}


// Test that getNextMatchingNodeID returns the ID at the index passed to it if 
// no prefix is passed in.
TEST(getNextMatchingNodeID, AlwaysMatchIfNoPrefix)
{
    GNMNID::nodes._reset();
    const std::array<uint8_t, GNMNID::nodes.idLength> id0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id1 = { 0, 0, 0, 0, 0, 0, 0, 1 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id2 = { 0, 0, 0, 0, 0, 0, 0, 2 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id3 = { 0, 0, 0, 0, 0, 0, 0, 3 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id4 = { 0, 0, 0, 0, 0, 0, 0, 4 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id5 = { 0, 0, 0, 0, 0, 0, 0, 5 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id6 = { 0, 0, 0, 0, 0, 0, 0, 6 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id7 = { 0, 0, 0, 0, 0, 0, 0, 7 };
    std::array<uint8_t, GNMNID::nodes.idLength> outbuf = {};

    // Set IDs
    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[7] = i;
        const auto isSet = GNMNID::nodes.set(i, buf);
        EXPECT_TRUE(isSet);
    }

    // Check IDs copied correctly
    for (auto& x: outbuf) { x = 0xff; }
    const auto i0 = GNMNID::getNextMatchingNodeID(0, nullptr, 0, outbuf.begin());
    EXPECT_EQ(0, i0);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id0));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i1 = GNMNID::getNextMatchingNodeID(1, nullptr, 0, outbuf.begin());
    EXPECT_EQ(1, i1);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id1));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i2 = GNMNID::getNextMatchingNodeID(2, nullptr, 0, outbuf.begin());
    EXPECT_EQ(2, i2);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id2));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i3 = GNMNID::getNextMatchingNodeID(3, nullptr, 0, outbuf.begin());
    EXPECT_EQ(3, i3);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id3));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i4 = GNMNID::getNextMatchingNodeID(4, nullptr, 0, outbuf.begin());
    EXPECT_EQ(4, i4);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id4));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i5 = GNMNID::getNextMatchingNodeID(5, nullptr, 0, outbuf.begin());
    EXPECT_EQ(5, i5);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id5));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i6 = GNMNID::getNextMatchingNodeID(6, nullptr, 0, outbuf.begin());
    EXPECT_EQ(6, i6);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id6));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i7 = GNMNID::getNextMatchingNodeID(7, nullptr, 0, outbuf.begin());
    EXPECT_EQ(7, i7);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id7));
}


// Test that getNextMatchingNodeID returns the correct ID if only a partial
// prefix is passed in.
TEST(getNextMatchingNodeID, ReturnCorrectIDWithPartialPrefix)
{
    GNMNID::nodes._reset();
    const std::array<uint8_t, GNMNID::nodes.idLength> id0 = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id1 = { 0, 0, 0, 1, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id2 = { 0, 0, 0, 2, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id3 = { 0, 0, 0, 3, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id4 = { 0, 0, 0, 4, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id5 = { 0, 0, 0, 5, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id6 = { 0, 0, 0, 6, 0, 0, 0, 0 };
    const std::array<uint8_t, GNMNID::nodes.idLength> id7 = { 0, 0, 0, 7, 0, 0, 0, 0 };
    std::array<uint8_t, GNMNID::nodes.idLength> outbuf = {};

    // Set IDs
    for (auto i = 0U; i != GNMNID::nodes.maxSets; ++i) {
        uint8_t buf[GNMNID::nodes.idLength] = {};
        buf[3] = i;
        const auto isSet = GNMNID::nodes.set(i, buf);
        ASSERT_TRUE(isSet);
    }

    // Check IDs copied correctly
    for (auto& x: outbuf) { x = 0xff; }
    const auto i0 = GNMNID::getNextMatchingNodeID(0, id0.begin(), 4, outbuf.begin());
    EXPECT_EQ(0, i0);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id0));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i1 = GNMNID::getNextMatchingNodeID(0, id1.begin(), 4, outbuf.begin());
    EXPECT_EQ(1, i1);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id1));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i2 = GNMNID::getNextMatchingNodeID(0, id2.begin(), 4, outbuf.begin());
    EXPECT_EQ(2, i2);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id2));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i3 = GNMNID::getNextMatchingNodeID(0, id3.begin(), 4, outbuf.begin());
    EXPECT_EQ(3, i3);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id3));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i4 = GNMNID::getNextMatchingNodeID(0, id4.begin(), 4, outbuf.begin());
    EXPECT_EQ(4, i4);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id4));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i5 = GNMNID::getNextMatchingNodeID(0, id5.begin(), 4, outbuf.begin());
    EXPECT_EQ(5, i5);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id5));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i6 = GNMNID::getNextMatchingNodeID(0, id6.begin(), 4, outbuf.begin());
    EXPECT_EQ(6, i6);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id6));
    for (auto& x: outbuf) { x = 0xff; }
    const auto i7 = GNMNID::getNextMatchingNodeID(0, id7.begin(), 4, outbuf.begin());
    EXPECT_EQ(7, i7);
    EXPECT_THAT(outbuf, ::testing::ElementsAreArray(id7));
}
