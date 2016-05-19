#include <OTV0p2Base.h>
#include <OTUnitTest.h>

static const constexpr uint8_t rxPin = 8;
static const constexpr uint8_t txPin = 5;
static const constexpr uint16_t baud = 9600;

OTV0P2BASE::OTSoftSerial3<rxPin, txPin> ser;

/**
 * @brief Test begin function.
 * @note  Make sure:
 *          - Pins set up correctly.
 *              - rxPin is input, pulled up.
 *              - txPin is output and set high.
 *          - delays are correct (HOW?)
 * @todo  Check pins set to input/output.
 */
void test_begin()
{
    Serial.println(__func__);
    ser.begin(baud);
    assert(digitalRead(rxPin) == HIGH); 
    assert(digitalRead(txPin) == HIGH);
}

/**
 * @brief Test end method functions correctly.
 * @todo  Make sure txPin is set correctly (input, pulled up).
 */
void test_teardown()
{
  ser.end();
}

/**
 * @brief Test write method returns correctly.
 * @note  Expected return value is 1.
 */
 void test_write()
 {
  Serial.println(__func__);
  assert(ser.write('a') == 1);  // Test write
  assert(ser.println('a') == 3);  // Test println (prints "a\r\n")
 }

 /**
  * @brief  Test peek returns correctly.
  * @note   Currently not implemented - Expected return is -1.
  */
 void test_peek()
 {
    Serial.println(__func__);
    while(ser.available()) ser.read();  // Empty buffer
    assert(ser.peek() == -1);  // Test peek with empty buffer
 }

 /**
  * @brief  Test read returns correctly.
  * @note   Should return -1.
  */
void test_read()
{
    Serial.println(__func__);
    while(ser.available()) ser.read();  // Empty buffer
    assert(ser.read() == -1);
}

/**
 * @brief Test if returns bool.
 * @note  Should return true.
 */
void test_bool()
{
  Serial.println(__func__);
  assert(ser == true);  // Test ser object returns true to indicate it is available
}

/**
 * @brief Test the availableForWrite return 0.
 * @note  This is not implemented in the lib as no async Tx planned.
 */
void test_availableForWrite()
{
    Serial.println(__func__);
    assert(ser.availableForWrite() == 0);  // Will always return 0 as no write buffer.
}

/**
 * @brief Test that available works.
 */
void test_available()
{
    Serial.println(__func__);
    assert(ser.available() == 0);  // Will be zero for an empty buffer.
}

/**
 * @brief Loopback test. Requires a serial device that will echo anything written.
 * @note  This test is written for the SIM900.
 *        The test will normally fail due to the difference in function call overheads compared
 *        to when it is called from OTSIM900Link.
 *        Forcing ser.halfDelay to 8 and ser.fullDelay to 25 seems to work.
 */
void test_sim900Loopback()
{
    Serial.println(__func__);
    char buf[5];
    memset(buf, 0, sizeof(buf));
    ser.println("AT");
    delay(500);
    Serial.print("available: ");
    Serial.println(ser.available());
    Serial.print("peek: ");
    Serial.println(ser.peek(), HEX);
    assert(ser.peek() == 'A');
    for (uint8_t i = 0; i < sizeof(buf); i++) {
        buf[i] = ser.read();
    }
    Serial.print("read: ");  // Expecting 41 54 08 02?
    for (uint8_t i = 0; i < sizeof(buf); i++) {
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    assert(buf[0] == 'A');
    assert(buf[1] == 'T');
}


//// Interrupt count.  Marked volatile so safe to read without a lock as is a single byte.
//static volatile uint8_t intCountPB;
// Previous state of port B pins to help detect changes.
static volatile uint8_t prevStatePB;
// Interrupt service routine for PB I/O port transition changes.
ISR(PCINT0_vect)
{
//  ++intCountPB;
    const uint8_t pins = PINB;
    const uint8_t changes = pins ^ prevStatePB;
    prevStatePB = pins;

    if((changes & 1) && !(pins & 1)){  // only triggered on falling pin change
        ser.handle_interrupt();
    }

}


void setup() {
  Serial.begin(4800);

  // Set up interrupts:
  cli();
  PCMSK0 |= (1 << PCINT0); // Arduino digital pin 8
  PCICR |= (1 << PCIE0);
  sei();
}

void loop() {
    // Copy looping stuff from unit tests.
    
    // Actual tests:
    OTUnitTest::begin(4800);
    test_begin(); // Not yet implemented.
    test_available();
    test_write();
    test_peek();
    test_read(); // Commented as covered by 'loopback'
    test_bool();
    test_availableForWrite();
    test_sim900Loopback();
    test_teardown();
    OTUnitTest::end();
}
