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
*/

/*
 * OTRadValve TempControl tests.
 */

#include <gtest/gtest.h>
#include <stdio.h>

#include "OTSIM900Link.h"


// Test for general sanity of OTSIM900Link.
// Make sure that an instance can be created and does not die horribly.
// Underlying simulated serial/SIM900 never accepts data or responds, eg like a dead card.
TEST(OTSIM900Link,basicsDeadCard)
{
    const bool verbose = false;

    class NULLSerialStream final : public Stream
      {
      public:
        void begin(unsigned long) { }
        void begin(unsigned long, uint8_t);
        void end();

        virtual size_t write(uint8_t c) override { if(verbose) { fprintf(stderr, "%c\n", (char) c); } return(0); }
        virtual int available() override { return(-1); }
        virtual int read() override { return(-1); }
        virtual int peek() override { return(-1); }
        virtual void flush() override { }
      };
    OTSIM900Link::OTSIM900Link<0, 0, 0, NULLSerialStream> l0;
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());
    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 10000; ++i) { l0.poll(); }
    EXPECT_GE(OTSIM900Link::START_UP, l0._getState()) << "should keep trying to start with GET_STATE, RETRY_GET_STATE and START_UP";
    // ...
    l0.end();
}

// Test for general sanity of OTSIM900Link.
// Make sure that an instance can be created and does not die horribly.
// Underlying simulated serial/SIM900 accepts output, does not respond.
TEST(OTSIM900Link,basics)
{
    const bool verbose = true;

    class SerialStream final : public Stream
      {
      public:
        void begin(unsigned long) { }
        void begin(unsigned long, uint8_t);
        void end();

        virtual size_t write(uint8_t c) override { if(verbose) { fprintf(stderr, "%c\n", (char) c); } return(1); }
        virtual int available() override { return(-1); }
        virtual int read() override { return(-1); }
        virtual int peek() override { return(-1); }
        virtual void flush() override { }
      };
    OTSIM900Link::OTSIM900Link<0, 0, 0, SerialStream> l0;
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());
    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 10000; ++i) { l0.poll(); }
    EXPECT_GE(OTSIM900Link::START_UP, l0._getState()) << "should keep trying to start with GET_STATE, RETRY_GET_STATE and START_UP";
    // ...
    l0.end();
}


