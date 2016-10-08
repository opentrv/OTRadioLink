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
 * Driver for portable C++ unit tests for this library.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <utility/OTV0P2BASE_Util.h>

class ExampleClass
{
public:
	ExampleClass() : myVal(0) {};
	uint8_t doSomething(uint8_t input)
	{
		myVal += input;
		return (input + 20);
	}

	uint32_t myVal;
};


TEST(ExampleTest, ShouldAddTen)
{
	// setup class
	ExampleClass myClass;

	//run tests
	EXPECT_EQ(0, myClass.myVal);
//	EXPECT_EQ(1, myClass.myVal);  // If an EXPECT_ statement fails, the test carries on.
	EXPECT_EQ(23, myClass.doSomething(3));
//	ASSERT_EQ(0, myClass.myVal);  // If an ASSERT_ statement fails, the function exits.
	EXPECT_EQ(3, myClass.myVal);
}



/**
 * @brief   Getting started with the gtest libraries.
 * @note    - Add the following to Project>Properties>C/C++ Build>Settings>GCC G++ linker>Libraries (-l):
 *              - gtest
 *              - gtest_main
 *              - pthread
 *          - Select Google Testing in Run>Run Configuration>C/C++ Unit Test>testTest>C/C++ Testing and click Apply then Run
 *          - Saved the test config
 */
