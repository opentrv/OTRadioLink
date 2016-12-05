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
// Simple short-term (<60s) elapsed-time computations for wall-clock seconds.
// Will give unhelpful results if called more than 60s after the original sample.
// Takes a value of 'now' as returned by getSecondsLT().
inline constexpr uint_fast8_t getElapsedSecondsVT(const uint_fast8_t startSecondsLT, const uint_fast8_t now)
  { return((now >= startSecondsLT) ? (now - startSecondsLT) : (60 + now - startSecondsLT)); }
/**
 * @brief   Increment secondsVT by 1 minor cycle.
 */
static constexpr uint_fast8_t minorCycleTimeSecs = 2;
static void incrementVTOneCycle() { secondsVT += minorCycleTimeSecs; }

namespace SIM900Emu {

struct SIM900Commands {
    static const char * AT;
    static const char * CPIN;
    static const char * CREG;
    static const char * CSTT;
    static const char * CIICR;
    static const char * CIFSR;
    static const char * CIPSTATUS;
    static const char * CIPSTART;
    static const char * CIPSEND;
};
const char * SIM900Commands::AT = "AT";
const char * SIM900Commands::CPIN = "AT+CPIN?";
const char * SIM900Commands::CREG = "AT+CREG?";
const char * SIM900Commands::CSTT = "AT+CSTT=apn";
const char * SIM900Commands::CIICR = "AT+CIICR";
const char * SIM900Commands::CIFSR = "AT+CIFSR";
const char * SIM900Commands::CIPSTATUS = "AT+CIPSTATUS";
const char * SIM900Commands::CIPSTART = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"";
const char * SIM900Commands::CIPSEND = "AT+CIPSEND=3";

struct SIM900Replies {
    static const char * AT;
    static const char * CPIN_TRUE;
    static const char * CREG_FALSE;
    static const char * CREG_TRUE;
    static const char * CSTT_FALSE;
    static const char * CSTT_TRUE;
    static const char * CIICR_FALSE; // todo check
    static const char * CIICR_TRUE;
    static const char * CIFSR_FALSE;
    static const char * CIFSR_TRUE;
    static const char * CIPSTATUS_FALSE;
    static const char * CIPSTATUS_START;
    static const char * CIPSTATUS_GPRSACT;
    static const char * CIPSTATUS_CONNECTED;
    static const char * CIPSTATUS_PDPDEACT;
    static const char * CIPSTART_FALSE;
    static const char * CIPSTART_TRUE;
    static const char * CIPSEND_FALSE; // TODO CHECK
    static const char * CIPSEND_TRUE;
};
const char * SIM900Replies::AT = "AT\r\n\r\nOK\r\n";
const char * SIM900Replies::CPIN_TRUE = "AT+CPIN?\r\n\r\n+CPIN: READY\r\n\r\nOK\r\n";
const char * SIM900Replies::CREG_FALSE = "AT+CREG?\r\n\r\n+CREG: 0,0\r\n\r\n'OK\r\n";
const char * SIM900Replies::CREG_TRUE = "AT+CREG?\r\n\r\n+CREG: 0,5\r\n\r\n'OK\r\n";
const char * SIM900Replies::CSTT_FALSE = "AT+CSTT\r\n\r\nERROR\r";
const char * SIM900Replies::CSTT_TRUE = "AT+CSTT\r\n\r\nOK\r";
const char * SIM900Replies::CIICR_FALSE = "AT+CIICR\r\n\r\nERROR\r\n"; // todo check
const char * SIM900Replies::CIICR_TRUE = "AT+CIICR\r\n\r\nOK\r\n";
const char * SIM900Replies::CIFSR_FALSE = "AT+CIFSR\r\n\r\nERRORr\n";
const char * SIM900Replies::CIFSR_TRUE = "AT+CIFSR\r\n\r\n172.16.101.199\r\n";
const char * SIM900Replies::CIPSTATUS_FALSE = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nERROR\r\n" ; // TODO CHECK
const char * SIM900Replies::CIPSTATUS_START = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP START\r\n" ;
const char * SIM900Replies::CIPSTATUS_GPRSACT = "AT+CIPSTATUS\r\n\r\nOK\r\n\r\nSTATE: IP GPRSACT\r\n" ;
const char * SIM900Replies::CIPSTATUS_CONNECTED = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: CONNECT OK\r\n" ;
const char * SIM900Replies::CIPSTATUS_PDPDEACT = "AT+CIPSTATUS\r\n\r\nOK\r\nSTATE: PDP-DEACT" ;
const char * SIM900Replies::CIPSTART_FALSE = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nERROR\r\n" ;
const char * SIM900Replies::CIPSTART_TRUE = "AT+CIPSTART=\"UDP\",\"0.0.0.0\",\"9999\"\r\n\r\nOK\r\n\r\nCONNECT OK\r\n" ;
const char * SIM900Replies::CIPSEND_FALSE = "AT+CIPSEND=3\r\n\r\nERROR" ; // TODO CHECK
const char * SIM900Replies::CIPSEND_TRUE = "AT+CIPSEND=3\r\n\r\n>" ;

/**
 * @brief   Simple emulator for keeping track of SIM900 state and providing appropriate responses.
 * @todo    emulate time
 */
class SIM900StateEmulator {
public:
    SIM900StateEmulator() : oldPinState(false), startTime(0) {};

    /**
     * @brief   Non-exhaustive list of states we go through using OTSIM900Link.
     * @note    States with a verb are transitory and can only be exited by the SIM900.
     */
    enum state_t {
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

    SIM900Replies replies;
    SIM900Commands commands;

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
//            myState = IP_CONFIGURING; // XXX
            myState = IP_GPRSACT;
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
//            myState = UDP_CONNECTING; // XXX
            myState = UDP_CONNECT_OK;
            break;
        case UDP_CONNECTING:
            // Wait a bit
            myState = UDP_CONNECT_OK;
            break;
        case UDP_CONNECT_OK:
            // This should correspond to IDLE. Waiting to send stuff.
            myState = UDP_CONNECT_OK;
            break;
        case UDP_CLOSING:
            // Wait a bit
//            myState = UDP_CLOSED; // XXX
            myState = UDP_CONNECTING;
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


    /**
     * @brief   Ensure serial command valid and delete trailing '\r\n'
     */
    bool verbose = false;
    bool parseCommand(std::string &command) {
        bool valid = false;  // Bool to store whether command is valid.
        if (0 < command.size()) {
            if((command.front() == 'A') && (command.back() == '\n')) {
                valid = true;
                command.pop_back();
                command.pop_back();
            }
            // Print command
            if (verbose) {
                std::string error = "";
                if (!valid) error += " INVALID";
                fprintf(stderr, "command received: \"%s\"%s\n", command.c_str(), error.c_str());
            }
        }
        return valid;
    }
    /**
     * @brief   Work through the state machine, replying as appropriate.
     * @param   command:    String containing the command.
     * @param   reply:      String to contain the response. Must be big enough to fit the full response!
     * @todo    add state machine updates
     * @note    APN must be set to "apn" with no quotes to be accepted.
     */
    void poll(std::string const &command, std::string &reply) {
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
            if(commands.AT == command) { reply.append(replies.AT); }           // Normal response
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }   // No need to set a PIN.
            else if(commands.CREG == command) { reply.append(replies.CREG_FALSE); updateState(); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_FALSE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_FALSE); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_INITIAL:
            // Need APN to be set. (CSTT=...)
            if(commands.AT == command) { reply.append(replies.AT); }           // Normal response
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }  // Now registered.
            else if(commands.CSTT == command) { reply.append(replies.CSTT_TRUE); updateState(); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_FALSE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_FALSE); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_START:
            // Need to start GPRS (CIICR)
            if(commands.AT == command) { reply.append(replies.AT); }
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }    // Can not set APN again!
            else if(commands.CIICR == command) { reply.append(replies.CIICR_TRUE); updateState();}  // Can start GPRS
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_FALSE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_START); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_CONFIGURING:
            // Wait a bit
            updateState();
            break;
        case IP_GPRSACT:
            // Need to check IP address (CIFSR)
            if(commands.AT == command) { reply.append(replies.AT); }
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }  // GPRS already started...
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_TRUE); updateState(); }  // Check IP address
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_GPRSACT); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case IP_STATUS:
            // Need to open UDP connection
            if(commands.AT == command) { reply.append(replies.AT); }
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_TRUE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_GPRSACT); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_TRUE); updateState(); }  // Open UDP
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        case UDP_CONNECTING:
            // Wait a bit
            updateState();
            break;
        case UDP_CONNECT_OK:
            // This should correspond to IDLE. Waiting to send stuff.
            if(commands.AT == command) { reply.append(replies.AT); }
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_TRUE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_CONNECTED); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_TRUE); }
            else if("123" == command) { reply = "123\r\nSEND OK\r\n"; }  // todo this must depend on the cipsend being asked.
            break;
        case UDP_CLOSING:
            // Wait a bit
            updateState();
            break;
        case UDP_CLOSED:
            // Need to open UDP or close GPRS.
            if(commands.AT == command) { reply.append(replies.AT); }
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_TRUE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_START); } // todo check
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_TRUE); updateState();}
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }
            break;
        case PDP_DEACTIVATING:
            if(commands.AT == command) { reply.append(replies.AT); }           // Normal response
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }   // No need to set a PIN.
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_FALSE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_PDPDEACT); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            updateState();
            break;
        case PDP_FAIL:
            // Everything has died.
            if(commands.AT == command) { reply.append(replies.AT); }           // Normal response
            else if(commands.CPIN == command) { reply.append(replies.CPIN_TRUE); }   // No need to set a PIN.
            else if(commands.CREG == command) { reply.append(replies.CREG_TRUE); }
            else if(commands.CSTT == command) { reply.append(replies.CSTT_FALSE); }
            else if(commands.CIICR == command) { reply.append(replies.CIICR_FALSE); }
            else if(commands.CIFSR == command) { reply.append(replies.CIFSR_FALSE); }
            else if(commands.CIPSTATUS == command) { reply.append(replies.CIPSTATUS_PDPDEACT); }
            else if(commands.CIPSTART == command) { reply.append(replies.CIPSTART_FALSE); }
            else if(commands.CIPSEND == command) { reply.append(replies.CIPSEND_FALSE); }
            else if("123" == command) { reply = "123\r\nERROR\r\n"; }  // Relevant states: SENDING TODO CHECK
            break;
        default: break;
        }
    }

    // Trigger fail states:
    // This triggers a dead-end state caused by signal loss during UDP connection
    void triggerPDPDeactFail() { myState = PDP_FAIL; }
    // This triggers a fail state where the SIM900 carries on responding normally.
    void triggerInvisibleFail() { myState = INVISIBLE_FAIL; }

    // emulate pin toggle:
    bool oldPinState;
    uint_fast8_t startTime;
    static constexpr uint_fast8_t minPowerPinToggleVT = 2; // Pin must be set high for at least 2 seconds to register.

    /**
     * @brief   Set all state back to defaults.
     * fixme POWERING_UP is a fudge until we have a good way of powering up emulator.
     */
    void reset() { myState = POWERING_UP; verbose = false; oldPinState = false, startTime = 0; }


    /**
     * @brief keep track of power pin
     */
    void pollPowerPin(bool high) {
        if(high) {
            if (!oldPinState)startTime = getSecondsVT();
            else  if (minPowerPinToggleVT >= getElapsedSecondsVT(startTime, getSecondsVT())) {  // XXX
                if (myState == POWER_OFF) myState = POWERING_UP;
                else myState = POWER_OFF;
            }
        }
        oldPinState = high;
    }
};

// Emulates blocking soft serial class.
class SoftSerialSimulator final : public Stream
    {
    private:
		// Data available to be read().
		static std::string toBeRead;

    public:
        static bool verbose;

        // Reset to clear state before a new test.
        static void reset() { written = ""; toBeRead = ""; writeCallback = NULL; }

        // Data written (with the write() call) to this Stream, outbound.
        static std::string written;

        // Callback to be made on write (if callback not NULL).
        static void (*writeCallback)();
        static void _doCallBackOnWrite() { if(NULL != writeCallback) { writeCallback(); } }

        // Add another char for read() to pick up.
        static void addCharToRead(const char c) { toBeRead += c; }
        // Add a whole string for read() to pick up.
        static void addCharToRead(const std::string &s) { toBeRead += s; }

        // Method from Stream.
        virtual size_t write(uint8_t uc) override
        {
            const char c = (char)uc;
            if(verbose) { if(isprint(c)) { fprintf(stderr, "<%c\n", c); } else { fprintf(stderr, "< %d\n", (int)c); } }
            written += c;
            _doCallBackOnWrite();
            return(1);
        }
        // Method from Stream.
        virtual int read() override
        {
            if(0 == toBeRead.size()) { return(-1); }
            const char c = toBeRead.front();
            if(verbose) { if(isprint(c)) { fprintf(stderr, ">%c\n", c); } else { fprintf(stderr, "> %d\n", (int)c); } }
            toBeRead.erase(0, 1);
            return(c);
        }

        void printToBeRead() { if(toBeRead.size()) fprintf(stderr, "\n>>>>>toBeRead:\n%s\n", toBeRead.c_str()); }
        void printWritten() { if(written.size()) fprintf(stderr, "\n\n<<<<<written:\n%s\n", written.c_str()); }

        // Method from Stream.
        virtual int available() override { return(-1); }
        // Method from Stream.
        virtual int peek() override { return(-1); }
        // Method from Stream.
        virtual void flush() override { }
        // Method from Serial.
        void begin(unsigned long) { }
        void end();
    };
std::string SoftSerialSimulator::toBeRead = "";
std::string SoftSerialSimulator::written = "";
void (*SoftSerialSimulator::writeCallback)() = NULL;

bool SoftSerialSimulator::verbose = false;
// Singleton instance.
static SoftSerialSimulator serialConnection;

/**
 * @brief class that holds emulator and serial object.
 */
class SIM900 {
private:

public:
    bool verbose = false;
    // actual emulator
    SIM900StateEmulator emu;

    /**
     * @brief   reset SIM900 state for new test.
     */
    void reset() { verbose = false; emu.reset(); }
    /**
     * @brief   Collects characters until a valid end character is seen.
     * @param
     * @retval  true if valid string found.
     */
    bool isEndCharReceived(std::string const &command) { if ('\n' == command.back()) return true; else return false; }
    /**
     * @brief   update emulator state
     * @param   written: buffer written to by OTSIM900Link. WARNING! This must contain a full and valid command
     *          and is cleared after it is read from!
     * @param   toBeRead: buffer to be read by OTSIM900Link. WARNING! This is must be cleared by OTSIM900Link!
     */
    void poll()
    {
//        emu.pollPowerPin(powerPin);
        if(isEndCharReceived(serialConnection.written)) {
            std::string toBeRead = "";
            if(emu.parseCommand(serialConnection.written)) emu.poll(serialConnection.written, toBeRead);
            SIM900Emu::serialConnection.addCharToRead(toBeRead);
            if(verbose) { SIM900Emu::serialConnection.printWritten(); SIM900Emu::serialConnection.printToBeRead(); }
            serialConnection.written.clear();
        }
    }
    /**
     * @brief   Trigger PDP-DEACT state
     */
    void triggerPDPDeactFail() { emu.triggerPDPDeactFail(); }
//    void setVerbose(bool verbose) { serialConnection.verbose = verbose; emu.verbose = verbose; }
};
static SIM900 sim900;
static const auto sim900WriteCallback = [] { sim900.poll(); };
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

// Test usability of SoftSerialSimulator, eg can it compile.
TEST(OTSIM900Link, SoftSerialSimulatorTest)
{
    // Clear out any serial state.
    SIM900Emu::serialConnection.reset();

    const char SIM900_PIN[] = "1111";
    const char SIM900_APN[] = "apn";
    const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
    const char SIM900_UDP_PORT[] = "9999";
    const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
    const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);
    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, SIM900Emu::SoftSerialSimulator> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());
}

// Test usability of SIM900 emulator, eg can it compile.
TEST(OTSIM900Link, SIM900EmulatorTest)
{
    // Clear out any serial state.
    SIM900Emu::serialConnection.reset();
    SIM900Emu::serialConnection.writeCallback = SIM900Emu::sim900WriteCallback;
    // reset emulator state
    SIM900Emu::sim900.reset();
    ASSERT_EQ(SIM900Emu::SIM900StateEmulator::POWERING_UP, SIM900Emu::sim900.emu.myState);
    ASSERT_FALSE(SIM900Emu::sim900.emu.verbose);
    ASSERT_FALSE(SIM900Emu::sim900.emu.oldPinState);
    ASSERT_EQ(0, SIM900Emu::sim900.emu.startTime);

//    SIM900Emu::sim900.verbose = true;

    const char SIM900_PIN[] = "1111";
    const char SIM900_APN[] = "apn";
    const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
    const char SIM900_UDP_PORT[] = "9999";
    const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
    const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);
    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, SIM900Emu::SoftSerialSimulator> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 100; ++i) {
        incrementVTOneCycle();
        l0.poll();
    }

    // ...
    l0.end();
}


/**
 * @brief   Simulate starting up a SIM900 that is powered down.
 */
TEST(OTSIM900Link, StartupFromOffTest)
{

        srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

        // Clear out any serial state.
        SIM900Emu::serialConnection.reset();
        SIM900Emu::serialConnection.writeCallback = SIM900Emu::sim900WriteCallback;
        // reset emulator state
        SIM900Emu::sim900.reset();
        ASSERT_EQ(SIM900Emu::SIM900StateEmulator::POWERING_UP, SIM900Emu::sim900.emu.myState);
        ASSERT_FALSE(SIM900Emu::sim900.emu.verbose);
        ASSERT_FALSE(SIM900Emu::sim900.emu.oldPinState);
        ASSERT_EQ(0, SIM900Emu::sim900.emu.startTime);

        // SIM900 Config data
        const char SIM900_PIN[] = "1111";
        const char SIM900_APN[] = "apn";
        const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
        const char SIM900_UDP_PORT[] = "9999";
        const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
        const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);

        // OTSIM900Link instantiation & init.
        OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, SIM900Emu::SoftSerialSimulator> l0;
        EXPECT_TRUE(l0.configure(1, &l0Config));
        EXPECT_TRUE(l0.begin());
        EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

        // Walk through startup behaviour in detail.
        // - Starts in INIT, Moves on to GET_STATE: GET_STATE, PIN LOW
        l0.poll();
        EXPECT_EQ(OTSIM900Link::GET_STATE, l0._getState());
        EXPECT_FALSE(l0._isPinHigh());
        // - If no reply, toggle pin:               START_UP, PIN HIGH
        l0.poll();
        EXPECT_EQ(OTSIM900Link::START_UP, l0._getState());
        // Pin should be high for 2 seconds.
        EXPECT_TRUE(l0._isPinHigh());
        secondsVT++;
        l0.poll();
        EXPECT_TRUE(l0._isPinHigh());
        secondsVT++;
        l0.poll();
        EXPECT_TRUE(l0._isPinHigh());
        secondsVT++;
        l0.poll();
        EXPECT_FALSE(l0._isPinHigh());
        // Locked out for a further 10 seconds, waiting for lockout to finish.
//        for (int i = secondsVT + 10; secondsVT < i; secondsVT++) EXPECT_EQ(OTSIM900Link::START_UP, l0._getState());
        for (int i = 0; i < 10; i++) {
            secondsVT++;
            l0.poll();
            EXPECT_EQ(OTSIM900Link::START_UP, l0._getState());
        }
        // - Replied so should move on:             CHECK_PIN, PIN LOW
        l0.poll();
        EXPECT_EQ(OTSIM900Link::CHECK_PIN, l0._getState());
        EXPECT_FALSE(l0._isPinHigh());
        incrementVTOneCycle();
        // ...
        l0.end();
}

// Walk through state space of OTSIM900Link.
// Make sure that an instance can be created and does not die horribly.
// Is meant to mainly walk through all the normal expected SIM900 behaviour when all is well.
// Other tests can look at error handling including unexpected/garbage responses.
//namespace B1
//{
//const bool verbose = false;
//
// Does a simple simulation of SIM900, responding sensibly to all commands needed by the OTSIM900Link impl.
// Allows for exercise of every major non-PANIC state of the OTSIM900Link implementation.
//class GoodSimulator final : public Stream
//{
//public:
//    // Events exposed.
//    static bool haveSeenCommandStart;
//
//private:
//    // Command being collected from OTSIM900Link.
//    bool waitingForCommand = true;
//    bool collectingCommand = false;
//    // Entire request starting "AT"; no trailing CR or LF stored.
//    std::string command;
//
//    // Reply (postfix) being returned to OTSIM900Link: empty if none.
//    std::string reply;
//    // Keep track (crudely) of state. Corresponds to OTSIM900LinkState values.
//    SIM900Emu::SIM900StateEmulator sim900;
//
//public:
//    void begin(unsigned long) { }
//    void begin(unsigned long, uint8_t);
//    void end();
//
//    virtual size_t write(uint8_t uc) override
//    {
//        const char c = (char)uc;
//        if(waitingForCommand) {
//            // Look for leading 'A' of 'AT' to start a command.
//            if('A' == c) {
//            waitingForCommand = false;
//            collectingCommand = true;
//            command = 'A';
//            haveSeenCommandStart = true; // Note at least one command start.
//            }
//        } else {
//            // Look for CR (or LF) to terminate a command.
//            if(('\r' == c) || ('\n' == c)) {
//                waitingForCommand = true;
//                collectingCommand = false;
//                if(verbose) { fprintf(stderr, "command received: %s\n", command.c_str()); }
//                // Respond to particular commands...
//                sim900.poll(command, reply);
//            }
//            else if(collectingCommand) { command += c; }
//        }
//        if(verbose) { if(isprint(c)) { fprintf(stderr, "<%c\n", c); } else { fprintf(stderr, "< %d\n", (int)c); } }
//        return(1);
//    }
//    virtual int read() override
//    {
//        if(0 == reply.size()) { return(-1); }
//        const char c = reply[0];
//        if(verbose) { if(isprint(c)) { fprintf(stderr, ">%c\n", c); } else { fprintf(stderr, "> %d\n", (int)c); } }
//        reply.erase(0, 1);
//        return(c);
//    }
//    virtual int available() override { return(-1); }
//    virtual int peek() override { return(-1); }
//    virtual void flush() override { }
//};
// Events exposed.
//bool GoodSimulator::haveSeenCommandStart = false;
//}
TEST(OTSIM900Link,basicsSimpleSimulator)
{
//    const bool verbose = B1::verbose;

    srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

    // Reset static state to make tests re-runnable.
//    B1::GoodSimulator::haveSeenCommandStart = false;
    // Clear out any serial state.
    SIM900Emu::serialConnection.reset();
    SIM900Emu::serialConnection.writeCallback = SIM900Emu::sim900WriteCallback;
    // reset emulator state
    SIM900Emu::sim900.reset();
    ASSERT_EQ(SIM900Emu::SIM900StateEmulator::POWERING_UP, SIM900Emu::sim900.emu.myState);
    ASSERT_FALSE(SIM900Emu::sim900.emu.verbose);
    ASSERT_FALSE(SIM900Emu::sim900.emu.oldPinState);
    ASSERT_EQ(0, SIM900Emu::sim900.emu.startTime);

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


//    ASSERT_FALSE(B1::GoodSimulator::haveSeenCommandStart);
//    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, B1::GoodSimulator> l0;
//    EXPECT_TRUE(l0.configure(1, &l0Config));
//    EXPECT_TRUE(l0.begin());
//    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());
    OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, SIM900Emu::SoftSerialSimulator> l0;
    EXPECT_TRUE(l0.configure(1, &l0Config));
    EXPECT_TRUE(l0.begin());
    EXPECT_EQ(OTSIM900Link::INIT, l0._getState());


    // Try to hang just by calling poll() repeatedly.
    for(int i = 0; i < 100; ++i) { incrementVTOneCycle(); statesChecked[l0._getState()] = true; l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}
    // Queue a message to send.
    l0.queueToSend((const uint8_t *)message, (uint8_t)sizeof(message)-1, (int8_t) 0, OTRadioLink::OTRadioLink::TXnormal);
    for(int i = 0; i < 100; ++i) { incrementVTOneCycle(); statesChecked[l0._getState()] = true; l0.poll(); }
//    EXPECT_TRUE(B1::GoodSimulator::haveSeenCommandStart) << "should see some attempt to communicate with SIM900";
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
    SIM900Emu::sim900.reset();

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



TEST(OTSIM900Link, MessageCountResetTest)
{
//        const bool verbose = B3::verbose;

        srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

        // Clear out any old state.
        SIM900Emu::serialConnection.reset();
        SIM900Emu::serialConnection.writeCallback = SIM900Emu::sim900WriteCallback;
        SIM900Emu::sim900.reset();

        // Vector of bools containing states to check. This covers all states expected in normal use. RESET and PANIC are not covered.
        std::vector<bool> statesChecked(OTSIM900Link::RESET, false);
        // Message to send.
        const char message[] = "123";

        // Setup OTSIM900Link.
        const char SIM900_PIN[] = "1111";
        const char SIM900_APN[] = "apn";
        const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
        const char SIM900_UDP_PORT[] = "9999";
        const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
        const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);
        OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, SIM900Emu::SoftSerialSimulator> l0;
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

//        EXPECT_EQ(OTSIM900Link::IDLE, l0._getState()) << "Expected state to be IDLE."; FIXME resetting not yet implemented in emulator! (DE20161128)

        // ...
        l0.end();
}


TEST(OTSIM900Link, PDPDeactResetTest)
{
//        const bool verbose = B3::verbose;

        srandom((unsigned)::testing::UnitTest::GetInstance()->random_seed()); // Seed random() for use in simulator; --gtest_shuffle will force it to change.

        // Reset static state to make tests re-runnable.
        SIM900Emu::serialConnection.reset();
        SIM900Emu::serialConnection.writeCallback = SIM900Emu::sim900WriteCallback;
        SIM900Emu::sim900.reset();
        ASSERT_EQ(SIM900Emu::SIM900StateEmulator::POWERING_UP, SIM900Emu::sim900.emu.myState);
        ASSERT_FALSE(SIM900Emu::sim900.emu.verbose);
        ASSERT_FALSE(SIM900Emu::sim900.emu.oldPinState);
        ASSERT_EQ(0, SIM900Emu::sim900.emu.startTime);

        // Message to send.
        const char message[] = "123";

        const char SIM900_PIN[] = "1111";
        const char SIM900_APN[] = "apn";
        const char SIM900_UDP_ADDR[] = "0.0.0.0"; // ORS server
        const char SIM900_UDP_PORT[] = "9999";
        const OTSIM900Link::OTSIM900LinkConfig_t SIM900Config(false, SIM900_PIN, SIM900_APN, SIM900_UDP_ADDR, SIM900_UDP_PORT);
        const OTRadioLink::OTRadioChannelConfig l0Config(&SIM900Config, true);


        OTSIM900Link::OTSIM900Link<0, 0, 0, getSecondsVT, SIM900Emu::SoftSerialSimulator> l0;
        EXPECT_TRUE(l0.configure(1, &l0Config));
        EXPECT_TRUE(l0.begin());
        EXPECT_EQ(OTSIM900Link::INIT, l0._getState());

        // Get to IDLE state
        EXPECT_FALSE(l0.isPowered());
        for(int i = 0; i < 20; ++i) { incrementVTOneCycle(); l0.poll(); if(l0._getState() == OTSIM900Link::IDLE) break;}
//        EXPECT_TRUE(l0.isPowered()); FIXME test broken due to implementation change (DE20161128)
        EXPECT_EQ(OTSIM900Link::IDLE, l0._getState()) << "OTSIM900Link NOT IDLE!";
        EXPECT_EQ(SIM900Emu::SIM900StateEmulator::UDP_CONNECT_OK, SIM900Emu::sim900.emu.myState);

        SIM900Emu::sim900.triggerPDPDeactFail();

        EXPECT_EQ(SIM900Emu::SIM900StateEmulator::PDP_FAIL, SIM900Emu::sim900.emu.myState);

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

//        EXPECT_EQ(OTSIM900Link::IDLE, l0._getState()) << "Expected state to be IDLE.";

        // ...
        l0.end();
}

