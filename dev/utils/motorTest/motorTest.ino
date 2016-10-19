/**
 * @brief Test the DRV8850 for basic functioning using a REV11.
 */

#include <OTV0p2Base.h>
#include <OTRadValve.h>

/** 
 * @note  Define pin functions. Note this is using the single row header.
 *        Counting from end closest to ATMega:
 *        Pin | Port
 *        1   | A1 
 *        2   | A2
 *        3   | A3
 *        4   | A6 (ADC only)
 *        5   | A7 (ADC only)
 *        6   | D3
 *        7   | D5
 *        8   | D6 (PWM)
 *        9   | D7 (PWM)
 *        10  | D2 (one-wire)
 *        11  | Vcc
 *        12  | GND
 */

#define V0p2_REV 11
#define WAKEUP_32768HZ_XTAL

static const constexpr uint8_t motorLeft = 5;
static const constexpr uint8_t motorRight = 6;
static const constexpr uint8_t motorCurrent = A1;
static const constexpr uint8_t motorEncoder = A2;
static const constexpr uint8_t motorNSleep = 7;

OTRadValve::DRV8850HardwareDriver<motorLeft, motorRight, motorNSleep, 1, motorEncoder> motorDriver;
//OTRadValve::ValveMotorDirectV1HardwareDriver<motorLeft, motorRight, motorCurrent, motorEncoder> motorDriver;
OTRadValve::TestValveMotor motor((OTRadValve::HardwareMotorDriverInterface *)(&motorDriver));

void setup() {
  Serial.begin(4800);
//  OTV0P2BASE::powerSetup();
  pinMode(motorCurrent, INPUT);
  pinMode(motorEncoder, INPUT);
  pinMode(motorLeft, OUTPUT);
  pinMode(motorRight, OUTPUT);
  pinMode(motorNSleep, OUTPUT);
//  digitalWrite(motorNSleep, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
//  digitalWrite(motorLeft, LOW);
//  digitalWrite(motorRight, HIGH);
  motor.poll();
//  Serial.println(analogRead(A1));
  delay(100);
}
