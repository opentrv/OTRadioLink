/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s):  Deniz Erbilgin 2016
*/

#ifndef ARDUINO_LIB_OTRADVALVE_VALVEMOTORDRV8850_H_
#define ARDUINO_LIB_OTRADVALVE_VALVEMOTORDRV8850_H_

#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTRadValve_ValveMotorBase.h"

namespace OTRadValve
{


#ifdef ValveMotorDirectV1HardwareDriverBase_DEFINED

/**
 * @class   DRV8850HardwareDriver
 * @brief   Implementation for the DRV8850 motor driver.
 * @note    IN1H + IN2L and IN1L + IN2H should be tied together and connected to MOTOR_DRIVE_ML_DigitalPin and MOTOR_DRIVE_MR_DigitalPin.
 *          This is in order to allow 2 pins to control the H-bridge.
 * @param   MOTOR_DRIVE_MI_AIN_DigitalPin: Current read. This is an ADCMUX number, not a pin number!
 * @param   MOTOR_DRIVE_MC_AIN_DigitalPin: Shaft encoder read. This is an ADCMUX number, not a pin number!
 * @param   MOTOR_DRIVE_ML_DigitalPin: H-Bridge control.
 * @param   MOTOR_DRIVE_MR_DigitalPin: H-Bridge control.
 * @param   nSLEEP: Sleep
 */
#define DRV8850HardwareDriver_DEFINED
template <uint8_t MOTOR_DRIVE_ML_DigitalPin, uint8_t MOTOR_DRIVE_MR_DigitalPin, uint8_t nSLEEP, uint8_t MOTOR_DRIVE_MI_AIN_DigitalPin, uint8_t MOTOR_DRIVE_MC_AIN_DigitalPin>
class DRV8850HardwareDriver : public ValveMotorDirectV1HardwareDriverBase
{
    // Last recorded direction.
    // Helpful to record shaft-encoder and other behaviour correctly around direction changes.
    // Marked volatile and stored as uint8_t to help thread-safety, and potentially save space.
    volatile uint8_t last_dir;
    // Temporary current limits, as values for REV7 H-Bridge are hard-coded in ValveMotorBase. These are expressed as ADC values.
    static const constexpr uint16_t maxDevCurrentReadingClosing = 300;  // FIXME
    static const constexpr uint16_t maxDevCurrentReadingOpening = 300;

public:
    DRV8850HardwareDriver() : last_dir((uint8_t)motorOff) { }

  // Detect if end-stop is reached or motor current otherwise very high.
  virtual bool isCurrentHigh(OTRadValve::HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const
    {
    // Check for high motor current indicating hitting an end-stop.
    // Measure motor current against (fixed) internal reference.
    const uint16_t mi = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL);
//#if 1 // 0 && defined(V0P2BASE_DEBUG)
//OTV0P2BASE::serialPrintAndFlush(F("    MI: "));
//OTV0P2BASE::serialPrintAndFlush(mi, DEC);
//OTV0P2BASE::serialPrintlnAndFlush();
//#endif
    const uint16_t miHigh = (OTRadValve::HardwareMotorDriverInterface::motorDriveClosing == mdir) ?
            maxDevCurrentReadingClosing : maxDevCurrentReadingOpening;
    const bool currentSense = (mi > miHigh); // &&
      // Recheck the value read in case spiky.
      // (OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL) > miHigh) &&
      // (OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL) > miHigh);
    return(currentSense);
    }

  // Poll simple shaft encoder output; true if on mark, false if not or if unused for this driver.
  virtual bool isOnShaftEncoderMark() const
    {
    // Power up IR emitter for shaft encoder and assume instant-on, as this has to be as fast as reasonably possible.
    OTV0P2BASE::power_intermittent_peripherals_enable();
//      const bool result = OTV0P2BASE::analogueVsBandgapRead(MOTOR_DRIVE_MC_AIN_DigitalPin, true);
    const uint16_t mc = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MC_AIN_DigitalPin, INTERNAL);
     const bool result = (mc < 120); // Arrrgh! FIXME: needs autocalibration during wiggle().
    OTV0P2BASE::power_intermittent_peripherals_disable();
#if 0 && defined(V0P2BASE_DEBUG)
OTV0P2BASE::serialPrintAndFlush(F("    MC: "));
OTV0P2BASE::serialPrintAndFlush(mc, DEC);
OTV0P2BASE::serialPrintlnAndFlush();
#endif
    return(result);
    }


  // Call to actually run/stop motor.
  // May take as much as (say) 200ms eg to change direction.
  // Stopping (removing power) should typically be very fast, << 100ms.
  //   * maxRunTicks  maximum sub-cycle ticks to attempt to run/spin for); zero will run for shortest reasonable time
  //   * dir  direction to run motor (or off/stop)
  //   * callback  callback handler
  virtual void motorRun(const uint8_t maxRunTicks,
                        const OTRadValve::HardwareMotorDriverInterface::motor_drive dir,
                        OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback)
    {
      // Remember previous state of motor.
    // This may help to correctly allow for (eg) position encoding inputs while a motor is slowing.
    const uint8_t prev_dir = last_dir;

    // Impossible to short the DRV8850 due to internal protection circuits.
    switch(dir)
      {
      case motorDriveClosing:
        {
		// Stop motor if running in the wrong direction
		// (Has no effect if motor is already running in the correct direction.)
		fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, LOW);
		// Wake DRV8850 if asleep
		fastDigitalWrite(nSLEEP, HIGH);
		// Let H-bridge respond and settle, and motor slow down if changing direction.
		// Otherwise there is a risk of browning out the device with a big current surge.
		if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
		fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, HIGH); // Run motor

		// Let H-bridge respond and settle and let motor run up.
		spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
		break; // Fall through to common case.
        }

      case motorDriveOpening:
        {
		// Stop motor if running in the wrong direction
		// (Has no effect if motor is already running in the correct direction.)
		fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, LOW);
		// Wake DRV8850 if asleep
		fastDigitalWrite(nSLEEP, HIGH);
		// Let H-bridge respond and settle, and motor slow down if changing direction.
		// Otherwise there is a risk of browning out the device with a big current surge.
		if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
		fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, HIGH); // Run motor

		// Let H-bridge respond and settle and let motor run up.
		spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
        break; // Fall through to common case.
        }

    case motorOff: default: // Explicit off, and default for safety.
    {
        // Everything off, unconditionally.
        // Motor is automatically stopped in sleep mode.
		fastDigitalWrite(nSLEEP, LOW);
		// Pull motor lines low to minimise current consumption (DRV8850 inputs are pulled low).
		fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, LOW);
		fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, LOW);

        // todo what is going on here?
        // Let H-bridge respond and settle.
        // Accumulate any shaft movement & time to the previous direction if not already stopped.
        // Wait longer if not previously off to allow for inertia, if shaft encoder is in use.
        const bool shaftEncoderInUse = false; // FIXME
        const bool wasOffBefore = (HardwareMotorDriverInterface::motorOff == prev_dir);
        const bool longerWait = shaftEncoderInUse || !wasOffBefore;
        spinSCTTicks(!longerWait ? minMotorHBridgeSettleTicks : minMotorRunupTicks, !longerWait ? 0 : minMotorRunupTicks/2, (motor_drive)prev_dir, callback);
        // Let H-bridge respond and settle.
        spinSCTTicks(minMotorHBridgeSettleTicks, 0, HardwareMotorDriverInterface::motorOff, callback);
        if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_60MS); } // Enforced low-power sleep on change of direction....
        break; // Fall through to common case.
        }
      }

    // Record new direction.
    last_dir = dir;
    }
};

// Actuator/driver for direct local (radiator) valve motor control.
template <uint8_t MOTOR_DRIVE_ML_DigitalPin, uint8_t MOTOR_DRIVE_MR_DigitalPin, uint8_t nSLEEP, uint8_t MOTOR_DRIVE_MI_AIN_DigitalPin, uint8_t MOTOR_DRIVE_MC_AIN_DigitalPin>
class DRV8850Driver final : public OTRadValve::AbstractRadValve
  {
  private:
    // Driver for the V1/DORM1 hardware.
    DRV8850HardwareDriver<MOTOR_DRIVE_ML_DigitalPin, MOTOR_DRIVE_MR_DigitalPin, nSLEEP, MOTOR_DRIVE_MI_AIN_DigitalPin, MOTOR_DRIVE_MC_AIN_DigitalPin> driver;
    // Logic to manage state, etc.
    CurrentSenseValveMotorDirect logic;

  public:
    DRV8850Driver(uint8_t minOpenPC = OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN,
                       uint8_t fairlyOpenPC = OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)
      : logic(&driver /* , lowBattOpt, minimiseActivityOpt, minOpenPC, fairlyOpenPC */ ) { } // FIXME: args wrong generally!

    // Regular poll/update.
    // This and get() return the actual estimated valve position.
    virtual uint8_t read()
      {
      logic.poll();
      value = logic.getCurrentPC();
      return(value);
      }

    // Set new target %-open value (if in range).
    // Returns true if the specified value is accepted.
    virtual bool set(const uint8_t newValue)
      {
      if(newValue > 100) { return(false); }
      logic.setTargetPC(newValue);
      return(true);
      }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    virtual uint8_t getMinPercentOpen() const { return(logic.getMinPercentOpen()); }

    // Call when given user signal that valve has been fitted (ie is fully on).
    virtual void signalValveFitted() { logic.signalValveFitted(); }

    // Waiting for indication that the valve head has been fitted to the tail.
    virtual bool isWaitingForValveToBeFitted() const { return(logic.isWaitingForValveToBeFitted()); }

    // Returns true if not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    virtual bool isInNormalRunState() const { return(logic.isInNormalRunState()); }

    // Returns true if in an error state,
    virtual bool isInErrorState() const { return(logic.isInErrorState()); }

    // Minimally wiggles the motor to give tactile feedback and/or show to be working.
    // May take a significant fraction of a second.
    // Finishes with the motor turned off, and a bias to closing the valve.
    virtual void wiggle() { logic.wiggle(); }
  };
#endif // ValveMotorDirectV1HardwareDriverBase_DEFINED

}



#endif /* ARDUINO_LIB_OTRADVALVE_VALVEMOTORDRV8850_H_ */
