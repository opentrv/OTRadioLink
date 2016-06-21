# REV7 Battery Life Testing
## Aim
To estimate the run time of the TRV1 model radiator valve before failure due to low battery.
The following cases will be tested, with hybrid NiMH and alkaline batteries:

1. Constant motor drive until failure.
2. The power consumption of the MCU over an extended period under 'normal use' conditions.
3. The power consumption of sending a packet by radio over an extended period.
4. The power consumption of encrypting a packet with 128-bit AESGCM and sending it by radio over an extended period.

## Equipment
- Device under test:
    - REV7 board.
    - Production ice-cream cones.
    - board version?
- Battery:
    - 2x Hybrid NiMH (capacity?)
    - 2x Alkaline (capacity?)
- How many devices do we test against?
- How many times do we test each device?
- Test Rig:
    - Raspberry Pi.
    - Serial cables.
    - Current measurement device (either directly attached or via arduino).

## Test Plans
### 1. Motor
#### Method
- Tested unattached to radiator.
- Run from end stop to end stop until failure (found by polling for stall current) and increment a counter.
- Print counter to serial each time motor reaches end stop.
- Serial logger on computer logs counter.
- The highest value captured by the logger that is still sequential is the number of cycles, preventing errors due to CPU brownout etc.

#### Notes and Assumptions
- Losses due to running the REV7 (e.g. MCU, radio, serial etc.) are small (~1%) compared to motor current and can be ignored.
- Motor current is only dependent on force required to turn the screw (e.g. independent of position of moving screw).
- Not sure how well this will reflect the real case as:
    - Valve base has much smaller travel.
    - Valve base will have higher resistance, causing higher currents.
    - Motor should not stall (as often) in real use.

#### Results

#### Future tests
- Test attached to valve-base.
- Test attached to a radiator valve.


### 2. MCU
#### Method

#### Notes and Assumptions
- Not using radio or motor.
- There are two states:
    - On:    MCU powered and running. Current consumption dominated by MCU and other sources can be ignored.
    - Sleep: MCU is in low power mode. Leakage and peripheral consumption is significant.
- Not practical to test until failure in a reasonable time frame. Will need to measure average current consumption and extrapolate.
- Need to estimate 'sleep' vs 'on' time.
- Need to define 'normal conditions'
- Need to turn off RFM23B or put in lowest power state.

#### Results

#### Future tests


### 3. Radio
#### Method
- Send a fixed number of pre-assembled frame as fast as possible (without breaking ETSI regs).
- Log the current consumption and work out average consumption per frame.

#### Notes and Assumptions
- Not using motor.
- MCU consumption (from controlling the radio, putting together packets etc.) is a confounding variable, but can be lumped with radio consumption or factored in from MCU current consumption test.
- Not practical to test until failure in a reasonable time frame. Will need to measure average current consumption and extrapolate.

#### Results

#### Future tests


### 4. Encryption + Radio
#### Method
- Encrypt and send a fixed number of frames as fast as possible.
- Log current consumption and work out average consumption per frame.
#### Notes and Assumptions
- Not using motor.
- MCU time spent not encrypting packets is insignifcant. MCU current consumption due to other tasks can be ignored.
- Not practical to test until failure in a reasonable time frame. Will need to measure average current consumption and extrapolate.
- Could potentially just measure encryption consumption and combine that with packet send consumption.

#### Results

#### Future tests


