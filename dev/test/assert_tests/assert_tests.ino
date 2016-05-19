#include <OTUnitTest.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin(4800);
  Serial.println(F("Start tests"));

}

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t x = 10;
  assert(6 != x);
  assert(6 == x);
}



