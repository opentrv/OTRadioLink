#include <SoftwareSerial.h>

/**
 * How to use:
 * *** Use Arduino. V0p2 does not have high enough clock speed to run this program reliably ***
 * Set initialBaud to current SIM900 baud, or to 19200 if on autobaud
 * 'l' lists supported bauds
 * 'r' displays current baud
 * 's' sets a new baud
 */

SoftwareSerial ser(7,8);

static const char setBaudCommand[] = "AT+IPR";
static const uint16_t initialBaud = 19200;
static const uint16_t targetBaud  = 2400;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(4800);
  ser.begin(initialBaud);

  // power up shield
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
  delay(500);
  digitalWrite(9, HIGH);
  delay(500);
  digitalWrite(9, LOW);
  delay(3000);
  while(ser.available() > 0) ser.read();

  Serial.println("Type 's' to set baud");
  getBaud();
}

void loop() {
  // put your main code here, to run repeatedly:
  char c = 0;
  if (Serial.available() > 0) c = Serial.read();
  if (c == 'l') getPossibleBauds();
  else if (c == 'r') getBaud();
  else if (c == 's') setBaud();
  
  if (ser.available() > 0) Serial.print((char)ser.read());
}

void getPossibleBauds()
{
  Serial.println("\n++ List Supported Bauds ++");
  ser.write(setBaudCommand, sizeof(setBaudCommand)-1);
  ser.println("=?");
}

void getBaud()
{
  Serial.println("\n++ Current Baud ++");
  ser.write(setBaudCommand, sizeof(setBaudCommand)-1);
  ser.println("?");
}

void setBaud()
{
  Serial.print("\n++ Setting Baud to ");
  Serial.print(targetBaud);
  Serial.println(" ++");

  ser.write(setBaudCommand, sizeof(setBaudCommand)-1);
  ser.print("=");
  ser.println(targetBaud);

  ser.begin(targetBaud);

  //getRate();
}

