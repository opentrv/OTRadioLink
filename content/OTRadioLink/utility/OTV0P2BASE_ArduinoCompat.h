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

// Enable minimal elements to support cross-compilation.
// NOT in normal OpenTRV namespace(s).

// F() macro on Arduino for hosting string constant in Flash, eg print(F("no space taken in RAM")).
#ifndef F
class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(string_literal))
#endif

// Minimal skeleton matching Print to permit at least compilation on non-Arduino platforms.
class Print
    {
    public:
        size_t println() { return(0); }
        size_t print(char) { return(0); }
        size_t println(char) { return(0); }
        size_t print(int, int = 10 /* DEC */) { return(0); }
        size_t println(int, int = 10 /* DEC */) { return(0); }
        size_t print(long, int = 10 /* DEC */) { return(0); }
        size_t println(long, int = 10 /* DEC */) { return(0); }
        size_t print(const char *) { return(0); }
        size_t println(const char *) { return(0); }
        size_t print(const __FlashStringHelper *) { return(0); }
        size_t println(const __FlashStringHelper *) { return(0); }
    };

#endif // ARDUINO


#endif
