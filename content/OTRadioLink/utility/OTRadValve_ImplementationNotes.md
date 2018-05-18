# OTRadValve Architecture Notes:
DE20170724


## Main Classes
### ModelledRadValve
- Abstract class.
- Radiator valve model. Encapsulates control logic for valve.
- In OTRadValve_AbstractRadValve.h

### ModelledRadValveComputeTargetTempBase:
- Abstract class.
- Classes that inherit from this use environmental data to decide what the target temperature set-point should be.
- E.g.  ModelledRadValveComputeTargetTempBasic uses occupancy data to decide on the target temperature.
- In OTRadValve_ModelledRadValve.h
- Takes and AbstractRadValve type.

### ModelledRadValveState:
- Uses temperature data and target temperature to decide pin position.
- In OTRadValve_ModelledRadValveState.h

### HardwareMotorDriverInterface:
- Abstract class.
- Classes that inherit from this run a physical motor for a set distance or time in a set direction, and take care of low level details such as keeping track of current/encoder sensing and toggling pins.
- E.g. ValveMotorDirectV1HardwareDriver implements TRV1 style motor.
- In OTRadValve_AbstractRadValve.h


## Usage
### Start-Up Routine And Calibration
- Steps:
    1. Check if valve needs to be fitted.
    1. Make sure battery is not low, and room is not dark.
    1. Check that user has signalled that the valve has been fitted (via UI interaction) or that 10 mins have passed.
    1. Signal to ValveMotorDirect that valve is fitted.
    1. ValveMotorDirect will automatically calibrate when ValveMotorDirect::read is called.
``` cpp
// Handle local direct-drive valve, eg DORM1.
// If waiting for for verification that the valve has been fitted
// then accept any manual interaction with controls as that signal.
// Also have a backup timeout of at least ~10m from startup
// for automatic recovery after a crash and restart,
// or where fitter simply forgets to initiate cablibration.
if(ValveDirect.isWaitingForValveToBeFitted()) {
    // Defer automatic recovery when battery low or in dark in case crashing/restarting
    // to try to avoid disturbing/waking occupants and/or entering battery death spiral.  (TODO-1037, TODO-963)
    // The initial minuteCount value can be anywhere in the range [0,3];
    // pick threshold to give user at least a couple of minutes to fit the device
    // if they do so with the battery in place.
    const bool delayRecalibration = batteryLow || AmbLight.isRoomDark();
    if(valveUI.veryRecentUIControlUse() || (minuteCount >= (delayRecalibration ? 240 : 5))) {
        ValveDirect.signalValveFitted();
    }
}
```

### Set New Valve Position
- Normally handled by ModelledRadValve.
    - ModelledRadValveComputeTargetTempBase decides new temp
    - ModelledRadValveState decides valve position
- Steps:
    1. Get & set current temperature with ModelledRadValveInputState::targetTempC.
    1. Get & set set-point temperature with ModelledRadValveInputState::setReferenceTemperatures.
    1. Calculate new valve position with ModelledRadValveState::tick.
    1. Set new valve position with ValveMotorDirect::set.
``` cpp
// Set the new set-point and the current ambient temperature.
radValveInputState.targetTempC = setPointTempC;
radValveInputState.setReferenceTemperatures(currentTempC16);
// Calculate the new valve position and set it in the motor driver.
radValveState.tick(valveOpenPC, radValveInputState, &ValveDirect);
ValveDirect.set(valveOpenPC);
``` 
    1. Move motor by calling ValveMotorDirect::read.
``` cpp
// May interfere with serial comms so test to make sure we are not writing status (!showStatus)
// May take significant time to run so test to make sure we don't overrun loop.
if(!showStatus && (OTV0P2BASE::getSubCycleTime() < ((OTV0P2BASE::GSCT_MAX/4)*3))) {
    ValveDirect.read();
}
```
