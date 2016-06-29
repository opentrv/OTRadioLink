# Key Loss Reproduction Tests
The EEPROM dumps can be found [here](https://github.com/opentrv/OTRadioLink/tree/master/dev/v0p2_key_amnesia/EEPROM%20memory%20dumps). They are intel hex files but with an additional description at the start.

## Aim
To reliably reproduce key loss issues on REV7 running V0p2_Main v1.0.4

## Test Device and Firmware
- REV7.
- Powered by benchtop PSU set to 3.0 V and current limitted to 500 mA.
- Motor connected.
- Front and back covers, and lid not attached.
- Firmware used was V0p2_Main from tag DORM1-1.0.4-20160315:
    - https://github.com/DamonHD/OpenTRV/tree/DORM1-1.0.4-20160315
    - https://github.com/opentrv/OTRadioLink/tree/DORM1-1.0.4-20160315
    - https://github.com/opentrv/OTAESGCM/tree/DORM1-1.0.4-20160315

## Setup common to all tests
- The firmware was burnt using USBTinyISP.

## Tests
### 1. EEPROM dump after newly burning
#### Purpose
To get a general idea of what happens when the key set and list.

#### Setup
1. The firmware was burnt using USBTinyISP.

#### Method
1. EEPROM dump immediately after burning.
2. Power cycle and dump EEPROM.
3. EEPROM dump after setting key.
4. EEPROM dump after setting key again.
5. EEPROM dump after clearing key.
6. EEPROM dump after setting key.

NOTE: Device powered off after each EEPROM dump.

#### Results
1. Minimal data - resets, ID etc.
2. Minimal data - resets, ID etc.
3. Minimal data - resets, ID etc.
4. Key present.
5. No key.
6. Key present.

NOTE: Tests and results are numbered after the stage they were performed at in the method.

#### Discussion
- 1. & 2. behaved as expected. The EEPROM only contained the reset counter etc. and the node ID.
- 3. displayed faulty behaviour. There was no evidence of the key in the EEPROM. Needs more investigation.
- 4. 5. and 6. were as expected. The key was correctly set and erased.

### 2. EEPROM dump after running a calibration cycle.
**Something has gotten confused here.**
- I think the method might be wrong.
- I either:
    1. Programmed the key in before running the calibration cycle.
    2. Waited for the calibration cycle to end before programming the key (to test whether waiting for the motor calibration to finish has an effect).
- Probably number 2.
- This is not important in light of subsequent tests.

#### Purpose
See comments above.

#### Setup
1. The firmware was burnt using USBTinyISP.

#### Method
1. EEPROM dump immediately after burning.
2. The device was left running long enough to complete a calibration cycle.
3. The device was power cycled multiple times.
4. EEPROM dump after setting key

#### Results
1. Minimal data - resets, ID etc.
2. The first transmission worked but subsequent transmissions failed due to lack of key.

#### Discussion
- The first transmission was received by a REV10, confirming the transmission occured.
- This suggests that the key is set in the EEPROM on the first attempt but is somehow then lost.
- This may have implications for automated testing.
    - Can not rely on first successful Tx indicating a pass as key may still be lost.
    - A second Tx seems to be a reliable indicator of a pass, at least in the short time frame tested.

### 3. Testing initial key retention
#### Purpose
To investigate the key retention for the initial transmission, as demonstrated in test 2.

#### Setup
1. The firmware was burnt using USBTinyISP.

#### Method
1. EEPROM dump immediately after burning.
2. EEPROM dump after calibration cycle
3. EEPROM dump after key set
4. EEPROM dump after reset

Notes: No power cycling! Lots of calibrating.

#### Results
1. Minimal data - resets, ID etc.
2. As 1.
3. The key was present.
4. The key was not present (all FFs)

#### Discussion
- The key is definitely set to EEPROM during the first set.
- For some reason it is not retained for very long.
- The key is always lost to all FFs.
- Key loss is independent of resetting and power cycling.
- Key retention is independent of resetting and power cycling.
- Further tests:
    - Is key retention independent of EEPROM location?
    - Is key retention independent time?
        - Powered on?
        - Powered off?
    - Is key retention independent of brown out detection voltage?
    - Is key retention dependent on EEPROM access to:
        - Location containing key?
        - Other locations?
    - Is key retained after a set-clear-set cycle, with no resets? i.e.:
        - K B xx...
        - K B *
        - K B xx...
        
### 4. Testing key retention while turned off
#### Purpose
To find out whether the key will be retained while the power is off. If it is, this suggests that key loss is caused by the device.

#### Setup
1. The firmware was burnt using USBTinyISP.

#### Method
1. The key was set and power was removed.
2. After a >5 minute wait, power was restored and an EEPROM dump was immediately taken.
3. Was a Tx possible?
4. An EEPROM dump was taken.

#### Results
2. The key was present.
3. First Tx occured. The rest failed as usual.
4. Key lost.

#### Discussion
- 5 minutes was chosen as
    - it would have taken too long otherwise (too many attempts wasted due to unreliable programmer).
    - Normal cycle is 4 minutes.
    - Would still like to test with a longer cycle.
- The results suggest (tentatively) that the key will survive long periods powered down, i.e. that whatever causes key loss happens when device is active.

### 5. Testing key retention without V0p2_Main
#### Purpose
To verify that the OTV0p2Base library functions work correctly in the abscence of the V0p2_Main control loop.

#### Method
1. The device was programmed with using the code at:
    - Uses OTV0p2Base library functions. These are the same as used in V0p2_Main.
    - Can set/erase/check/read keys over CLI.
2. EEPROM dump was taken.
3. Key was set and read back.
4. Device was power cycled and key read back.
5. EEPROM dump was taken.

#### Results
2. Empty EEPROM.
5. Key present.

#### Discussion
- The results suggest that the OTV0p2Base security and EEPROM libraries work as they should.


test
K B 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee e1
