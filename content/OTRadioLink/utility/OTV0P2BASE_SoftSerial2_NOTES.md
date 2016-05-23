# OTSoftSerial2 Implementation Notes
## Todo:
- [x] Rewrite to use stream interface Arduino libs (arduino-1.6.7/hardware/arduino/avr/cores/arduino/Stream.h)
- [x] Template the class to speed it up.

## Current Problems (20160523):
1. Have to block for long periods of time while waiting for Rx.

## Solutions:
1. Get OTSoftSerialAsync working.

## Assumptions & interface additions:
1. Lowest clock speed is fast enough to accurately sample Rx line.
2. Nothing will block too long between bytes.
3. Added 'sendBreak()' as we need it for the RN2483.
4. rxPin, txPin and speed are passed in as template parameters. begin can still be called as usual but the value passed to it will be ignored.

## Notes
### Rx with multiple bytes:
As the library is not async and we are trying to preserve the Arduino Serial interface, read() must be called from a loop when receiving multiple bytes.
Example function that reads multiple characters into an array (this example is adapted from OTSIM900Link, which expects the function to return the first time it times out.):
```cpp
/**
 * @brief    Enter blocking read. Fills buffer or times out after 100 ms.
 * @param    data: data buffer to write to.
 * @param    length: length of data buffer.
 * @retval   number of characters received before time out.
 */
uint8_t readLotsOfChars(char *data, uint8_t length)
{
    // clear buffer, get time and init i to 0
    uint8_t counter = 0;
    uint8_t len = length;
    char *pdata = data;
    
    // Loop while reading characters.
    while(len--) {
        char c = softSerial2.read();
        if(c == -1) break;  // softSerial2 return -1 on timeout, so we break out of the loop.
        *pdata++ = c;  // only want to write to the array or increment the counter if not timed out.
        counter++;
    }
  return counter;
}
```
