/*
 * OTRadValve_ValveMotorBase.h
 *
 *  Created on: 11 Oct 2016
 *      Author: denzo
 */

#ifndef CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_VALVEMOTORBASE_H_
#define CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_VALVEMOTORBASE_H_


#include <stdint.h>
#include "OTRadValve_AbstractRadValve.h"

namespace OTRadValve
{


class ValveMotorDirectV1HardwareDriverBase : public OTRadValve::HardwareMotorDriverInterface
  {
  public:
    // Approx minimum time to let H-bridge settle/stabilise (ms).
    static const uint8_t minMotorHBridgeSettleMS = 8;
    // Min sub-cycle ticks for H-bridge to settle.
    static const uint8_t minMotorHBridgeSettleTicks = max(1, minMotorHBridgeSettleMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD);

    // Approx minimum runtime to get motor up to speed (from stopped) and not give false high-current readings (ms).
    // Based on DHD20151019 DORM1 prototype rig-up and NiMH battery; 32ms+ seems good.
    static const uint8_t minMotorRunupMS = 32;
    // Min sub-cycle ticks to run up.
    static const uint8_t minMotorRunupTicks = max(1, minMotorRunupMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD);

  private:
    // Maximum current reading allowed when closing the valve (against the spring).
    static const uint16_t maxCurrentReadingClosing = 600;
    // Maximum current reading allowed when opening the valve (retracting the pin, no resisting force).
    // Keep this as low as possible to reduce the chance of skipping the end-stop and game over...
    // DHD20151229: at 500 Shenzhen sample unit without outer case (so with more flex) was able to drive past end-stop.
    static const uint16_t maxCurrentReadingOpening = 450; // DHD20151023: 400 seemed marginal.

  protected:
    // Spin for up to the specified number of SCT ticks, monitoring current and position encoding.
    //   * maxRunTicks  maximum sub-cycle ticks to attempt to run/spin for); strictly positive
    //   * minTicksBeforeAbort  minimum ticks before abort for end-stop / high-current,
    //       don't attempt to run at all if less than this time available before (close to) end of sub-cycle;
    //       should be no greater than maxRunTicks
    //   * dir  direction to run motor (open or closed) or off if waiting for motor to stop
    //   * callback  handler to deliver end-stop and position-encoder callbacks to;
    //     non-null and callbacks must return very quickly
    // If too few ticks remain before the end of the sub-cycle for the minimum run,
    // then this will return true immediately.
    // Invokes callbacks for high current (end stop) and position (shaft) encoder where applicable.
    // Aborts early if high current is detected at the start,
    // or after the minimum run period.
    // Returns true if aborted early from too little time to start, or by high current (assumed end-stop hit).
    bool spinSCTTicks(uint8_t maxRunTicks, uint8_t minTicksBeforeAbort, OTRadValve::HardwareMotorDriverInterface::motor_drive dir, OTRadValve::HardwareMotorDriverInterfaceCallbackHandler &callback);
  };

// Note: internal resistance of fresh AA alkaline cell may be ~0.2 ohm at room temp:
//    http://data.energizer.com/PDFs/BatteryIR.pdf
// NiMH may be nearer 0.025 ohm.
// Typical motor impedance expected here ~5 ohm, with supply voltage 2--3V.


// Time before starting to retract pint during initialisation, in seconds.
// Long enough for to leave the CLI some time for setting things like setting secret keys.
// Short enough not to be annoying waiting for the pin to retract before fitting a valve.
//static const uint8_t initialRetractDelay_s = 30;
const constexpr uint8_t initialRetractDelay_s = 30;

// Runtime for dead-reckoning adjustments (from stopped) (ms).
// Smaller values nominally allow greater precision when dead-reckoning,
// but may force the calibration to take longer.
// Based on DHD20151020 DORM1 prototype rig-up and NiMH battery; 250ms+ seems good.
static const constexpr uint8_t minMotorDRMS = 250;
// Min sub-cycle ticks for dead reckoning.
static const constexpr uint8_t minMotorDRTicks = max(1, (uint8_t)(minMotorDRMS / OTV0P2BASE::SUBCYCLE_TICK_MS_RD));

// Absolute limit in sub-cycle beyond which motor should not be started.
// This should allow meaningful movement and stop and settle and no sub-cycle overrun.
// Allows for up to 120ms enforced sleep either side of motor run for example.
// This should not be so greedy as to (eg) make the CLI unusable: 90% is pushing it.
static const constexpr uint8_t sctAbsLimit = OTV0P2BASE::GSCT_MAX - max(1, ((OTV0P2BASE::GSCT_MAX+1)/4)) - OTRadValve::ValveMotorDirectV1HardwareDriverBase::minMotorRunupTicks - 1 - (uint8_t)(240 / OTV0P2BASE::SUBCYCLE_TICK_MS_RD);

// Absolute limit in sub-cycle beyond which motor should not be started for dead-reckoning pulse.
// This should allow meaningful movement and no sub-cycle overrun.
//static const uint8_t sctAbsLimitDR = sctAbsLimit - minMotorDRTicks;
const constexpr uint8_t sctAbsLimitDR = sctAbsLimit - minMotorDRTicks;


}

#endif /* CONTENT_OTRADIOLINK_UTILITY_OTRADVALVE_VALVEMOTORBASE_H_ */
