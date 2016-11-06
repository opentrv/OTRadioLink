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

Author(s) / Copyright (s): Deniz Erbilgin 2016
                           Damon Hart-Davis 2016
*/

/*
 * CLI support routines
 *
 * V0p2/AVR only for now.
 */

// NOTE: some CLI routines may live alongside the devices they support, not here.

#include "OTV0P2BASE_CLI.h"

#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_RTC.h"
#include "OTV0P2BASE_Serial_IO.h"
#include "OTV0P2BASE_Security.h"
#include "OTV0P2BASE_Sleep.h"
#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE {
namespace CLI {


#ifdef ARDUINO_ARCH_AVR

// Prints warning to serial (that must be up and running) that invalid (CLI) input has been ignored.
// Probably should not be inlined, to avoid creating duplicate strings in Flash.
void InvalidIgnored() { Serial.println(F("Invalid, ignored.")); }

// If CLI_INTERACTIVE_ECHO defined then immediately echo received characters, not at end of line.
#define CLI_INTERACTIVE_ECHO
// Character that should trigger any pending command from user to be sent.
// (Should be avoided at start of status output.)
static const char CLIPromptChar = ((char) OTV0P2BASE::SERLINE_START_CHAR_CLI);
// Generate CLI prompt and wait a little while (typically ~1s) for an input command line.
// Returns number of characters read (not including terminating CR or LF); 0 in case of failure.
// Ignores any characters queued before generating the prompt.
// Does not wait if too close to (or beyond) the end of the minor cycle.
// Takes a buffer and its size; fills buffer with '\0'-terminated response if return > 0.
// Serial must already be running.
//   * idlefn: if non-NULL this is called while waiting for input;
//       it must not interfere with UART RX, eg by messing with CPU clock or interrupts
//   * maxSCT maximum sub-cycle time to wait until
uint8_t promptAndReadCommandLine(const uint8_t maxSCT, char *const buf, const uint8_t bufsize, void (*idlefn)())
    {
    if((NULL == buf) || (bufsize < 2)) { return(0); } // FAIL

    // Compute safe limit time given granularity of sleep and buffer fill.
    const uint8_t targetMaxSCT = (maxSCT <= MIN_CLI_POLL_SCT) ? ((uint8_t) 0) : ((uint8_t) (maxSCT - 1 - MIN_CLI_POLL_SCT));
    if(OTV0P2BASE::getSubCycleTime() >= targetMaxSCT) { return(0); } // Too short to try.

    // Purge any stray pending input, such as a trailing LF from previous input.
    while(Serial.available() > 0) { Serial.read(); }

    // Generate and flush prompt character to the user, after a CRLF to reduce ambiguity.
    // Do this AFTER flushing the input so that sending command immediately after prompt should work.
    Serial.println();
    Serial.print(CLIPromptChar);
    // Idle a short while to try to save energy, waiting for serial TX end and possible RX response start.
    OTV0P2BASE::flushSerialSCTSensitive();

    // Wait for input command line from the user (received characters may already have been queued)...
    // Read a line up to a terminating CR, either on its own or as part of CRLF.
    // (Note that command content and timing may be useful to fold into PRNG entropy pool.)
    uint8_t n = 0;
    while(n < bufsize - 1)
        {
        // Read next character if immediately available.
        if(Serial.available() > 0)
          {
          int ic = Serial.read();
          if(('\r' == ic) || ('\n' == ic)) { break; } // Stop at CR, eg from CRLF, or LF.
    //#ifdef CLI_INTERACTIVE_ECHO
    //      if(('\b' == ic) || (127 == ic))
    //        {
    //        // Handle backspace or delete as delete...
    //        if(n > 0) // Ignore unless something to delete...
    //          {
    //          Serial.print('\b');
    //          Serial.print(' ');
    //          Serial.print('\b');
    //          --n;
    //          }
    //        continue;
    //        }
    //#endif
          if((ic < 32) || (ic > 126)) { continue; } // Drop bogus non-printable characters.
          // Ignore any leading char that is not a letter (or '?' or '+'),
          // and force leading (command) char to upper case.
          if(0 == n)
            {
            ic = toupper(ic);
            if(('+' != ic) && ('?' != ic) && ((ic < 'A') || (ic > 'Z'))) { continue; }
            }
          // Store the incoming char.
          buf[n++] = (char) ic;
#ifdef CLI_INTERACTIVE_ECHO
          Serial.print((char) ic); // Echo immediately.
#endif
          continue;
          }
        // Quit WITHOUT PROCESSING THE POSSIBLY-INCOMPLETE INPUT if time limit is hit (or very close).
        const uint8_t sct = OTV0P2BASE::getSubCycleTime();
        if(sct >= maxSCT)
          {
          n = 0;
          break;
          }
        // Idle waiting for input, to save power, then/else do something useful with some CPU cycles...
    //#if CAN_IDLE_15MS
    //    // Minimise power consumption leaving CPU/UART clock running, if no danger of RX overrun.
    //    // Don't do this too close to end of target end time to avoid missing it.
    //    // Note: may get woken on timer0 interrupts as well as RX and watchdog.
    //    if(sct < targetMaxSCT-2)
    //      {
    ////      idle15AndPoll(); // COH-63 and others: crashes some REV0 and REV9 boards (reset).
    //      // Rely on being woken by UART, or timer 0 (every ~16ms with 1MHz CPU), or backstop of timer 2.
    //      set_sleep_mode(SLEEP_MODE_IDLE); // Leave everything running but the CPU...
    //      sleep_mode();
    //      pollIO(false);
    //      continue;
    //      }
    //#endif
        if(NULL != idlefn) { (idlefn)(); } // Do something useful with the time if possible.
        }

    if(n > 0)
        {
        // For implausible input print a very brief low-CPU-cost help message and give up as efficiently and safely and quickly as possible.
        const char firstChar = buf[0];
        const bool plausibleCommand = ((firstChar > ' ') && (firstChar <= 'z'));
        if(!plausibleCommand)
            {
            // Force length back to zero to indicate bad input.
            n = 0;
            Serial.println(F("? for help"));
            }
        else
            {
            // Null-terminate the received command line.
            buf[n] = '\0';

            // strupr(buf); // Force to upper-case
#ifdef CLI_INTERACTIVE_ECHO
            Serial.println(); // ACK user's end-of-line.
#else
            Serial.println(buf); // Echo the line received (asynchronously).
#endif
        // Restart the CLI timer on receipt of plausible (ASCII) input (cf noise from UART floating or starting up),
//            resetCLIActiveTimer();
            }

        // Capture a little potential timing and content entropy at the end, though don't claim any.
        OTV0P2BASE::addEntropyToPool(firstChar ^ OTV0P2BASE::getSubCycleTime(), 0);
        }

    // Force any pending output before return / possible UART power-down.
    OTV0P2BASE::flushSerialSCTSensitive();
    return(n);
    }

// Set / clear node association(s) (nodes to accept frames from) (eg "A hh hh hh hh hh hh hh hh").
//        To add a new node/association: "A hh hh hh hh hh hh hh hh"
//        - Reads first two bytes of each token in hex and ignores the rest.
//        - Prints number of nodes set to serial
//        - Stops adding nodes once EEPROM full
//        - No error checking yet.
//        - Only accepts upper case for hex values.
//        To clear all nodes: "A *".
//        To query current status "A ?".
// On writing a new association/entry all bytes after the ID must be erased to 0xff.
// TODO: optionally allow setting (persistent) MSBs of counter to current+1 and force counterparty restart to eliminate replay attack.
bool SetNodeAssoc::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 3 character sequence makes sense and is safe to tokenise, eg "A *".
    if((buflen >= 3) && (NULL != (tok1 = strtok_r(buf + 2, " ", &last))))
        {
        // "A ?" or "A ? XX"
        if('?' == *tok1)
            {
            // Query current association status.
            Serial.println(F("IDs:"));
            const uint8_t nn = countNodeAssociations();
//            Serial.println(nn);
            // Print first two bytes (and last) of each association's node ID.
            for(uint8_t i = 0; i < nn; ++i)
                {
                uint8_t nodeID[OpenTRV_Node_ID_Bytes];
                getNodeAssociation(i, nodeID);
                Serial.print(nodeID[0], HEX);
                Serial.print(' ');
                Serial.print(nodeID[1], HEX);
                Serial.print(F(" ... "));
                Serial.print(nodeID[OpenTRV_Node_ID_Bytes - 1], HEX);
                Serial.println();
                }
            // If byte is provided in token after '?' then show index from looking up that prefix from 0.
            char *tok2 = strtok_r(NULL, " ", &last);
            if(NULL != tok2)
                {
                const int p1 = OTV0P2BASE::parseHexByte(tok2);
                if(-1 != p1)
                    {
                    const uint8_t prefix1 = p1;
                    uint8_t nodeID[OpenTRV_Node_ID_Bytes];
                    const int8_t index = getNextMatchingNodeID(0, &prefix1, 1, nodeID);
                    Serial.println(index);
                    }
                }
            }
        else if('*' == *tok1)
            {
            // Clear node IDs.
            OTV0P2BASE::clearAllNodeAssociations();
            Serial.println(F("Cleared"));
            }
        else if(buflen >= (1 + 3*OpenTRV_Node_ID_Bytes)) // 25)
            {
            // corresponds to "A " followed by 8 space-separated hex-byte tokens.
            // Note: As there is no variable for the number of nodes stored, will pass pointer to
            //       addNodeAssociation which will return a value based on how many spaces there are left
            uint8_t nodeID[OpenTRV_Node_ID_Bytes]; // Buffer to store node ID // TODO replace with settable node size constant
            // Loop through tokens setting nodeID.
            // FIXME check for invalid ID bytes (i.e. containing 0xFF or non-HEX)
            char *thisTok = tok1;
            for(uint8_t i = 0; i < sizeof(nodeID); i++, thisTok = strtok_r(NULL, " ", &last))
                {
                // If token is valid, parse hex to binary.
                if(NULL != thisTok)
                    {
                    const int ib = OTV0P2BASE::parseHexByte(thisTok);
                    if((-1 == ib) || !OTV0P2BASE::validIDByte(ib)) { InvalidIgnored(); return(false); } // ERROR: abrupt exit.
                    nodeID[i] = (uint8_t) ib;
                    }
                }
            // Try to save this association to EEPROM, reporting result.
            const int8_t index = OTV0P2BASE::addNodeAssociation(nodeID);
            if(index >= 0)
                {
                Serial.print(F("Index "));
                Serial.println(index);
                }
            else
                { InvalidIgnored(); } // Full.
            }
        else { InvalidIgnored(); } // Indicate bad args.
        }
    else { InvalidIgnored(); } // Indicate bad args.
    return(false); // Don't print stats: may have done a lot of work...
    }

// Dump (human-friendly) stats (eg "D N").
// DEBUG only: "D?" to force partial stats sample and "D!" to force an immediate full stats sample; use with care.
// Avoid showing status afterwards as may already be rather a lot of output.
bool DumpStats::doCommand(char *const buf, const uint8_t buflen)
    {
#if 0 && defined(DEBUG)
    if(buflen == 2) // Sneaky way of forcing stats samples.
      {
      if('?' == buf[1]) { sampleStats(false); Serial.println(F("Part sample")); }
      else if('!' == buf[1]) { sampleStats(true); Serial.println(F("Full sample")); }
      break;
      }
#endif
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 3 character sequence makes sense and is safe to tokenise, eg "D 0".
    if((buflen >= 3) && (NULL != (tok1 = strtok_r(buf+2, " ", &last))))
      {
      const uint8_t setN = (uint8_t) atoi(tok1);
      const uint8_t thisHH = OTV0P2BASE::getHoursLT();
//          const uint8_t lastHH = (thisHH > 0) ? (thisHH-1) : 23;
      // Print label.
      switch(setN)
        {
        default: { Serial.print('?'); break; }
        case V0P2BASE_EE_STATS_SET_TEMP_BY_HOUR:
        case V0P2BASE_EE_STATS_SET_TEMP_BY_HOUR_SMOOTHED:
            { Serial.print('C'); break; }
        case V0P2BASE_EE_STATS_SET_AMBLIGHT_BY_HOUR:
        case V0P2BASE_EE_STATS_SET_AMBLIGHT_BY_HOUR_SMOOTHED:
            { Serial.print(F("ambl")); break; }
        case V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR:
        case V0P2BASE_EE_STATS_SET_OCCPC_BY_HOUR_SMOOTHED:
            { Serial.print(F("occ%")); break; }
        case V0P2BASE_EE_STATS_SET_RHPC_BY_HOUR:
        case V0P2BASE_EE_STATS_SET_RHPC_BY_HOUR_SMOOTHED:
            { Serial.print(F("RH%")); break; }
        case V0P2BASE_EE_STATS_SET_USER1_BY_HOUR:
        case V0P2BASE_EE_STATS_SET_USER1_BY_HOUR_SMOOTHED:
            { Serial.print('u'); break; }
#if defined(V0P2BASE_EE_STATS_SET_WARMMODE_BY_HOUR_OF_WK)
        case V0P2BASE_EE_STATS_SET_WARMMODE_BY_HOUR_OF_WK:
            { Serial.print('W'); break; }
#endif
        }
      Serial.print(' ');
      if(setN & 1) { Serial.print(F("smoothed")); } else { Serial.print(F("last")); }
      Serial.print(' ');
      // Now print values.
      for(uint8_t hh = 0; hh < 24; ++hh)
        {
        const uint8_t statRaw = OTV0P2BASE::getByHourStat(setN, hh);
        // For unset stat show '-'...
        if(OTV0P2BASE::STATS_UNSET_BYTE == statRaw) { Serial.print('-'); }
        // ...else print more human-friendly version of stat.
        else switch(setN)
          {
          default: { Serial.print(statRaw); break; } // Generic decimal stats.

          // Special formatting cases.
          case V0P2BASE_EE_STATS_SET_TEMP_BY_HOUR:
          case V0P2BASE_EE_STATS_SET_TEMP_BY_HOUR_SMOOTHED:
            // Uncompanded temperature, rounded.
            { Serial.print((expandTempC16(statRaw)+8) >> 4); break; }
#if defined(V0P2BASE_EE_STATS_SET_WARMMODE_BY_HOUR_OF_WK)
          case V0P2BASE_EE_STATS_SET_WARMMODE_BY_HOUR_OF_WK:
            // Warm mode usage bitmap by hour over week.
            { Serial.print(statRaw, HEX); break; }
#endif
          }
#if 0 && defined(DEBUG)
        // Show how many values are lower than the current one.
        Serial.print('(');
        Serial.print(OTV0P2BASE::countStatSamplesBelow(setN, statRaw));
        Serial.print(')');
#endif
        if(hh == thisHH) { Serial.print('<'); } // Highlight current stat in this set.
#if 0 && defined(DEBUG)
        if(inOutlierQuartile(false, setN, hh)) { Serial.print('B'); } // In bottom quartile.
        if(inOutlierQuartile(true, setN, hh)) { Serial.print('T'); } // In top quartile.
#endif
        Serial.print(' ');
        }
      Serial.println();
      }
    return(false);
    }

// Show/set generic parameter values (eg "G N [M]").
bool GenericParam::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 3 character sequence makes sense and is safe to tokenise, eg "D 0".
    if((buflen >= 3) && (NULL != (tok1 = strtok_r(buf+2, " ", &last))))
      {
      const uint8_t paramN = (uint8_t) atoi(tok1);
      // Reject if parameter number out of bounds.
      if(paramN > V0P2BASE_EE_LEN_RAW_INSPECTABLE) { InvalidIgnored(); return(false); }
      // Check for a second (new value) parameter.
      // If not present, print the current parameter value raw.
      char *tok2 = strtok_r(NULL, " ", &last);
      if(NULL == tok2)
        {
        // Print raw value in decimal.
        Serial.println(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_RAW_INSPECTABLE + paramN)));
        return(true);
        }
      // Set (decimal) parameter value.
      const int v = atoi(tok2);
      if((v < 0) || (v > 255)) { InvalidIgnored(); return(false); }
      const uint8_t vb = (uint8_t) v;
      eeprom_smart_update_byte((uint8_t *)(V0P2BASE_EE_START_RAW_INSPECTABLE + paramN), vb);
      return(true);
      }
    InvalidIgnored();
    return(false);
    }

// Show / reset node ID ('I').
bool NodeID::doCommand(char *const buf, const uint8_t buflen)
    {
    if((3 == buflen) && ('*' == buf[2]))
      { OTV0P2BASE::ensureIDCreated(true); } // Force ID change.
    Serial.print(F("ID:"));
    for(uint8_t i = 0; i < V0P2BASE_EE_LEN_ID; ++i)
      {
      Serial.print(' ');
      Serial.print(eeprom_read_byte((uint8_t *)(V0P2BASE_EE_START_ID + i)), HEX);
      }
    Serial.println();
    return(true);
    }

// Show / reset node ID ('I').
bool NodeIDWithSet::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Allow set: I nn nn nn nn nn nn nn nn
    if((buflen >= (1 + 3*OpenTRV_Node_ID_Bytes)) && (NULL != (tok1 = strtok_r(buf + 2, " ", &last))))
        {
        // TODO: merge with code for 'A' which also parses node IDs.
        // Corresponds to "I " followed by 8 space-separated hex-byte tokens.
        // Buffer to store node ID.
        uint8_t nodeID[OpenTRV_Node_ID_Bytes];
        // Loop through tokens setting nodeID.
        // Check for invalid ID bytes (i.e. containing 0xFF or <0x80 or non-HEX)
        char *thisTok = tok1;
        for(uint8_t i = 0; i < sizeof(nodeID); i++, thisTok = strtok_r(NULL, " ", &last))
            {
            // If token is valid, parse hex to binary.
            if(NULL != thisTok)
                {
                const int ib = OTV0P2BASE::parseHexByte(thisTok);
                if((-1 == ib) || !OTV0P2BASE::validIDByte(ib)) { InvalidIgnored(); return(false); } // ERROR: abrupt exit.
                nodeID[i] = (uint8_t) ib;
                }
            }
        // Try to write the ID directly to EEPROM.
        for(uint8_t i = 0; i < V0P2BASE_EE_LEN_ID; ++i)
            { eeprom_smart_update_byte((uint8_t *)(V0P2BASE_EE_START_ID + i), nodeID[i]); }
        Serial.println();
        return(true);
        }

    // Fall back to base class for display and random-reset ('*') behaviour.
    return(NodeID::doCommand(buf, buflen));
    }

// Set secret key ("K ...").
// "K B XX .. XX"  sets the primary building key, "K B *" erases it.
// Clearing a key conditionally resets the primary TX message counter to avoid IV reuse
// if a non-NULL callback has been provided.
// Note that this may take significant time and will mess with the CPU clock.
// TODO-907:
bool SetSecretKey::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 5 character sequence makes sense and is safe to tokenise, eg "K B *".
    if((buflen >= 5) && (NULL != (tok1 = strtok_r(buf + 2, " ", &last))))
        {
        if('B' == toupper(*tok1))
            {
            // If first token is a 'B'...
            char *tok2 = strtok_r(NULL, " ", &last);
            // If second token is '*' clears EEPROM.
            // Otherwise, test to see if a valid key has been entered.
            if(NULL != tok2)
                {
                if (*tok2 == '*')
                    {
                    OTV0P2BASE::setPrimaryBuilding16ByteSecretKey (NULL);
                    Serial.println(F("B clear"));
#if 0 && defined(DEBUG)
                    uint8_t keyTest[16];
                    OTV0P2BASE::getPrimaryBuilding16ByteSecretKey(keyTest);
                    for(uint8_t i = 0; i < sizeof(keyTest); i++)
                        {
                        Serial.print(keyTest[i], HEX);
                        Serial.print(" ");
                        }
                    Serial.println();
#endif
                    // Notify key cleared.
                    if(NULL != keysClearedFn) { keysClearedFn(); }
                    return(false);
                    }
                else if(buflen >= 3 + 2*16)
                    {
                    // "K B" + 16x " hh" or " h" tokens.
                    // 0 array to store new key
                    uint8_t newKey[OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY];
                    // parse and set first token, which has already been recovered
                    newKey[0] = OTV0P2BASE::parseHexByte(tok2);
                    // loop through rest of secret key
                    for (uint8_t i = 1; i < OTV0P2BASE::VOP2BASE_EE_LEN_16BYTE_PRIMARY_BUILDING_KEY; i++)
                        {
                        char *thisTok = strtok_r(NULL, " ", &last);
                        const int ib = OTV0P2BASE::parseHexByte(thisTok);
                        if(-1 == ib) { InvalidIgnored(); return(false); } // ERROR: abrupt exit.
                        newKey[i] = (uint8_t)ib;
                        }
                    if(OTV0P2BASE::setPrimaryBuilding16ByteSecretKey(newKey))
                        { Serial.println(F("B set")); }
                    else
                        { Serial.println(F("!B")); } // ERROR: key not set

#if 0 && defined(DEBUG)
                    uint8_t keyTest[16];
                    OTV0P2BASE::getPrimaryBuilding16ByteSecretKey(keyTest);
                    for(uint8_t i = 0; i < sizeof(keyTest); i++)
                        {
                        Serial.print(keyTest[i], HEX);
                        Serial.print(" ");
                        }
                    Serial.println();
#endif
                    return(false);
                    }
                }
            }
        }
    InvalidIgnored();
    return(false);
    }

// Set local time (eg "T HH MM").
bool SetTime::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 5 character sequence makes sense and is safe to tokenise, eg "T 1 2".
    if((buflen >= 5) && (NULL != (tok1 = strtok_r(buf+2, " ", &last))))
      {
      char *tok2 = strtok_r(NULL, " ", &last);
      if(NULL != tok2)
        {
        const int hh = atoi(tok1);
        const int mm = atoi(tok2);
        // TODO: zap collected stats if time change too large (eg >> 1h).
        if(!OTV0P2BASE::setHoursMinutesLT(hh, mm)) { OTV0P2BASE::CLI::InvalidIgnored(); }
        }
      }
    return(true);
    }

// Set TX privacy level ("X NN").
// Lower means less privacy: 0 means send everything, 255 means send as little as possible.
bool SetTXPrivacy::doCommand(char *const buf, const uint8_t buflen)
    {
    char *last; // Used by strtok_r().
    char *tok1;
    // Minimum 3 character sequence makes sense and is safe to tokenise, eg "X 0".
    if((buflen >= 3) && (NULL != (tok1 = strtok_r(buf+2, " ", &last))))
      {
      const uint8_t nn = (uint8_t) atoi(tok1);
      OTV0P2BASE::eeprom_smart_update_byte((uint8_t *)V0P2BASE_EE_START_STATS_TX_ENABLE, nn);
      }
    return(true);
    }

// Zap/erase learned statistics ('Z').
// Avoid showing status afterwards as may already be rather a lot of output.
bool ZapStats::doCommand(char *const, const uint8_t)
    {
    // Try to avoid causing an overrun if near the end of the minor cycle (even allowing for the warning message if unfinished!).
    if(OTV0P2BASE::zapStats((uint16_t) OTV0P2BASE::fnmax(1, ((int)OTV0P2BASE::msRemainingThisBasicCycle()/2) - 20)))
      { Serial.println(F("Zapped.")); }
    else
      { Serial.println(F("Not finished.")); }
    return(false); // May be slow; avoid showing stats line which will in any case be unchanged.
    }

#endif // ARDUINO_ARCH_AVR


} }
