# OTRadValve Architecture Notes:
DE20170724

## AbstractRadValve
- Abstract class.
- Radiator valve model. Encapsulates control logic for valve.
- In OTRadValve_AbstractRadValve.h

## ModelledRadValveComputeTargetTempBase:
- Abstract class.
- Classes that inherit from this use environmental data to decide what the target temperature set-point should be.
- E.g.  ModelledRadValveComputeTargetTempBasic uses occupancy data to decide on the target temperature.
- In OTRadValve_ModelledRadValve.h
- Takes and AbstractRadValve type.

## ModelledRadValveState:
- Uses temperature data and target temperature to decide pin position.
- In OTRadValve_ModelledRadValveState.h

## HardwareMotorDriverInterface:
- Abstract class.
- Classes that inherit from this run a physical motor for a set distance or time in a set direction, and take care of low level details such as keeping track of current/encoder sensing and toggling pins.
- E.g. ValveMotorDirectV1HardwareDriver implements TRV1 style motor.
- In OTRadValve_AbstractRadValve.h
