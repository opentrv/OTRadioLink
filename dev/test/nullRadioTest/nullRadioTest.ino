#include <SoftwareSerial.h>
#include <OTNullRadioLink.h>

Test<7, 8> test;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  test.begin(19200);
  Serial.println("go");
}

void loop() {
  // put your main code here, to run repeatedly:
  test.print("AT\r");
  while(test.available() > 0) {
    Serial.print(test.read());
  }
  Serial.println("\nRX end");
  delay(1000);
}
