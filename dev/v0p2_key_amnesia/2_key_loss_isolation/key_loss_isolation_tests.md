# Config tests
All code in this report is based on the following tags:

https://github.com/DamonHD/OpenTRV/tree/0160629-KeyLossTesting

https://github.com/opentrv/OTRadioLink/tree/20160629-KeyLossTesting

and is located in:

https://github.com/opentrv/OTRadioLink/blob/20160629-KeyLossTesting/content/OTRadioLink/utility/OTRadioLink_SecureableFrameType_V0p2Impl.cpp

## Aim
To isolate the V0p2_Main config option that leads to key loss.

## Equipment
- REV7 board. **Make sure motor is not connected!!!**
- Benchtop PSU.
- USBTinyISP.
- FTDI cable.

## Tests
### 1. Config 1
#### Results
- Config file used: ./OTV0p2_CONFIG_REV7.h.v1
- On wait (no reset):
    - First Tx: PASS
    - Second Tx: FAIL
- On reset: FAIL

#### Discussion
- This was an attempt at a minimum version...
- Key loss possibly caused by whatever is written in the section just before key (tx message counter)?

### 2. Checking if we reset the IV after writing key.
#### Method
- Put a line to print to serial when the following function is called.
```cpp
bool SimpleSecureFrame32or0BodyTXV0p2::resetRaw3BytePersistentTXRestartCounterInEEPROM(const bool allZeros)
```
- Flash and set key.
- Reset to trigger Tx

#### Discussion
- The key is lost.
- Only thing I could find that calls it is:
```cpp
bool SimpleSecureFrame32or0BodyTXV0p2::incrementAndGetPrimarySecure6BytePersistentTXMessageCounter(uint8_t *const buf)
```


### 3. Narrowing it down
#### Method + results
- Put a line to print to serial when the following function is called.
```cpp
bool SimpleSecureFrame32or0BodyTXV0p2::incrementAndGetPrimarySecure6BytePersistentTXMessageCounter(uint8_t *const buf)
```
- Flash and set key.
- Reset to trigger Tx

#### Discussion
- This function initialises the message counter if it is all 0s.
- When the counter is inited, the key is erased to prevent counter values being reused.
- From OTRadioLink_SecurableFrameType_V0p2Impl.cpp:line 251
``` cpp
    // VITAL FOR CIPHER SECURITY: increase value of restart/reboot counter before first use after (re)boot.
    // Security improvement: if initialising and persistent/restart part is all zeros
    // then force it to an entropy-laden non-zero value that still leaves most of its lifetime.
    // Else simply increment it as per the expected restart counter behaviour.
    // NOTE: AS A MINIMUM the restart counter must be incremented here on initialisation.
    if(doInitialisation)
        {
        if(!get3BytePersistentTXRestartCounter(buf)) { return(false); }
        if((0 == buf[0]) && (0 == buf[1]) && (0 == buf[2]))
            {
            if(!resetRaw3BytePersistentTXRestartCounterInEEPROM(false)) { return(false); } } // The key is erased in this function call.
        else
            { incrementPersistent = true; }
        }
```
- This function is currently (20160629) **only** called during the transmission of a secure frame.
- This means that:
    1. The tx routine gets the key and passes it to the secure frame generation function.
        - The generator calls incrementAndGetPrimarySecure6BytePersistentTXMessageCounter in order to assemble an IV.
        - As the EEPROM was erased during flashing, it initialises the message counter, erasing the key in the process.
    2. As the key has already been copied to the stack at this point, the rest of the transmission is completed successfully.
    3. The next time the tx routine is called, it cannot retrieve the key as it has been erased.

        


