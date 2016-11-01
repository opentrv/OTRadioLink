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

#include "OTSIM900Link.h"


// Test for general sanity of OTSIM900Link.
// Make sure that an instance can be created and does not die horribly.
TEST(OTSIM900Link,basics)
{
    class NULLSerialStream final : public Stream
      {
      public:
        void begin(unsigned long) { }
        void begin(unsigned long, uint8_t);
        void end();

        virtual size_t write(uint8_t) override { return(0); }
        virtual int available() override { return(-1); }
        virtual int read() override { return(-1); }
        virtual int peek() override { return(-1); }
        virtual void flush() override { }
      };
    OTSIM900Link::OTSIM900Link<0, 0, 0, NULLSerialStream> l0;
    EXPECT_TRUE(l0.begin());
    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 1000; ++i) { l0.poll(); }
    // ...
    l0.end();
}


