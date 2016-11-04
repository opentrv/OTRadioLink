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
 * OTRadValve SIM900 tests.
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include "OTSIM900Link.h"

// Test the getter function definitely does what it should.
TEST(OTSIM900Link, getterFunction)
{
    const char SIM900_PIN[] = "1111";
    const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, NULL, NULL, NULL);
    EXPECT_EQ(SIM900_PIN[0], SIM900Config.get((const uint8_t *)SIM900Config.PIN));
}

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
    const char SIM900_PIN[] = "1111";
    const char SIM900_APN[] = "apn";
    const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
    const char SIM900_UDP_PORT[] = "9999";
    const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
    const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);
    OTSIM900Link::OTSIM900Link<0, 0, 0, NULLSerialStream> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());
    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 100; ++i) { l0.poll(); }
    EXPECT_GE(OTSIM900Link::START_UP, l0._getState()) << "should keep trying to start with GET_STATE, RETRY_GET_STATE";
    // ...
    l0.end();
}

// Walk through state space of OTSIM900Link.
// Make sure that an instance can be created and does not die horribly.
// Is meant to mainly walk through all the normal expected SIM900 behaviour when all is well.
// Other tests can look at error handling including unexpected/garbage responses.
namespace B1
{
const bool verbose = false;

// Does a simple simulation of SIM900, responding sensibly to all commands needed by the OTSIM900Link impl.
// Allows for exercise of every major non-PANIC state of the OTSIM900Link implementation.
class GoodSimulator final : public Stream
  {
  public:
    // Events exposed.
    static bool haveSeenCommandStart;

  private:
    // Command being collected from OTSIM900Link.
    bool waitingForCommand = true;
    bool collectingCommand = false;
    // Entire request starting "AT"; no trailing CR or LF stored.
    std::string command;

    // Reply (postfix) being returned to OTSIM900Link: empty if none.
    std::string reply;

    // Keep track (crudely) of state. Corresponds to OTSIM900LinkState values.
    uint8_t sim900LinkState = 0;

  public:
    void begin(unsigned long) { }
    void begin(unsigned long, uint8_t);
    void end();

    virtual size_t write(uint8_t uc) override
      {
      const char c = (char)uc;
      if(waitingForCommand)
        {
        // Look for leading 'A' of 'AT' to start a command.
        if('A' == c)
          {
          waitingForCommand = false;
          collectingCommand = true;
          command = 'A';
          haveSeenCommandStart = true; // Note at least one command start.
          }
        }
      else
        {
        // Look for CR (or LF) to terminate a command.
        if(('\r' == c) || ('\n' == c))
          {
          waitingForCommand = true;
          collectingCommand = false;
          if(verbose) { fprintf(stderr, "command received: %s\n", command.c_str()); }
          // Respond to particular commands...
          if("AT" == command) { // Relevant states: GET_STATE, RETRY_GET_STATE, START_UP
              if(sim900LinkState == OTSIM900Link::GET_STATE) {
                  reply = "vfd";  // garbage to force into RETRY_GET_STATE
                  sim900LinkState = OTSIM900Link::RETRY_GET_STATE;
              } else reply = "AT\r\n\r\nOK\r\n";
          }
          else if("AT+CPIN?" == command) { reply = /* (random() & 1) ? "No PIN\r" : */ "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n"; }  // Relevant states: CHECK_PIN
          else if("AT+CREG?" == command) { reply = /* (random() & 1) ? "+CREG: 0,0\r" : */ "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n"; } // Relevant states: WAIT_FOR_REGISTRATION
          else if("AT+CSTT=apn" == command) { reply =  "AT+CSTT\r\n\r\nOK\r"; } // Relevant states: SET_APN
          else if("AT+CIPSTATUS" == command) {
              switch (sim900LinkState){
                  case OTSIM900Link::RETRY_GET_STATE:  // GPRS inactive)
                      sim900LinkState = OTSIM900Link::START_GPRS;
                      reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n";
                      break;
                  case OTSIM900Link::START_GPRS:          // GPRS is activated.
                      sim900LinkState = OTSIM900Link::GET_IP;
                      reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n";
                      break;
                  case OTSIM900Link::GET_IP:    // UDP connected.
                      reply = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n";
                      break;
                  default: break;
              }
          }  // Relevant states: START_GPRS, WAIT_FOR_UDP
          else if("AT+CIICR" == command) { reply = "AT+CIICR\r\n\r\nOK\r\n"; }  // Relevant states: START_GPRS
          else if("AT+CIFSR" == command) { reply = "AT+CIFSR\r\n\r\n172.16.101.199\r\n"; }  // Relevant States: GET_IP
          else if("AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"" == command) { reply = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n"; }  // Relevant states: OPEN_UDP
          else if("AT+CIPSEND=3" == command) { reply = "AT+CIPSEND=3\r\n\r\n>"; }  // Relevant states:  SENDING
          else if("123" == command) { reply = "123\r\nSEND OK\r\n"; }  // Relevant states: SENDING
          }
        else if(collectingCommand) { command += c; }
        }
      if(verbose) { if(isprint(c)) { fprintf(stderr, "<%c\n", c); } else { fprintf(stderr, "< %d\n", (int)c); } }
      return(1);
      }
    virtual int read() override
        {
        if(0 == reply.size()) { return(-1); }
        const char c = reply[0];
        if(verbose) { if(isprint(c)) { fprintf(stderr, ">%c\n", c); } else { fprintf(stderr, "> %d\n", (int)c); } }
        reply.erase(0, 1);
        return(c);
        }
    virtual int available() override { return(-1); }
    virtual int peek() override { return(-1); }
    virtual void flush() override { }
  };
// Events exposed.
bool GoodSimulator::haveSeenCommandStart;
}
TEST(OTSIM900Link,basicsSimpleSimulator)
{
//    const bool verbose = B1::verbose;

    srandom(::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

    // Vector of bools containing states to check. This covers all states expected in normal use. RESET and PANIC are not covered.
    std::vector<bool> statesChecked(OTSIM900Link::RESET, false);
    // Message to send.
    const char message[] = "123";

    const char SIM900_PIN[] = "1111";
    const char SIM900_APN[] = "apn";
    const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
    const char SIM900_UDP_PORT[] = "9999";
    const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
    const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);


    ASSERT_FALSE(B1::GoodSimulator::haveSeenCommandStart);
    OTSIM900Link::OTSIM900Link<0, 0, 0, B1::GoodSimulator> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());

    // Queue a message to send.
    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 100; ++i) { statesChecked[l0._getState()] = true; l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}
    l0.queueToSend((const uint8_t *)message, (uint8_t)sizeof(message)-1, (int8_t) 0, OTRadioLink::OTRadioLink::TXnormal);
    for(int i = 0; i < 100; ++i) { statesChecked[l0._getState()] = true; l0.poll(); }
    EXPECT_TRUE(B1::GoodSimulator::haveSeenCommandStart) << "should see some attempt to communicate with SIM900";
    for(int i = 0; i < OTSIM900Link::RESET; i++) EXPECT_TRUE(statesChecked[i]) << "state " << i << " not seen.";  // Check what states have been seen.
    // ...
    l0.end();
}


