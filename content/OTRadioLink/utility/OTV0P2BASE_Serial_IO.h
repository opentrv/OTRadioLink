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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
                           Deniz Erbilgin 2015
*/

/*
 Serial (USB) I/O.
 
 For a V0p2 board, write to the hardware serial,
 otherwise (assuming non-embedded) write to stdout.

 Also, simple debug output to the serial port at its default (bootloader BAUD) rate.

 The debug support only enabled if V0P2BASE_DEBUG is defined, else does nothing, or at least as little as possible.
 */

#ifndef OTV0P2BASE_SERIAL_IO_H
#define OTV0P2BASE_SERIAL_IO_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "utility/OTV0P2BASE_ArduinoCompat.h"
#endif

#include <OTV0p2Base.h>


namespace OTV0P2BASE
{


// Version (code/board) information printed as one line to serial (with line-end, and flushed); machine- and human- parseable.
// Format: "board VX.X REVY YYYY/Mmm/DD HH:MM:SS".
// Built as a macro to ensure __DATE__ and __TIME__ expanded in the scope of the caller.
#define V0p2Base_serialPrintlnBuildVersion() \
  do { \
  OTV0P2BASE::serialPrintAndFlush(F("board V0.2 REV")); \
  OTV0P2BASE::serialPrintAndFlush(V0p2_REV); \
  OTV0P2BASE::serialPrintAndFlush(' '); \
  /* Rearrange date into sensible most-significant-first order, and make it (nearly) fully numeric. */ \
  /* FIXME: would be better to have this in PROGMEM (Flash) rather than RAM, eg as F() constant. */ \
  const char _YYYYMmmDD[] = \
    { \
    __DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], \
    '/', \
    __DATE__[0], __DATE__[1], __DATE__[2], \
    '/', \
    ((' ' == __DATE__[4]) ? '0' : __DATE__[4]), __DATE__[5], \
    '\0' \
    }; \
  OTV0P2BASE::serialPrintAndFlush(_YYYYMmmDD); \
  OTV0P2BASE::serialPrintlnAndFlush(F(" " __TIME__)); \
  } while(false)


#ifndef V0P2BASE_DEBUG
#define V0P2BASE_DEBUG_SERIAL_PRINT(s) // Do nothing.
#define V0P2BASE_DEBUG_SERIAL_PRINTFMT(s, format) // Do nothing.
#define V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(fs) // Do nothing.
#define V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) // Do nothing.
#define V0P2BASE_DEBUG_SERIAL_PRINTLN() // Do nothing.
#define V0P2BASE_DEBUG_SERIAL_TIMESTAMP() // Do nothing.
#else

// Send simple string or numeric to serial port and wait for it to have been sent.
// Make sure that Serial.begin() has been invoked, etc.
#define V0P2BASE_DEBUG_SERIAL_PRINT(s) { OTV0P2BASE::serialPrintAndFlush(s); }
#define V0P2BASE_DEBUG_SERIAL_PRINTFMT(s, fmt) { OTV0P2BASE::serialPrintAndFlush((s), (fmt)); }
#define V0P2BASE_DEBUG_SERIAL_PRINT_FLASHSTRING(fs) { OTV0P2BASE::serialPrintAndFlush(F(fs)); }
#define V0P2BASE_DEBUG_SERIAL_PRINTLN_FLASHSTRING(fs) { OTV0P2BASE::serialPrintlnAndFlush(F(fs)); }
#define V0P2BASE_DEBUG_SERIAL_PRINTLN() { OTV0P2BASE::serialPrintlnAndFlush(); }
// Print timestamp with no newline in format: MinutesSinceMidnight:Seconds:SubCycleTime
//extern void _debug_serial_timestamp();
//#define V0P2BASE_DEBUG_SERIAL_TIMESTAMP() _debug_serial_timestamp()

#endif // V0P2BASE_DEBUG



// Conventions for data sent on V0p2 serial (eg FTDI) link to indicate 'significant' lines.
// If the initial character on the line is one of the following distinguished values
// then that implies that the entire line is for the described purpose.
// For example, lines from the V0p2 unit starting with '!' can treated as an error log.
enum Serial_LineType_InitChar {
    // Reserved characters at the start of a line from V0p2 to attached server/upstream system.
    SERLINE_START_CHAR_CLI = '>', // CLI prompt.
    SERLINE_START_CHAR_ERROR = '!', // Error log line.
    SERLINE_START_CHAR_WARNING = '?', // Warning log line.
    SERLINE_START_CHAR_INFO = '+', // Informational log line.
    SERLINE_START_CHAR_RSTATS = '@', // Remote (binary) stats log line.
    SERLINE_START_CHAR_RJSTATS = '{', // Remote (JSON) stats log line.
    SERLINE_START_CHAR_STATS = '=' // Local stats log line.
};


// Write a single (Flash-resident) string to serial followed by line-end and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintlnAndFlush(__FlashStringHelper const *line);

// Write a single (Flash-resident) string to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(__FlashStringHelper const *text);

// Write a single string to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(char const *text);

// Write a single (read-only) string to serial followed by line-end and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintlnAndFlush(char const *text);

// Write a single (Flash-resident) character to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(char c);

// Write a single (Flash-resident) number to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(int i, int fmt = 10); // Arduino print.h: #define DEC 10

// Write a single (Flash-resident) number to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(unsigned u, int fmt = 10); // Arduino print.h: #define DEC 10

// Write a single (Flash-resident) number to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(unsigned long u, int fmt = 10); // Arduino print.h: #define DEC 10

// Write line-end to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintlnAndFlush();

// Write a single (read-only) buffer of length len to serial followed by line-end and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialWriteAndFlush(char const *buf, uint8_t len);


} // OTV0P2BASE

#endif // OTV0P2BASE_SERIAL_IO_H
