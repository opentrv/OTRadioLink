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
                           Deniz Erbilgin 2016
*/

/*
 * OTRadValve SIM900 tests.
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>

#include "OTSIM900Link.h"


/**
 * @brief   dummy callback function to pass a time value to OTSIM900Link
 * @retval  Number of seconds this minute in range [0,59].
 */
static uint_fast8_t secondsVT = 0;
uint_fast8_t getSecondsVT() { return(secondsVT % 60); }
/**
 * @brief   Increment secondsVT by 1 minor cycle.
 */
static constexpr uint_fast8_t minorCycleTimeSecs = 2;
static void incrementVTOneCycle() { secondsVT += minorCycleTimeSecs; }


namespace SIM900Emu {
/**
 * @brief   Simple emulator for stepping through SIM900 states, including fail states.
 * @todo    state machine
 * @todo    emulate time
 */
class SIM900Emulator {
public:
    // constructor
    SIM900Emulator();

    /**
     * @brief   Work through the state machine, replying as appropriate.
     * @param   command:    String containing the command.
     * @param   reply:      String to contain the response. Must be big enough to fit the full response!
     * @todo    add state machine updates
     * @note    APN must be set to "apn" with no quotes to be accepted.
     */
    void _poll(const std::string &command, std::string &reply) {
            // Respond to particular commands when not powered down...
        switch (myState) {
        case POWER_OFF: break;  // do nothing
        case POWERING_UP:
            // send some garbage.
            reply = "vfd";   // garbage when not fully powered. todo replace with random characters
            updateState();
            break;
        case REGISTERING:
            // Wait for some time to pass.
            if(commands.AT == command) { reply = replies.AT; }           // Normal response
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }   // No need to set a PIN.
            else if(commands.CREG == command) { reply = replies.CREG_FALSE; updateState(); }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_FALSE; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_FALSE; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_INITIAL:
            // Need APN to be set. (CSTT=...)
            if(commands.AT == command) { reply = replies.AT; }           // Normal response
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }
            else if(commands.CREG == command) { reply = replies.CREG_READY; }  // Now registered.
            else if(commands.CSTT == command) { reply =  replies.CSTT_READY; updateState(); }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_FALSE; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_FALSE; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_START:
            // Need to start GPRS (CIICR)
            if(commands.AT == command) { reply = replies.AT; }
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }    // Can not set APN again!
            else if(commands.CIICR == command) { reply = replies.CIICR_READY; updateState();}  // Can start GPRS
            else if(commands.CIFSR == command) { reply = replies.CIFSR_FALSE; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_START; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_CONFIGURING:
            // Wait a bit
            updateState();
            break;
        case IP_GPRSACT:
            // Need to check IP address (CIFSR)
            if(commands.AT == command) { reply = replies.AT; }
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }  // GPRS already started...
            else if(commands.CIFSR == command) { reply = replies.CIFSR_READY; updateState(); }  // Check IP address
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_GPRSACT; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_STATUS:
            // Need to open UDP connection
            if(commands.AT == command) { reply = replies.AT; }
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_READY; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_GPRSACT; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_TRUE; updateState(); }  // Open UDP
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case UDP_CONNECTING:
            // Wait a bit
            updateState();
            break;
        case UDP_CONNECT_OK:
            // This should correspond to IDLE. Waiting to send stuff.
            if(commands.AT == command) { reply = replies.AT; }
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_READY; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_CONNECTED; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_TRUE; }
            else if("123" == command) { reply = "123\r\nSEND OK\r\n"; }  // todo this must depend on the cipsend being asked.
            break;
        case UDP_CLOSING:
            // Wait a bit
            updateState();
            break;
        case UDP_CLOSED:
            // Need to open UDP or close GPRS.
            if(commands.AT == command) { reply = replies.AT; }
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_READY; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_START; } // todo check
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_TRUE; updateState();}
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }
            break;
        case PDP_DEACTIVATING:
            if(commands.AT == command) { reply = replies.AT; }           // Normal response
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }   // No need to set a PIN.
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_FALSE; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_PDPDEACT; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            updateState();
            break;
        case PDP_FAIL:
            // Everything has died.
            if(commands.AT == command) { reply = replies.AT; }           // Normal response
            else if(commands.CPIN == command) { reply = replies.CPIN_READY; }   // No need to set a PIN.
            else if(commands.CREG == command) { reply = replies.CREG_READY; }
            else if(commands.CSTT == command) { reply = replies.CSTT_FALSE; }
            else if(commands.CIICR == command) { reply = replies.CIICR_FALSE; }
            else if(commands.CIFSR == command) { reply = replies.CIFSR_FALSE; }
            else if(commands.CIPSTATUS == command) { reply = replies.CIPSTATUS_PDPDEACT; }
            else if(commands.CIPSTART == command) { reply = replies.CIPSTART_FALSE; }
            else if(commands.CIPSEND == command) { reply = replies.CIPSEND_FALSE; }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        default: break;
        }
    }

    // emulate pin toggle:
    void setPinHigh(bool high) {
        /* todo add timing logic */
        if(high) {
            if (myState == POWER_OFF) myState = POWERING_UP;
            else myState = POWER_OFF;
        }
    }
    // Trigger fail states:
    // This triggers a dead-end state caused by signal loss during UDP connection
    void triggerPDPDeactFail() { myState = PDP_FAIL; }
    // This triggers a fail state where the SIM900 carries on responding normally.
    void triggerInvisibleFail() { myState = INVISIBLE_FAIL; }
    /**
     * @brief   Non-exhaustive list of states we go through using OTSIM900Link.
     * @note    States with a verb are transitory and can only be exited by the SIM900.
     */
    enum state_t{
        POWER_OFF,      // SIM900 is powered down.
        POWERING_UP,       // power pin has been toggled, but sim900 not on yet.
        REGISTERING,    // Trying to connect to a cell mast.
        IP_INITIAL,     // Registered to a cell mast.
        IP_START,       // APN set, IP stack started.
        IP_CONFIGURING, // GPRS connection being started.
        IP_GPRSACT,     // GPRS ready.
        IP_STATUS,      // IP address has bee checked.
        UDP_CONNECTING, // Opening UDP connection.
        UDP_CONNECT_OK, // UDP connection ready..
        UDP_CLOSING,
        UDP_CLOSED,     // UDP connection closed but GPRS still active.
        PDP_DEACTIVATING,
        PDP_FAIL,       // Registration lost during GPRS connection. Unrecoverable.
        INVISIBLE_FAIL  // SIM900 responding as normal but not sending. Unrecoverable, undetectable by device. todo take this out of enum.
    } myState = POWER_OFF;

    /**
     * @brief   Work through the states as appropriate.
     * @note    ONLY COVERS NORMAL FLOW! POWER ON OFF ETC DONE USING PUBLIC INTERFACE!
     * @note    Add timing information.
     */
    void updateState() {
        switch (myState) {
        case POWER_OFF:
            // do nothing
            break;
        case POWERING_UP:
            // send some garbage.
            // wait for several seconds to pass.
            // go to REGISTERING
            myState = REGISTERING;
            break;
        case REGISTERING:
            // Wait for some time to pass.
            myState = IP_INITIAL;
            break;
        case IP_INITIAL:
            // Need APN to be set. (CSTT=...)
            myState = IP_START;
            break;
        case IP_START:
            // Need to start GPRS (CIICR)
            myState = IP_CONFIGURING;
            break;
        case IP_CONFIGURING:
            // Wait a bit
            myState = IP_GPRSACT;
            break;
        case IP_GPRSACT:
            // Need to check IP address (CIFSR)
            myState = IP_STATUS;
            break;
        case IP_STATUS:
            // Need to open UDP connection
            myState = UDP_CONNECTING;
            break;
        case UDP_CONNECTING:
            // Wait a bit
            break;
        case UDP_CONNECT_OK:
            // This should correspond to IDLE. Waiting to send stuff.
            myState = UDP_CONNECT_OK;
            break;
        case UDP_CLOSING:
            // Wait a bit
            myState = UDP_CLOSED;
            break;
        case UDP_CLOSED:
            // Need to open UDP or close GPRS.
            myState = UDP_CONNECTING;
            break;
        case PDP_DEACTIVATING:
            myState = IP_INITIAL;
            break;
        case PDP_FAIL:
            // Everything has died.
            break;
        default: break;
        }
    }

    struct SIM900Commands {
        constexpr std::string AT = "AT";
        constexpr std::string CPIN = "AT+CPIN?";
        constexpr std::string CREG = "AT+CREG?";
        constexpr std::string CSTT = "AT+CSTT=apn";
        constexpr std::string CIICR = "AT+CIICR";
        constexpr std::string CIFSR = "AT+CIFSR";
        constexpr std::string CIPSTATUS = "AT+CIPSTATUS";
        constexpr std::string CIPSTART = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"";
        constexpr std::string CIPSEND = "AT+CIPSEND=3";
    } commands;

    struct SIM900Replies {
        constexpr std::string AT = "AT\r\n\r\nOK\r\n";
        constexpr std::string CPIN_READY = "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n";
        constexpr std::string CREG_FALSE = "AT+CREG?\r\n\r\n+CREG: 0,0\r\n\r\n'OK\r\n";
        constexpr std::string CREG_READY = "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n";
        constexpr std::string CSTT_FALSE = "AT+CSTT\r\n\r\nERROR\r";
        constexpr std::string CSTT_READY = "AT+CSTT\r\n\r\nOK\r";
        constexpr std::string CIICR_FALSE = "AT+CIICR\r\n\r\nERROR\r\n"; // todo check
        constexpr std::string CIICR_READY = "AT+CIICR\r\n\r\nOK\r\n";
        constexpr std::string CIFSR_FALSE = "AT+CIFSR\r\n\r\nERRORr\n";
        constexpr std::string CIFSR_READY = "AT+CIFSR\r\n\r\n172.16.101.199\r\n";
        constexpr std::string CIPSTATUS_FALSE = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nERROR\r\n" ; // TODO CHECK
        constexpr std::string CIPSTATUS_START = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n" ;
        constexpr std::string CIPSTATUS_GPRSACT = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n" ;
        constexpr std::string CIPSTATUS_CONNECTED = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n" ;
        constexpr std::string CIPSTATUS_PDPDEACT = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: PDP-DEACT" ;
        constexpr std::string CIPSTART_FALSE = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nERROR\r\n" ;
        constexpr std::string CIPSTART_TRUE = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n" ;
        constexpr std::string CIPSEND_FALSE = "AT+CIPSEND=3\r\n\r\nERROR" ; // TODO CHECK
        constexpr std::string CIPSEND_TRUE = "AT+CIPSEND=3\r\n\r\n>" ;
    } replies;
};

}








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
    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, NULLSerialStream> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());
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
const bool verbose = true;

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
    SIM900Emu::SIM900Emulator sim900;

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
              if(sim900LinkState == OTSIM900Link::INIT) {
                  reply = "vfd";  // garbage to force into RETRY_GET_STATE
                  sim900LinkState = OTSIM900Link::GET_STATE;
              } else reply = "AT\r\n\r\nOK\r\n";
          }
          else if("AT+CPIN?" == command) { reply = /* (random() & 1) ? "No PIN\r" : */ "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n"; }  // Relevant states: CHECK_PIN
          else if("AT+CREG?" == command) { reply = /* (random() & 1) ? "+CREG: 0,0\r" : */ "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n"; } // Relevant states: WAIT_FOR_REGISTRATION
          else if("AT+CSTT=apn" == command) { reply =  "AT+CSTT\r\n\r\nOK\r"; } // Relevant states: SET_APN
          else if("AT+CIPSTATUS" == command) {
              switch (sim900LinkState){
                  case OTSIM900Link::GET_STATE:  // GPRS inactive)
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
bool GoodSimulator::haveSeenCommandStart = false;
}
TEST(OTSIM900Link,basicsSimpleSimulator)
{
//    const bool verbose = B1::verbose;

    srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

    // Reset static state to make tests re-runnable.
    B1::GoodSimulator::haveSeenCommandStart = false;

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
    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, B1::GoodSimulator> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 100; ++i) { incrementVTOneCycle(); statesChecked[l0._getState()] = true; l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}
    // Queue a message to send.
    l0.queueToSend((const uint8_t *)message, (uint8_t)sizeof(message)-1, (int8_t) 0, OTRadioLink::OTRadioLink::TXnormal);
    for(int i = 0; i < 100; ++i) { incrementVTOneCycle(); statesChecked[l0._getState()] = true; l0.poll(); }
    EXPECT_TRUE(B1::GoodSimulator::haveSeenCommandStart) << "should see some attempt to communicate with SIM900";
    for(size_t i = 0; i < OTSIM900Link::RESET; i++)
        { EXPECT_TRUE(statesChecked[i]) << "state " << i << " not seen."; } // Check what states have been seen.
    // ...
    l0.end();
}


namespace B2 {
const bool verbose = false;

// Gets to CHECK_PIN state and then starts spewing random characters..
// Allows for checking getResponse can deal with invalid input, and tests the RESET state.
class GarbageSimulator final : public Stream
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
            if('A' == c) {
                waitingForCommand = false;
                collectingCommand = true;
                command = 'A';
                haveSeenCommandStart = true; // Note at least one command start.
            }
        } else {
            // Look for CR (or LF) to terminate a command.
            if(('\r' == c) || ('\n' == c)) {
                waitingForCommand = true;
                collectingCommand = false;
                if(verbose) { fprintf(stderr, "command received: %s\n", command.c_str()); }
                if("AT" == command) { // Relevant states: GET_STATE, RETRY_GET_STATE, START_UP
                    if(sim900LinkState == OTSIM900Link::INIT) {
                        reply = "vfd";  // garbage to force into RETRY_GET_STATE
                        sim900LinkState = OTSIM900Link::GET_STATE;
                    } else reply = "AT\r\n\r\nOK\r\n";
                } else {
                    // spew out garbage...
                    reply.resize(500);
                    for(size_t i = 0; i < 500; i++) {
                        reply[i] = char(random() & 0xff);
                    }
                }
            } else if(collectingCommand) { command += c; }
        }
        if(verbose) { if(isprint(c)) { fprintf(stderr, "<%c\n", c); } else { fprintf(stderr, "< %d\n", (uint8_t)c); } }
        return(1);
    }
    virtual int read() override
    {
        if(0 == reply.size()) { return(-1); }
        const char c = reply[0];
        if(verbose) { if(isprint(c)) { fprintf(stderr, ">%c\n", c); } else { fprintf(stderr, "> %d\n", (uint8_t)c); } }
        reply.erase(0, 1);
        return(c);
    }
    virtual int available() override { return(-1); }
    virtual int peek() override { return(-1); }
    virtual void flush() override { }
  };
// Events exposed.
bool GarbageSimulator::haveSeenCommandStart = false;
}

TEST(OTSIM900Link,GarbageTestSimulator)
{
//    const bool verbose = B2::verbose;

    // Seed random() for use in simulator; --gtest_shuffle will force it to change.
    srandom((unsigned) ::testing::UnitTest::GetInstance()->random_seed());

    // Reset static state to make tests re-runnable.
    B2::GarbageSimulator::haveSeenCommandStart = false;

    // Vector of bools containing states to check. This covers all states expected in normal use. RESET and PANIC are not covered.
    std::vector<bool> statesChecked(OTSIM900Link::RESET, false);
    // Message to send.

    const char SIM900_PIN[] = "1111";
    const char SIM900_APN[] = "apn";
    const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
    const char SIM900_UDP_PORT[] = "9999";
    const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
    const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);

    ASSERT_FALSE(B2::GarbageSimulator::haveSeenCommandStart);
    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, B2::GarbageSimulator> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 100; ++i) { incrementVTOneCycle(); statesChecked[l0._getState()] = true; l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}
    EXPECT_TRUE(B2::GarbageSimulator::haveSeenCommandStart) << "should see some attempt to communicate with SIM900";
    EXPECT_TRUE(statesChecked[OTSIM900Link::INIT]) << "state GET_STATE not seen.";  // Check what states have been seen.
    EXPECT_TRUE(statesChecked[OTSIM900Link::GET_STATE]) << "state RETRY_GET_STATE not seen.";  // Check what states have been seen.
    EXPECT_TRUE(statesChecked[OTSIM900Link::START_UP]) << "state START_UP not seen.";  // Check what states have been seen.
    EXPECT_TRUE(statesChecked[OTSIM900Link::CHECK_PIN]) << "state CHECK_PIN not seen.";  // Check what states have been seen.
    EXPECT_TRUE(statesChecked[OTSIM900Link::RESET]) << "state RESET not seen.";  // Check what states have been seen.

    // ...
    l0.end();
}


// Simulate resetting the SIM900 due sending the maximum allowed value of message counter.
namespace B3
{
const bool verbose = false;

// Gets the SIM900 to a ready to send state and then forces a reset.
// First will stop responding, then will start up again and do sends.
class MessageCountResetSimulator final : public Stream
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

    // Prepare the SIM900 for testing by bringing it into a ready-to-send state.
    bool isSIM900Prepared = false;
    void prepareSIM900() {
        // Respond to particular commands...
        if("AT+CPIN?" == command) { reply = "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n"; }  // Relevant states: CHECK_PIN
        else if("AT+CREG?" == command) { reply = "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n"; } // Relevant states: WAIT_FOR_REGISTRATION
        else if("AT+CSTT=apn" == command) { reply =  "AT+CSTT\r\n\r\nOK\r"; } // Relevant states: SET_APN
        else if("AT+CIPSTATUS" == command) {
            switch (sim900LinkState){
                case OTSIM900Link::START_UP:  // GPRS inactive)
                    sim900LinkState = OTSIM900Link::START_GPRS;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n";
                    break;
                case OTSIM900Link::START_GPRS:          // GPRS is activated.
                    sim900LinkState = OTSIM900Link::GET_IP;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n";
                    break;
                case OTSIM900Link::GET_IP:    // UDP connected.
                    sim900LinkState = OTSIM900Link::IDLE;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n";
                    break;
                default: break;
            }
        }  // Relevant states: START_GPRS, WAIT_FOR_UDP
        else if("AT+CIICR" == command) { reply = "AT+CIICR\r\n\r\nOK\r\n"; }  // Relevant states: START_GPRS
        else if("AT+CIFSR" == command) { reply = "AT+CIFSR\r\n\r\n172.16.101.199\r\n"; }  // Relevant States: GET_IP
        else if("AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"" == command) { reply = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n"; }  // Relevant states: OPEN_UDP
    }

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
            if("AT" == command) {  // Relevant states: GET_STATE, RETRY_GET_STATE, START_UP
                reply = "AT\r\n\r\nOK\r\n";
                sim900LinkState = OTSIM900Link::START_UP; // Hacky way of synchronising the internal state after reset (AT is only used when restarting)..
            }
            else if (sim900LinkState < OTSIM900Link::IDLE)  { prepareSIM900(); } // 9 corresponds to IDLE
            else if("AT+CIPSTATUS" == command) { reply = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n"; }
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
bool MessageCountResetSimulator::haveSeenCommandStart = false;
}
TEST(OTSIM900Link, MessageCountResetTest)
{
//        const bool verbose = B3::verbose;

        srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

        // Reset static state to make tests re-runnable.
        B3::MessageCountResetSimulator::haveSeenCommandStart = false;

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


        ASSERT_FALSE(B3::MessageCountResetSimulator::haveSeenCommandStart);
        OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, B3::MessageCountResetSimulator> l0;
        EXPECT_TRUE(l0.configure(1, &l0Config));
        EXPECT_TRUE(l0.begin());
        EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

        // Get to IDLE state
        EXPECT_FALSE(l0.isPowered());
        for(int i = 0; i < 20; ++i) { l0.poll(); secondsVT += 12; if(l0._getState() == OTSIM900Link::IDLE) break;}
//        EXPECT_TRUE(l0.isPowered()); //FIXME test broken due to implementation change (DE20161128)

        // Queue a message to send. ResetSimulator should reply PDP DEACT which should trigger a reset.
        int sendCounter = 0;
        for( sendCounter = 0; sendCounter < 300; sendCounter++) {
            if (!l0.isPowered()) break;
            l0.queueToSend((const uint8_t *)message, (uint8_t)sizeof(message)-1, (int8_t) 0, OTRadioLink::OTRadioLink::TXnormal);
            for(int j = 0; j < 10; ++j) { incrementVTOneCycle(); if (!l0.isPowered()) break; l0.poll(); }
        }
        EXPECT_FALSE(l0.isPowered()) << "Expected l0.isPowered to be false.";
//        EXPECT_EQ(255, sendCounter)  << "Expected 255 messages sent."; // FIXME test broken due to implementation change (DE20161128)
        secondsVT += 15;
        l0.poll();
//        EXPECT_EQ(OTSIM900Link::START_UP, l0._getState()) << "Expected state to be START_UP."; FIXME test broken due to implementation change (DE20161128)
        incrementVTOneCycle();
        l0.poll();
//        EXPECT_TRUE(l0.isPowered())  << "Expected l0.isPowered to be true."; FIXME test broken due to implementation change (DE20161128)

        for(int i = 0; i < 20; ++i) { incrementVTOneCycle(); l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}

        EXPECT_EQ(OTSIM900Link::IDLE, l0._getState()) << "Expected state to be IDLE.";

        // ...
        l0.end();
}


// Simulate resetting the SIM900.
namespace B4
{
const bool verbose = false;

// Gets the SIM900 to a ready to send state and then forces a reset.
// First will stop responding, then will start up again and do sends.
class PDPDeactResetSimulator final : public Stream
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

    // Prepare the SIM900 for testing by bringing it into a ready-to-send state.
    bool isSIM900Prepared = false;
    void prepareSIM900() {
        // Respond to particular commands...
        if("AT+CPIN?" == command) { reply = "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n"; }  // Relevant states: CHECK_PIN
        else if("AT+CREG?" == command) { reply = "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n"; } // Relevant states: WAIT_FOR_REGISTRATION
        else if("AT+CSTT=apn" == command) { reply =  "AT+CSTT\r\n\r\nOK\r"; } // Relevant states: SET_APN
        else if("AT+CIPSTATUS" == command) {
            switch (sim900LinkState){
                case OTSIM900Link::START_UP:  // GPRS inactive)
                    sim900LinkState = OTSIM900Link::START_GPRS;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n";
                    break;
                case OTSIM900Link::START_GPRS:          // GPRS is activated.
                    sim900LinkState = OTSIM900Link::GET_IP;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n";
                    break;
                case OTSIM900Link::GET_IP:    // UDP connected.
                    sim900LinkState = OTSIM900Link::IDLE;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n";
                    break;
                default: break;
            }
        }  // Relevant states: START_GPRS, WAIT_FOR_UDP
        else if("AT+CIICR" == command) { reply = "AT+CIICR\r\n\r\nOK\r\n"; }  // Relevant states: START_GPRS
        else if("AT+CIFSR" == command) { reply = "AT+CIFSR\r\n\r\n172.16.101.199\r\n"; }  // Relevant States: GET_IP
        else if("AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"" == command) { reply = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n"; }  // Relevant states: OPEN_UDP
    }

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
            if("AT" == command) {  // Relevant states: GET_STATE, RETRY_GET_STATE, START_UP
                reply = "AT\r\n\r\nOK\r\n";
                sim900LinkState = OTSIM900Link::START_UP; // Hacky way of synchronising the internal state after reset (AT is only used when restarting)..
            }
            else if (sim900LinkState < OTSIM900Link::IDLE)  { prepareSIM900(); } // 9 corresponds to IDLE
            else if("AT+CIPSTATUS" == command) { reply = (random() & 0x01) ? "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n" : "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: PDP-DEACT"; }
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
bool PDPDeactResetSimulator::haveSeenCommandStart = false;
}
TEST(OTSIM900Link, PDPDeactResetTest)
{
//        const bool verbose = B3::verbose;

        srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

        // Reset static state to make tests re-runnable.
        B4::PDPDeactResetSimulator::haveSeenCommandStart = false;

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


        ASSERT_FALSE(B4::PDPDeactResetSimulator::haveSeenCommandStart);
        OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, B4::PDPDeactResetSimulator> l0;
        EXPECT_TRUE(l0.configure(1, &l0Config));
        EXPECT_TRUE(l0.begin());
        EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

        // Get to IDLE state
        EXPECT_FALSE(l0.isPowered());
        for(int i = 0; i < 20; ++i) { incrementVTOneCycle(); statesChecked[l0._getState()] = true; l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}
//        EXPECT_TRUE(l0.isPowered()); FIXME test broken due to implementation change (DE20161128)

        // Queue a message to send. ResetSimulator should reply PDP DEACT which should trigger a reset.
        for( int i = 0; i < 300; i++) {
            if (!l0.isPowered()) break;
            l0.queueToSend((const uint8_t *)message, (uint8_t)sizeof(message)-1, (int8_t) 0, OTRadioLink::OTRadioLink::TXnormal);
            for(int j = 0; j < 10; ++j) { incrementVTOneCycle(); if (!l0.isPowered()) break; l0.poll(); }
        }
        EXPECT_FALSE(l0.isPowered()) << "Expected l0.isPowered to be false.";
        secondsVT += 12;
        l0.poll();
//        EXPECT_EQ(OTSIM900Link::START_UP, l0._getState()) << "Expected state to be START_UP."; FIXME test broken due to implementation change (DE20161128)
        incrementVTOneCycle();
        l0.poll();
//        EXPECT_TRUE(l0.isPowered())  << "Expected l0.isPowered to be true."; FIXME test broken due to implementation change (DE20161128)

        for(int i = 0; i < 20; ++i) { incrementVTOneCycle(); l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}

        EXPECT_EQ(OTSIM900Link::IDLE, l0._getState()) << "Expected state to be IDLE.";

        // ...
        l0.end();
}


// Simulate resetting the SIM900. XXX
namespace B5
{
const bool verbose = true;

// Gets the SIM900 to a ready to send state and then forces a reset.
// First will stop responding, then will start up again and do sends.
class PowerStateSimulator final : public Stream
  {
  public:
    // Events exposed.
    static bool haveSeenCommandStart;
    static bool powered;

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

    // Prepare the SIM900 for testing by bringing it into a ready-to-send state.
    bool isSIM900Prepared = false;
    void prepareSIM900() {
        // Respond to particular commands...
        if("AT+CPIN?" == command) { reply = "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n"; }  // Relevant states: CHECK_PIN
        else if("AT+CREG?" == command) { reply = "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n"; } // Relevant states: WAIT_FOR_REGISTRATION
        else if("AT+CSTT=apn" == command) { reply =  "AT+CSTT\r\n\r\nOK\r"; } // Relevant states: SET_APN
        else if("AT+CIPSTATUS" == command) {
            switch (sim900LinkState){
                case OTSIM900Link::START_UP:  // GPRS inactive)
                    sim900LinkState = OTSIM900Link::START_GPRS;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n";
                    break;
                case OTSIM900Link::START_GPRS:          // GPRS is activated.
                    sim900LinkState = OTSIM900Link::GET_IP;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n";
                    break;
                case OTSIM900Link::GET_IP:    // UDP connected.
                    sim900LinkState = OTSIM900Link::IDLE;
                    reply = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n";
                    break;
                default: break;
            }
        }  // Relevant states: START_GPRS, WAIT_FOR_UDP
        else if("AT+CIICR" == command) { reply = "AT+CIICR\r\n\r\nOK\r\n"; }  // Relevant states: START_GPRS
        else if("AT+CIFSR" == command) { reply = "AT+CIFSR\r\n\r\n172.16.101.199\r\n"; }  // Relevant States: GET_IP
        else if("AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"" == command) { reply = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n"; }  // Relevant states: OPEN_UDP
    }

  public:
    void begin(unsigned long) { }
    void begin(unsigned long, uint8_t);
    void end();

    virtual size_t write(uint8_t uc) override
      {
      const char c = (char)uc;
      if(/*powered && XXX */ waitingForCommand)
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
            if("AT" == command) {  // Relevant states: GET_STATE, RETRY_GET_STATE, START_UP
                reply = "AT\r\n\r\nOK\r\n";
                sim900LinkState = OTSIM900Link::START_UP; // Hacky way of synchronising the internal state after reset (AT is only used when restarting)..
            }
            else if (sim900LinkState < OTSIM900Link::IDLE)  { prepareSIM900(); } // 9 corresponds to IDLE
            else if("AT+CIPSTATUS" == command) { reply = (random() & 0x01) ? "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n" : "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: PDP-DEACT"; }
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
bool PowerStateSimulator::haveSeenCommandStart = false;
/**
 * @brief   Keep track of whether SIM900 is powered.
 * @note    powered should only flip state if the power pin is held high for longer than 2 seconds VT.
 */
bool PowerStateSimulator::powered = true; // expose this one XXX
static constexpr uint_fast8_t minPowerToggleTime = 2;
uint_fast8_t pinSetHighTime;
/**
 * @brief   Flip power state if pin state is high for longer than 2 seconds.
 */
void updateSIM900Powered(const bool pinstate) {
    static bool oldPinState = false;
    if (pinstate) {
        if(!oldPinState) pinSetHighTime = secondsVT;
        if((getSecondsVT() - pinSetHighTime) > minPowerToggleTime) PowerStateSimulator::powered = ~PowerStateSimulator::powered;
    }
}
}
// Commented as not currently implemented (DE20161128)
TEST(OTSIM900Link, PowerStateTest)
{
//        const bool verbose = B3::verbose;


        srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

        // Reset static state to make tests re-runnable.
        B5::PowerStateSimulator::haveSeenCommandStart = false;
        B5::PowerStateSimulator::powered = false;

        // Vector of bools containing states to check. This covers all states expected in normal use. RESET and PANIC are not covered.
        std::vector<bool> statesChecked(OTSIM900Link::RESET, false);

        // Message to send.
//        const char message[] = "123";

        // SIM900 Config data
        const char SIM900_PIN[] = "1111";
        const char SIM900_APN[] = "apn";
        const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
        const char SIM900_UDP_PORT[] = "9999";
        const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
        const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);

        // OTSIM900Link instantiation & init.
        ASSERT_FALSE(B5::PowerStateSimulator::haveSeenCommandStart);
        OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, B5::PowerStateSimulator> l0;
        EXPECT_TRUE(l0.configure(1, &l0Config));
        EXPECT_TRUE(l0.begin());
        EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

        // Walk through startup behaviour in detail.
//        incrementVTOneCycle();
        l0.poll();
        EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());
        secondsVT += 12;
        l0.poll(); // This poll clears the bPowerLock flag
        l0.poll(); // Allowing this to carry on with the program flow.
        EXPECT_EQ(OTSIM900Link::START_UP, l0._getState());
        incrementVTOneCycle();
//        incrementVTOneCycle();
//        l0.poll();
//        EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());
//        secondsVT += 12;
//        l0.poll();
//        EXPECT_EQ(OTSIM900Link::START_UP, l0._getState());
//        incrementVTOneCycle();
        l0.poll();
        EXPECT_EQ(OTSIM900Link::CHECK_PIN, l0._getState());
        incrementVTOneCycle();
        for (int i = 0; i < 10; i++) { l0.poll(); incrementVTOneCycle(); }

//        // Test power up.
//        B5::updateSIM900Powered(l0._isPinHigh());
//        EXPECT_FALSE(l0.isPowered());
//        EXPECT_FALSE(B5::PowerStateSimulator::powered);
//        EXPECT_FALSE(l0._isPinHigh());
//        l0.poll();  // 0 seconds
//        B5::updateSIM900Powered(l0._isPinHigh());
//        EXPECT_FALSE(l0.isPowered());
//        EXPECT_FALSE(B5::PowerStateSimulator::powered);
//        EXPECT_TRUE(l0._isPinHigh());
//        secondsVT++;
//        l0.poll();  // 1 seconds
//        B5::updateSIM900Powered(l0._isPinHigh());
//        EXPECT_FALSE(l0.isPowered());
//        EXPECT_FALSE(B5::PowerStateSimulator::powered);
//        EXPECT_TRUE(l0._isPinHigh()) ;
//        secondsVT++;
//        l0.poll();  // 2 seconds
//        B5::updateSIM900Powered(l0._isPinHigh());
//        EXPECT_FALSE(l0.isPowered());
//        EXPECT_FALSE(B5::PowerStateSimulator::powered);
//        EXPECT_TRUE(l0._isPinHigh());
//        secondsVT++;
//        l0.poll();  // 3 seconds . SIM900 should be powered by now.
//        B5::updateSIM900Powered(l0._isPinHigh());
//        EXPECT_TRUE(l0.isPowered());  // SIM900 should be powered by now.
//        EXPECT_TRUE(B5::PowerStateSimulator::powered);
//        EXPECT_FALSE(l0._isPinHigh()); // Pin should be set low.

        // ...
        l0.end();
}


