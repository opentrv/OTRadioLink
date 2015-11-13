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

void setup() {
  // put your setup code here, to run once:
  Serial.begin(4800);
  ser.begin(initialBaud);

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
  char baud[7];
  memset(baud, 0, sizeof(baud));
  Serial.println("\n++ Set Baud ++");
  Serial.print("Enter Baud rate followed by '\\r': ");
  for(uint8_t i = 0; i < (sizeof(baud)-1); i++) {
    char c = 0;
    while(!Serial.available()); // block until serial available

    if(Serial.available() > 0) c = Serial.read();

    if(c == '\r') break;
    else if (c >= '0' && c <= '9') {
      baud[i] = c;
    } //else return;
  }
  Serial.println(baud);

  ser.write(setBaudCommand, sizeof(setBaudCommand)-1);
  ser.print("=");
  ser.println(baud);

  ser.begin(atoi(baud));

  //getRate();
}

