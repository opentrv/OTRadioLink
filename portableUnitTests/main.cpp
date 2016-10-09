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
 * Driver and sanity test for portable C++ unit tests for this library.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <utility/OTV0P2BASE_Util.h>


// Sanity test.
TEST(SanityTest,SelfTest)
{
    EXPECT_EQ(42, 42);
//    fputs("*** Tests built: " __DATE__ " " __TIME__ "\n", stderr);
}

//// Minimally test a real library function.
//TEST(SanityTest,ShouldConvertFromHex)
//{
//    const char s[] = "0a";
//    // This works. It's inline and only in the header.
//    EXPECT_EQ(10, OTV0P2BASE::parseHexDigit(s[1]));
//    // The compiler can't find this for some reason (function def in source file).
//    EXPECT_EQ(10, OTV0P2BASE::parseHexByte(s));
//}



/**
 * @brief   Getting started with the gtest libraries.
 * @note    - Add the following to Project>Properties>C/C++ Build>Settings>GCC G++ linker>Libraries (-l):
 *              - gtest
 *              - gtest_main
 *              - pthread
 *          - Select Google Testing in Run>Run Configuration>C/C++ Unit Test>testTest>C/C++ Testing and click Apply then Run
 *          - Saved the test config
 */

 /**
  * See also: https://github.com/google/googletest/blob/master/googletest/docs/Primer.md
  */
