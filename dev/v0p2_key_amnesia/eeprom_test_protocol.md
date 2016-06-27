# V0p2 Key Amnesia Testing
## Aim
To discover any causes for the V0p2 devices forgetting node IDs and keys programmed in their EEPROM.

The following cases will be tested, both using the V0p2_Main firmware and using a seperate test sketch:
1. Newly programmed using an ISP (EEPROM + flash erased).
2. Newly programmed using the bootloader (??).
3. Programmed with new key after erasing old key.
4. Programmed with new key without erasing old key.

## Equipment
- Device Under Test:
    - REV7s?
    - REV10s?
    - REV11s?
- Test Rig:
    - Raspberry Pi.
    - Serial cables.
    - Relay board with multiple relays for switching devices.

## Test Plans
### 1. Power off time
#### Method
- Set up a test case as outlined above, on multiple devices.
- Program key and check it has been retained.
- Shutdown and wait n seconds.
- Power up and check key has been retained.
- Repeat until failure, with 'n' increasing exponentially.

#### Notes and Assumptions
- Tests the effect of loss of power, e.g. when transporting to home.
- Brownout must be tested seperately.

### 2. Power on time
#### Method
???

#### Notes and Assumptions
- Tests the effect of up-time?
- What is a good way of implementing this?

### 3. Brownout
#### Method
- As with 1. but reducing Vcc below 2.0 V instead of powering off.

#### Notes and Assumptions
- How to change voltage?
- Power cycling may do this anyway as capacitors discharge.
    - REV10 browns out on shutdown (as evidenced by it restarting when power removed.)




