/**
 * Sketch for testing OTV0P2BASE_Security.h library functions.
 **/

#include <OTV0p2Base.h>


// Example 16 byte Key for test.
static const uint8_t key[16] = { 0x00, 0x11, 0x22, 0x33,
                                 0x44, 0x55, 0x66, 0x77, 
                                 0x88, 0x99, 0xaa, 0xbb, 
                                 0xcc, 0xdd, 0xee, 0xe1 };

void setup() {
  // Start serial
  Serial.begin(4800);
  Serial.println(F("Begin"));
}

void loop() {
  // Check if there is anything in the serial buffer.
  if (Serial.available() > 0) {
    uint8_t c = Serial.read();
    // Decide what to do
    switch (c) {
      case 's':
      // Test the function for setting the key.
      Serial.print(F("Setting key to"));
      for (uint8_t i = 0; i < sizeof(key); i++) {
        Serial.print(" ");
        Serial.print(key[i], HEX);
      }
      Serial.println();
      OTV0P2BASE::setPrimaryBuilding16ByteSecretKey(key);  // FUNCTION BEING TESTED!
      break;
      case 'e':
      // Test the function for erasing the key.
      Serial.println(F("Erasing Key"));
      OTV0P2BASE::setPrimaryBuilding16ByteSecretKey(NULL); // FUNCTION BEING TESTED!
      break;
      case 'r':
      // Test the function for reading the key.
      Serial.println(F("Reading Key:"));
      {
        uint8_t readKey[16];
        OTV0P2BASE::getPrimaryBuilding16ByteSecretKey(readKey);  // FUNCTION BEING TESTED!
        for (uint8_t i = 0; i < sizeof(key); i++) {
          Serial.print(" ");
          Serial.print(readKey[i], HEX);
        }
        Serial.println();
      }
      break;
      case 'c':
      // Test the function for checking the key.
      Serial.print(F("Checking key: "));
      if (OTV0P2BASE::checkPrimaryBuilding16ByteSecretKey(key)) Serial.println(F("PASS"));  // FUNCTION BEING TESTED!
      else Serial.println(F("FAIL"));
      default:
      break;
    }
  }
}
