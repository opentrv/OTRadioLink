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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
                            Deniz Erbilgin 2016
*/

/*
 * Driver for DORM1/REV7 direct motor drive.
 *
 * V0p2/AVR only.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_VALVEMOTORDIRECTV1_H
#define ARDUINO_LIB_OTRADVALVE_VALVEMOTORDIRECTV1_H


#include <stddef.h>
#include <stdint.h>
#include <OTV0p2Base.h>
#include "OTRadValve_ValveMotorBase.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


#ifdef ARDUINO_ARCH_AVR

// Implementation for V1 (REV7/DORM1) motor.
// Usually not instantiated except within ValveMotorDirectV1.
// Creating multiple instances (trying to drive same motor) almost certainly a BAD IDEA.
#define ValveMotorDirectV1HardwareDriver_DEFINED
template <uint8_t MOTOR_DRIVE_ML_DigitalPin, uint8_t MOTOR_DRIVE_MR_DigitalPin, uint8_t MOTOR_DRIVE_MI_AIN_DigitalPin, uint8_t MOTOR_DRIVE_MC_AIN_DigitalPin>
class ValveMotorDirectV1HardwareDriver final : public ValveMotorDirectV1HardwareDriverBase
  {
    // Last recorded direction.
    // Helpful to record shaft-encoder and other behaviour correctly around direction changes.
    // Marked volatile and stored as uint8_t to help thread-safety, and potentially save space.
    volatile uint8_t last_dir;

  public:
    ValveMotorDirectV1HardwareDriver() : last_dir((uint8_t)motorOff) { }

    // Detect if end-stop is reached or motor current otherwise very high.
    virtual bool isCurrentHigh(OTRadValve::HardwareMotorDriverInterface::motor_drive mdir = motorDriveOpening) const
      {
      // Check for high motor current indicating hitting an end-stop.
      // Measure motor current against (fixed) internal reference.
      const uint16_t mi = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, INTERNAL);
//      const uint16_t mi = OTV0P2BASE::analogueNoiseReducedRead(MOTOR_DRIVE_MI_AIN_DigitalPin, DEFAULT);
//#if 1 // 0 && defined(V0P2BASE_DEBUG)
//OTV0P2BASE::serialPrintAndFlush(F("    MI: "));
//OTV0P2BASE::serialPrintAndFlush(mi, DEC);
//OTV0P2BASE::serialPrintlnAndFlush();
//#endif
      const uint16_t miHigh = (OTRadValve::HardwareMotorDriverInterface::motorDriveClosing == mdir) ?
          maxCurrentReadingClosing : maxCurrentReadingOpening;
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
    // DHD20160105: note that for REV7/DORM1 with ~2.4V+ battery, H-bridge drive itself ~20mA+, motor ~200mA.
    virtual void motorRun(const uint8_t maxRunTicks,
                          const OTRadValve::HardwareMotorDriverInterface::motor_drive dir,
                          OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback)
      {
      // Remember previous state of motor.
      // This may help to correctly allow for (eg) position encoding inputs while a motor is slowing.
      const uint8_t prev_dir = last_dir;

      // *** MUST NEVER HAVE L AND R LOW AT THE SAME TIME else board may be destroyed at worst. ***
      // Operates as quickly as reasonably possible, eg to move to stall detection quickly...
      // TODO: consider making atomic to block some interrupt-related accidents...
      // TODO: note that the mapping between L/R and open/close not yet defined.
      // DHD20150205: 1st cut REV7 all-in-in-valve, seen looking down from valve into base, cw => close (ML=HIGH), ccw = open (MR=HIGH).
      switch(dir)
        {
        case motorDriveClosing:
          {
          // Pull one side high immediately *FIRST* for safety.
          // Stops motor if other side is not already low.
          // (Has no effect if motor is already running in the correct direction.)
          fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, HIGH);
          pinMode(MOTOR_DRIVE_ML_DigitalPin, OUTPUT); // Ensure that the HIGH side is an output (can be done after, as else will be safe weak pull-up).
          // Let H-bridge respond and settle, and motor slow down if changing direction.
          // Otherwise there is a risk of browning out the device with a big current surge.
          if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
          pinMode(MOTOR_DRIVE_MR_DigitalPin, OUTPUT); // Ensure that the LOW side is an output.
          fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, LOW); // Pull LOW last.
          // Let H-bridge respond and settle and let motor run up.
          spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
          break; // Fall through to common case.
          }

        case motorDriveOpening:
          {
          // Pull one side high immediately *FIRST* for safety.
          // Stops motor if other side is not already low.
          // (Has no effect if motor is already running in the correct direction.)
          fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, HIGH);
          pinMode(MOTOR_DRIVE_MR_DigitalPin, OUTPUT); // Ensure that the HIGH side is an output (can be done after, as else will be safe weak pull-up).
          // Let H-bridge respond and settle, and motor slow down if changing direction.
          // Otherwise there is a risk of browning out the device with a big current surge.
          if(prev_dir != dir) { OTV0P2BASE::nap(WDTO_120MS); } // Enforced low-power sleep on change of direction....
          pinMode(MOTOR_DRIVE_ML_DigitalPin, OUTPUT); // Ensure that the LOW side is an output.
          fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, LOW); // Pull LOW last.
          // Let H-bridge respond and settle and let motor run up.
          spinSCTTicks(max(maxRunTicks, minMotorRunupTicks), minMotorRunupTicks, dir, callback);
          break; // Fall through to common case.
          }

        case motorOff: default: // Explicit off, and default for safety.
          {
          // Everything off, unconditionally.
          //
          // Turn one side of bridge off ASAP.
          fastDigitalWrite(MOTOR_DRIVE_MR_DigitalPin, HIGH); // Belt and braces force pin logical output state high.
          pinMode(MOTOR_DRIVE_MR_DigitalPin, INPUT_PULLUP); // Switch to weak pull-up; slow but possibly marginally safer.
          // Let H-bridge respond and settle.
          // Accumulate any shaft movement & time to the previous direction if not already stopped.
          // Wait longer if not previously off to allow for inertia, if shaft encoder is in use.
          const bool shaftEncoderInUse = false; // FIXME.
          const bool wasOffBefore = (HardwareMotorDriverInterface::motorOff == prev_dir);
          const bool longerWait = shaftEncoderInUse || !wasOffBefore;
          spinSCTTicks(!longerWait ? minMotorHBridgeSettleTicks : minMotorRunupTicks, !longerWait ? 0 : minMotorRunupTicks/2, (motor_drive)prev_dir, callback);
          fastDigitalWrite(MOTOR_DRIVE_ML_DigitalPin, HIGH); // Belt and braces force pin logical output state high.
          pinMode(MOTOR_DRIVE_ML_DigitalPin, INPUT_PULLUP); // Switch to weak pull-up; slow but possibly marginally safer.
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

#define ValveMotorDirectV1_DEFINED
// Actuator/driver for direct local (radiator) valve motor control.
//   * lowBattOpt  allows monitoring of supply voltage to avoid some activities with low batteries; can be NULL
//   * minimiseActivityOpt  callback returns true if unnecessary activity should be suppressed
//     to avoid disturbing occupants, eg when room dark and occupants may be sleeping; can be NULL
//   * binaryOnly  if true, use simplified valve control logic that only aims for fully open or closed
#define ValveMotorDirectV1_DEFINED
template
    <
    uint8_t MOTOR_DRIVE_ML_DigitalPin, uint8_t MOTOR_DRIVE_MR_DigitalPin, uint8_t MOTOR_DRIVE_MI_AIN_DigitalPin, uint8_t MOTOR_DRIVE_MC_AIN_DigitalPin,
    class LowBatt_t = OTV0P2BASE::SupplyVoltageLow, LowBatt_t *lowBattOpt = NULL,
    bool binaryOnly = false
    >
class ValveMotorDirectV1 : public OTRadValve::AbstractRadValve
  {
  private:
    // Driver for the V1/DORM1 hardware.
    ValveMotorDirectV1HardwareDriver<MOTOR_DRIVE_ML_DigitalPin, MOTOR_DRIVE_MR_DigitalPin, MOTOR_DRIVE_MI_AIN_DigitalPin, MOTOR_DRIVE_MC_AIN_DigitalPin> driver;

    // Logic to manage state.
    // A simplified form of the driver is used if binaryOnly is true.
    template <bool Condition, typename TypeTrue, typename TypeFalse>
      class typeIf;
    template <typename TypeTrue, typename TypeFalse>
      struct typeIf<true, TypeTrue, TypeFalse> { typedef TypeTrue t; };
    template <typename TypeTrue, typename TypeFalse>
      struct typeIf<false, TypeTrue, TypeFalse> { typedef TypeFalse t; };
    typename typeIf<binaryOnly, CurrentSenseValveMotorDirectBinaryOnly, CurrentSenseValveMotorDirect>::t logic;

  public:
    ValveMotorDirectV1(bool (*const minimiseActivityOpt)() = ((bool(*)())NULL),
                       uint8_t minOpenPC = OTRadValve::DEFAULT_VALVE_PC_MIN_REALLY_OPEN,
                       uint8_t fairlyOpenPC = OTRadValve::DEFAULT_VALVE_PC_MODERATELY_OPEN)
      : logic(&driver, OTV0P2BASE::getSubCycleTime,
         OTRadValve::CurrentSenseValveMotorDirect::computeMinMotorDRTicks(OTV0P2BASE::SUBCYCLE_TICK_MS_RD),
         OTRadValve::CurrentSenseValveMotorDirect::computeSctAbsLimit(OTV0P2BASE::SUBCYCLE_TICK_MS_RD,
                                                                      OTV0P2BASE::GSCT_MAX,
                                                                      ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks),
        lowBattOpt, minimiseActivityOpt,
        minOpenPC, fairlyOpenPC)
      { }

    // Regular poll/update.
    // This and get() return the actual estimated valve position.
    virtual uint8_t read() override
      {
      logic.poll();
      value = logic.getCurrentPC();
      return(value);
      }

    // Set new target %-open value (if in range).
    // Returns true if the specified value is accepted.
    virtual bool set(const uint8_t newValue) override
      {
      if(newValue > 100) { return(false); }
      logic.setTargetPC(newValue);
      return(true);
      }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    virtual uint8_t getMinPercentOpen() const override { return(logic.getMinPercentOpen()); }

    // Call when given user signal that valve has been fitted (ie is fully on).
    virtual void signalValveFitted() override { logic.signalValveFitted(); }

    // Waiting for indication that the valve head has been fitted to the tail.
    virtual bool isWaitingForValveToBeFitted() const override { return(logic.isWaitingForValveToBeFitted()); }

    // Returns true iff not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    virtual bool isInNormalRunState() const override { return(logic.isInNormalRunState()); }

    // Returns true if in an error state,
    virtual bool isInErrorState() const override { return(logic.isInErrorState()); }

    // Minimally wiggles the motor to give tactile feedback and/or show to be working.
    // May take a significant fraction of a second.
    // Finishes with the motor turned off, and a bias to closing the valve.
    virtual void wiggle() const override { logic.wiggle(); }
  };

#endif // ARDUINO_ARCH_AVR


    }

#endif
