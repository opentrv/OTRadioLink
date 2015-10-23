#include <OTV0p2Base.h>

OTV0P2BASE::OTSoftSerial ser(7, 8);
//SoftwareSerial ser(7,8);
static const char sendStr1[] = "AT\n";
static const uint8_t pwr_pin = 6;

static const uint16_t baud = 4800;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(baud);
  ser.begin();  // defaults to 2400. This is what sim900 is currently set to.
  Serial.println("Start Program");
}

void loop() {
  // put your main code here, to run repeatedly:
  
  if(Serial.available() > 0) {
    char input = Serial.read();
    Serial.println(input);
    if(input == 'a') {
      Serial.println("Go");
      uint8_t i = 5;
      uint8_t val[6];
      memset(val, 0, sizeof(val));
      
      sendAT();

      Serial.print(ser.read(val, sizeof(val)-1));
      Serial.print("\t:");

      Serial.println((char *)val);
    }
  }
}

void sendAT()
{
  ser.write(sendStr1, sizeof(sendStr1)-1);
}

void onOff()
{
  digitalWrite(pwr_pin, HIGH);
  delay(1000);
  digitalWrite(pwr_pin, LOW);
}

