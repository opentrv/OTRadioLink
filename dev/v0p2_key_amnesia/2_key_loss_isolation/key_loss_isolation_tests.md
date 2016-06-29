# Config tests
## Aim
To isolate the V0p2_Main config option that leads to key loss.

## Equipment
- REV7 board. **Make sure motor is not connected!!!**
- Benchtop PSU.
- USBTinyISP.
- FTDI cable.

## Tests
### 1. Config 1
#### 
- Config file used: ./OTV0p2_CONFIG_REV7.h.v1
- On wait (no reset):
    - First Tx: PASS
    - Second Tx: FAIL
- On reset: FAIL

#### Discussion
- This was an attempt at a minimum version...
- Key loss possibly caused by whatever is written just before key?

### 2. Checking if we reset the IV after writing key.

#### Discussion
- Resetting the IV will clear the key!





K B 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee e1
