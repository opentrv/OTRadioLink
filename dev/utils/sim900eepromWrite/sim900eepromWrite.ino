#include <avr/eeprom.h>

/**
 * @brief   Writes OTSIM900Link config values to radioConfig block. Intended for atmega328p.
 * @note    This is intended for use with V0p2_main and assumes a radioConfig start address of 768. This can be changed by updating eepromStart
 *          Make sure config values are less than 256 bytes in total.
 * How to use:
 * 1- Set pin, apn and udp values
 * 2- Upload to atmega328p
 * 3- Program will update eeprom to new config values. It will only write if eeprom contains different values.
 * 4- Program will print size of eeprom block used and what is written to serial.
 */

static const uint16_t eepromStart = 768; // Do not change
static const uint8_t pin[] = "0000";
static const uint8_t apn[] = "m2mkit.telefonica.com";
static const uint8_t udpAddr[] = "46.101.64.191";
static const uint8_t udpPort[] = "9999";


void setup() {
  // put your setup code here, to run once:
  Serial.begin(4800);
  Serial.println(F("Writing to eeprom"));
  uint16_t eepromSize = 0;
  uint16_t eepromPtr = eepromStart;

  
  eeprom_update_block(pin, (void *)eepromPtr, sizeof(pin));
  eepromPtr += sizeof(pin);
  eeprom_update_block(apn, (void *)eepromPtr, sizeof(apn));
  eepromPtr += sizeof(apn);
  eeprom_update_block(udpAddr, (void *)eepromPtr, sizeof(udpAddr));
  eepromPtr += sizeof(udpAddr);
  eeprom_update_block(udpPort, (void *)eepromPtr, sizeof(udpPort));

  eepromSize = eepromPtr-eepromStart;

  Serial.print(eepromSize);
  Serial.println(F(" bytes of EEPROM used"));

  uint8_t eepromRead[255];
  eeprom_read_block(eepromRead, (void *)eepromStart, 255);
  uint16_t count = eepromStart;
  Serial.print("Location ");
  Serial.print(count, HEX);
  Serial.print("\t");
  for (uint16_t i = 0; i < 255; i++) {
    char c = (char) eepromRead[i];
    if(c != '\0') Serial.print(c);
    else {
      Serial.println();
      Serial.print("Location ");
      Serial.print(count, HEX);
      Serial.print("\t");
    }
    count++;
  }
 /* Serial.println();
  for (uint16_t i = 0; i < 255; i++) {
    uint8_t c = (uint8_t) eepromRead[i];
    Serial.print(c, HEX);
    Serial.print(" ");
  }*/
}

void loop() {
  // put your main code here, to run repeatedly:

}
