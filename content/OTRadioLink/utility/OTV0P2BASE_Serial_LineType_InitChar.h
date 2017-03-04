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

Author(s) / Copyright (s): Damon Hart-Davis 2016--2017
*/

/*
 Serial_LineType_InitChar definition.
 */

#ifndef OTV0P2BASE_SERIAL_LINETYPE_INITCHAR_H
#define OTV0P2BASE_SERIAL_LINETYPE_INITCHAR_H


namespace OTV0P2BASE
{


// Convention for V0p2 serial (eg FTDI) link indicating 'significant' lines.
// If the initial character on the line is one of the enumerated values
// then that implies that the entire line is for the described purpose.
// Eg, lines from a V0p2 unit starting with '!' can treated as an error log.
enum Serial_LineType_InitChar : uint8_t {
    // Reserved characters at the start of a line from V0p2
    // to an attached server/upstream system.
    SERLINE_START_CHAR_CLI = '>', // CLI prompt.
    SERLINE_START_CHAR_ERROR = '!', // Error log line.
    SERLINE_START_CHAR_WARNING = '?', // Warning log line.
    SERLINE_START_CHAR_INFO = '+', // Informational log line.
    SERLINE_START_CHAR_RSTATS = '@', // Remote (binary) stats log line.
    SERLINE_START_CHAR_RJSTATS = '{', // Remote (JSON) stats log line.
    SERLINE_START_CHAR_STATS = '=' // Local stats log line.
};


} // OTV0P2BASE

#endif // OTV0P2BASE_SERIAL_LINETYPE_INITCHAR_H
