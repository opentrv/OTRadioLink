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

// CLI support routines

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_CLI_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_CLI_H_

#include <stdint.h>
#include <Arduino.h>


namespace OTV0P2BASE {


    // Base CLI entry.
    // If derived classes don't need to retain state
    // then they can be created on the fly to handle commands and destroyed afterwards.
    class CLIEntryBase
        {
        public:
            // Run the command as selected by the command letter.
            // If this returns false then suppress the default status response and print "OK" instead.
            virtual bool doCommand(char *buf, uint8_t buflen) = 0;
        };


namespace CLI {


    // Prints warning to serial (that must be up and running) that invalid (CLI) input has been ignored.
    // Probably should not be inlined, to avoid creating duplicate strings in Flash.
    void InvalidIgnored();


    // Standard/common CLI command implementations
    //--------------------------------------------

    // Set / clear node association(s) (nodes to accept frames from) (eg "A hh hh hh hh hh hh hh hh").
    class SetNodeAssoc : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

    // Dump (human-friendly) stats (eg "D N").
    class DumpStats : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

    // Used to show or reset node ID (eg "I").
    //
    // Set or display new random ID.
    // Set only if the command line is (nearly) exactly "I *" to avoid accidental reset.
    // In either cas display the current one.
    // Should possibly restart the system afterwards.
    //
    // Example use:
    //
    //>I
    //ID: 98 A4 F5 99 E3 94 A8 C2
    //=F0%@18C6;X0;T15 38 W255 0 F255 0 W255 0 F255 0;S6 6 16;{"@":"98a4","L":146,"B|cV":333,"occ|%":0,"vC|%":0}
    //
    //>I
    //ID: 98 A4 F5 99 E3 94 A8 C2
    //=F0%@18C6;X0;T15 38 W255 0 F255 0 W255 0 F255 0;S6 6 16;{"@":"98a4","L":146,"B|cV":333,"occ|%":0,"vC|%":0}
    //
    //>I *
    //Setting ID byte 0 9F
    //Setting ID byte 1 9C
    //Setting ID byte 2 8B
    //Setting ID byte 3 B2
    //Setting ID byte 4 A0
    //Setting ID byte 5 E2
    //Setting ID byte 6 E2
    //Setting ID byte 7 AF
    //ID: 9F 9C 8B B2 A0 E2 E2 AF
    //=F0%@18C6;X0;T15 38 W255 0 F255 0 W255 0 F255 0;S6 6 16;{"@":"9f9c","L":146,"B|cV":333,"occ|%":0,"vC|%":0}
    //
    //>
    class NodeID : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

    // Set secret key ("K ...").
    class SetSecretKey : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

    // Set local time (eg "T HH MM").
    class SetTime : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

    // Set TX privacy level (eg "X NN").
    class SetTXPrivacy : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

    // Zap/erase learned statistics (eg "Z").
    class ZapStats : public CLIEntryBase { public: virtual bool doCommand(char *buf, uint8_t buflen); };

} }



#endif /* CONTENT_OTRADIOLINK_UTILITY_OTV0P2BASE_CLI_H_ */
