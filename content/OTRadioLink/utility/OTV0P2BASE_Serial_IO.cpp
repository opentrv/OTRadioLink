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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
                           Deniz Erbilgin 2015
*/

/*
 Simple debug output to the serial port at its default (bootloader BAUD) rate.

 Only enabled if V0P2BASE_DEBUG is defined, else does nothing, or at least as little as possible.
 
 See some other possibilities here: http://playground.arduino.cc/Main/Printf
 */

#include <stdio.h>

#include "OTV0P2BASE_Serial_IO.h"

#ifdef EFR32FG1P133F256GM48
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"
#endif  // EFR32FG1P133F256GM48

namespace OTV0P2BASE
{
#ifdef ARDUINO
// Default baud for V0p2 hardware serial, including bootloading.
static const uint16_t V0p2_DEFAULT_UART_BAUD = 4800;
#endif

// Flush to use for all serialPrintXXX() and V0P2BASE_DEBUG_PRINTXXX routines.
#ifdef ARDUINO
#define _flush() flushSerialSCTSensitive() // FIXME
#else
#define _flush() fflush(stdout)
#endif


#ifdef EFR32FG1P133F256GM48
// Start up serial dev
void PrintEFR32::setup(const uint32_t baud)
{
    CMU_ClockEnable(cmuClock_GPIO, true);
    // /* USART is a HFPERCLK peripheral. Enable HFPERCLK domain and USART0.
    // * We also need to enable the clock for GPIO to configure pins. */
    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_USART0, true);

    // /* Initialize with default settings and then update fields according to application requirements. */
    USART_InitAsync_TypeDef initAsync = USART_INITASYNC_DEFAULT;
    initAsync.baudrate = baud;
    USART_InitAsync(dev, &initAsync);

    // /* Enable I/O and set location */
    dev->ROUTEPEN |= USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN;
    dev->ROUTELOC0 = (dev->ROUTELOC0
                        & ~(_USART_ROUTELOC0_TXLOC_MASK
                            | _USART_ROUTELOC0_RXLOC_MASK))
                        | (outputNo << _USART_ROUTELOC0_TXLOC_SHIFT)
                        | (outputNo << _USART_ROUTELOC0_RXLOC_SHIFT);
    /* To avoid false start, configure TX pin as initial high */
    GPIO_PinModeSet((GPIO_Port_TypeDef)AF_USART0_TX_PORT(outputNo), AF_USART0_TX_PIN(outputNo), gpioModePushPull, 1);
    GPIO_PinModeSet((GPIO_Port_TypeDef)AF_USART0_RX_PORT(outputNo), AF_USART0_RX_PIN(outputNo), gpioModeInput, 0);
    isSetup = true;
}
size_t PrintEFR32::write(uint8_t c)
{
    // Prevent blocking if UART has not been set up.
    if (!isSetup) return true;
    USART_Tx(dev, c);
    return false;
}
size_t PrintEFR32::write(const uint8_t *buf, size_t len)
{
    if ((nullptr == buf) || (!isSetup)) return -1;
    for (auto *ip = buf; ip != buf+len; ++ip) USART_Tx(dev, *ip);
    return len;
}
PrintEFR32 Serial;
#endif // #ifdef EFR32FG1P133F256GM48



// Write a single (Flash-resident) string to serial followed by line-end and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintlnAndFlush(__FlashStringHelper const * const line)
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the line of text followed by line end.
  Serial.println(line);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  puts((const char *)line);
  putchar('\n');
  _flush();
#endif
  }

// Write a single (Flash-resident) string to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(__FlashStringHelper const * const text)
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the text.
  Serial.print(text);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  printf("%s", ((const char *)text));
  _flush();
#endif
  }

// Write a single (read-only) string to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(const char * const text)
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the text.
  Serial.print(text);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  printf("%s", text);
  _flush();
#endif
  }

// Write a single (read-only) string to serial followed by line-end and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintlnAndFlush(const char * const line)
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the line of text followed by line end.
  Serial.println(line);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  puts((const char *)line);
  putchar('\n');
  _flush();
#endif
  }

// Write a single (Flash-resident) character to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintAndFlush(const char c)
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the character.
  Serial.print(c);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  putchar(c);
  _flush();
#endif
  }

// Write a single (Flash-resident) number to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
#ifdef ARDUINO
void serialPrintAndFlush(const int i, const uint8_t fmt)
  {
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the character.
  Serial.print(i, fmt);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
  }
#else
void serialPrintAndFlush(const int i, const uint8_t /*fmt*/)
  {
  printf("%d", i); // FIXME: ignores fmt
  _flush();
  }
#endif

// Write a single (Flash-resident) number to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
#ifdef ARDUINO
void serialPrintAndFlush(const unsigned u, const uint8_t fmt)
  {
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the character.
  Serial.print(u, fmt);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
  }
#else
void serialPrintAndFlush(const unsigned u, const uint8_t /*fmt*/)
  {
  printf("%u", u); // FIXME: ignores fmt
  _flush();
  }
#endif

// Write a single (Flash-resident) number to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
#ifdef ARDUINO
void serialPrintAndFlush(const unsigned long u, const uint8_t fmt)
  {
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the character.
  Serial.print(u, fmt);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
  }
#else
void serialPrintAndFlush(const unsigned long u, const uint8_t /*fmt*/)
  {
  printf("%lu", u); // FIXME: ignores fmt
  _flush();
  }
#endif


// Write line-end to serial and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialPrintlnAndFlush()
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the text.
  Serial.println();
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  putchar('\n');
  _flush();
#endif
  }

// Write a single (read-only) buffer of length len to serial followed by line-end and wait for transmission to complete.
// This enables the serial if required and shuts it down afterwards if it wasn't enabled.
void serialWriteAndFlush(const char* buf, const uint8_t len)
  {
#ifdef ARDUINO
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Send the character.
  Serial.write(buf, len);
  // Ensure that all text is sent before this routine returns, in case any sleep/powerdown follows that kills the UART.
  _flush();
  if(neededWaking) { powerDownSerial(); }
#else
  fwrite(buf, 1, len, stdout);
  _flush();
#endif
  }


#ifdef V0P2BASE_DEBUG // Don't emit debug-support code unless in V0P2BASE_DEBUG.

// Print timestamp with no newline in format: MinutesSinceMidnight:Seconds:SubCycleTime
/*void _debug_serial_timestamp()	// FIXME This getSubCycleTime breaks in unit tests
  {
  const bool neededWaking = powerUpSerialIfDisabled<V0p2_DEFAULT_UART_BAUD>();
  // Grab time values ASAP, fastest-incrementing first.
  // TODO: could lock out interrupts to capture atomically.
  const uint8_t ss = getSubCycleTime();
  const uint8_t s = OTV0P2BASE::getSecondsLT();
  const uint16_t m = OTV0P2BASE::getMinutesSinceMidnightLT();
  Serial.print(m);
  Serial.print(':'); Serial.print(s);
  Serial.print(':'); Serial.print(ss);
  _flush();
  if(neededWaking) { powerDownSerial(); }
  }
*/
#endif // V0P2BASE_DEBUG

} // OTV0P2BASE
