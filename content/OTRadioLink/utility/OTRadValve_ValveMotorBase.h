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

#ifndef ARDUINO_LIB_OTRADVALVE_VALVEMOTORBASE_H_
#define ARDUINO_LIB_OTRADVALVE_VALVEMOTORBASE_H_


#include <stdint.h>
#include "OTRadValve_CurrentSenseValveMotorDirect.h"

namespace OTRadValve
{


#ifdef ARDUINO_ARCH_AVR
#define ValveMotorDirectV1HardwareDriverBase_DEFINED
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

#endif // ARDUINO_ARCH_AVR

}

#endif /* ARDUINO_LIB_OTRADVALVE_VALVEMOTORBASE_H_ */
