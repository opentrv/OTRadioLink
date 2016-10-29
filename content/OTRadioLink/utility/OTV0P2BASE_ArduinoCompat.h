/*
//The OpenTRV project licenses this file to you
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
 Basic compatibility support for Arduino and non-Arduino environments.
 */

#ifndef OTV0P2BASE_ARDUINOCOMPAT_H
#define OTV0P2BASE_ARDUINOCOMPAT_H


#ifndef ARDUINO

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Enable minimal elements to support cross-compilation.
// NOT in normal OpenTRV namespace(s).

// F() macro on Arduino for hosting string constant in Flash, eg print(F("no space taken in RAM")).
#ifndef F
class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(static_cast<const char *>(string_literal)))
#endif

// Minimal skeleton matching Print to permit at least compilation on non-Arduino platforms.
// Implementation is not necessarily efficient as assumed to be for (unit) test.
class Print
    {
    public:
        virtual size_t write(uint8_t) = 0;
        virtual size_t write(const uint8_t *buf, size_t size) { size_t n = 0; while((size-- > 0) && (0 != write(*buf++))) { ++n; } return(n); }
        size_t write(const char *buf, size_t size) { return write((const uint8_t *)buf, size); }
        size_t println() { return(write("\r\n", 2)); }
        size_t print(char c) { return(write(c)); }
        size_t println(char c) { const size_t n = print(c); return(n + println()); }
        size_t print(unsigned char uc, int b = 10 /* DEC */) { return(print((unsigned long)uc, b)); }
        size_t println(unsigned char uc, int b = 10 /* DEC */) { const size_t n = print(uc, b); return(n + println()); }
        size_t print(int i, int b = 10 /* DEC */) { return(print((long) i, b)); }
        size_t println(int i, int b = 10 /* DEC */) { const size_t n = print(i, b); return(n + println()); }
size_t print(long, int = 10 /* DEC */) { return(0); }
        size_t println(long l, int b = 10 /* DEC */) { const size_t n = print(l, b); return(n + println()); }
size_t print(unsigned long, int = 10 /* DEC */) { return(0); }
        size_t println(unsigned long ul, int b = 10 /* DEC */) { const size_t n = print(ul, b); return(n + println()); }
        size_t print(const char *s) { return(write(s, strlen(s))); }
        size_t println(const char *s) { const size_t n = print(s); return(n + println()); }
        size_t print(const __FlashStringHelper *f) { return(print(reinterpret_cast<const char *>(f))); }
        size_t println(const __FlashStringHelper *f) { const size_t n = print(f); return(n + println()); }
    };

#endif // ARDUINO


#endif
