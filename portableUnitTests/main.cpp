/**
 * @brief   Getting started with the gtest libraries.
 * @note    - Add the following to Project>Properties>C/C++ Build>Settings>GCC G++ linker>Libraries (-l):
 *              - gtest
 *              - gtest_main
 *              - pthread
 *          - Select Google Testing in Run>Run Configuration>C/C++ Unit Test>testTest>C/C++ Testing and click Apply then Run
 *          - Saved the test config
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <utility/OTV0P2BASE_Util.h>
//#include <utility/OTV0P2BASE_Util.cpp>


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


// Actually test a real OTRadioLink file.
TEST(LibraryTest,ShouldConvertFromHex)
{
    const char s[] = "0a";
    // This works. It's inline and only in the header.
    EXPECT_EQ(10, OTV0P2BASE::parseHexDigit(s[1]));
    // The compiler can't find this for some reason (function def in source file).
    EXPECT_EQ(10, OTV0P2BASE::parseHexByte(s));
}
