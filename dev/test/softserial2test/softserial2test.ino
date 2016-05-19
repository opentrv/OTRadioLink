#include <OTV0p2Base.h>
#include <OTUnitTest.h>

OTV0P2BASE::OTSoftSerial2<8, 5> ser;

/**
 * @brief Test begin function.
 * @note  Make sure:
 *          - Pins set up correctly.
 *              - rxPin is input, pulled up.
 *              - txPin is output and set high.
 *          - delays are correct (HOW?)
 */
void test_setup()
{
  ser.begin(2400);
  // tests
}

/**
 * @brief Test end method functions correctly.
 * @note  Make sure txPin is set correctly (input, pulled up).
 */
void test_teardown()
{
  ser.end();
  // tests
}

/**
 * @brief Test write method returns correctly.
 * @note  Expected return value is 1.
 */
 void test_write()
 {
  Serial.println(__func__);
  // this should go in an assert is 1.
  assert(ser.write('a') == 1);
 }

 /**
  * @brief  Test peek returns correctly.
  * @note   Currently not implemented - Expected return is -1.
  */
 void test_peek()
 {
    Serial.println(__func__);
    // This should go in an assert is -1.
    assert(ser.peek() == -1);
 }

 /**
  * @brief  Test read returns correctly.
  * @note   Should return -1.
  * @todo   Work out how to test with characters (loopback?)
  */
void test_read()
{
    Serial.println(__func__);
    // This should go in an assert is -1
    assert(ser.read() == -1);
    // Further tests of whether it actually works...
}

/**
 * @brief Test if returns bool.
 * @note  Should return true.
 */
void test_bool()
{
  Serial.println(__func__);
  // This should go in assert true statement.
  assert(ser == true);
}

/**
 * @brief Test the availableForWrite return 0.
 * @note  This is not implemented in the lib as no async Tx planned.
 */
void test_availableForWrite()
{
    Serial.println(__func__);
    // Assert this is 0
    assert(ser.availableForWrite() == false);
}

/**
 * @brief Loopback test. Requires a serial device that will echo anything written.
 * @note  This test is written for the SIM900.
 */
void test_sim900Loopback()
{
    Serial.println(__func__);
    char buf[5];
    memset(buf, 0, sizeof(buf));
    ser.println("AT");
    for (uint8_t i = 0; i < sizeof(buf); i++) {
        buf[i] = ser.read();
    }
    assert(buf[0] == 'A');
    assert(buf[1] == 'T');
}

void setup() {
  Serial.begin(4800);
}

void loop() {
    // Copy looping stuff from unit tests.
    
    // Actual tests:
    OTUnitTest::begin(4800);
    test_setup(); // Not yet implemented.
    test_peek();
//    test_read(); // Commented as covered by 'loopback'
//    test_write();
    test_bool();
    test_availableForWrite();
    test_sim900Loopback();
    test_teardown();
    OTUnitTest::end();
}
